from __future__ import annotations

import argparse
import json
import logging
import os
import secrets
import string
import tempfile
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any


logger = logging.getLogger(__name__)


DEFAULT_PERMISSION_FILE = "lark_permissions.json"
DEFAULT_PAIRING_TTL_SECONDS = 600
DEFAULT_RELOAD_INTERVAL_SECONDS = 30


@dataclass(frozen=True)
class AuthResult:
    authorized: bool
    open_id: str | None
    tenant_key: str | None
    user_id: str | None = None
    roles: tuple[str, ...] = ()
    pairing_code: str | None = None
    reason: str = ""


class PairingAuthorizer:
    def __init__(
        self,
        permission_file: str | os.PathLike[str] = DEFAULT_PERMISSION_FILE,
        reload_interval_seconds: int = DEFAULT_RELOAD_INTERVAL_SECONDS,
        pairing_ttl_seconds: int = DEFAULT_PAIRING_TTL_SECONDS,
    ) -> None:
        self._path = Path(permission_file)
        self._reload_interval_seconds = reload_interval_seconds
        self._pairing_ttl_seconds = pairing_ttl_seconds
        self._lock = threading.RLock()
        self._config: dict[str, Any] = self._empty_config()
        self._mtime_ns: int | None = None
        self._next_reload_at = 0.0

    def authorize_payload(self, payload: dict[str, Any]) -> AuthResult:
        open_id, tenant_key = extract_lark_identity(payload)
        if not open_id:
            return AuthResult(
                authorized=False,
                open_id=None,
                tenant_key=tenant_key,
                reason="missing_open_id",
            )

        with self._lock:
            self._reload_if_needed()
            self._drop_expired_pairings_locked()

            user = self._find_user_locked(open_id, tenant_key)
            if user is not None:
                return AuthResult(
                    authorized=True,
                    open_id=open_id,
                    tenant_key=tenant_key,
                    user_id=str(user["user_id"]),
                    roles=tuple(user.get("roles") or ()),
                    reason="authorized",
                )

            code = self._ensure_pending_pairing_locked(open_id, tenant_key)
            self._save_locked()
            return AuthResult(
                authorized=False,
                open_id=open_id,
                tenant_key=tenant_key,
                pairing_code=code,
                reason="pending_pairing",
            )

    def approve_pairing(
        self,
        code: str,
        user_id: str,
        roles: list[str] | None = None,
    ) -> AuthResult:
        normalized_code = code.strip().upper()
        if not normalized_code:
            raise ValueError("pairing code is required")
        if not user_id.strip():
            raise ValueError("user_id is required")

        with self._lock:
            self._reload(force=True)
            self._drop_expired_pairings_locked()

            pending = self._pop_pending_pairing_locked(normalized_code)
            if pending is None:
                return AuthResult(
                    authorized=False,
                    open_id=None,
                    tenant_key=None,
                    pairing_code=normalized_code,
                    reason="pairing_code_not_found",
                )

            role_values = _normalize_roles(roles or ["user"])
            self._upsert_user_locked(
                user_id=user_id.strip(),
                open_id=str(pending["open_id"]),
                tenant_key=pending.get("tenant_key"),
                roles=role_values,
            )
            self._save_locked()
            return AuthResult(
                authorized=True,
                open_id=str(pending["open_id"]),
                tenant_key=pending.get("tenant_key"),
                user_id=user_id.strip(),
                roles=tuple(role_values),
                pairing_code=normalized_code,
                reason="paired",
            )

    def reload(self) -> None:
        with self._lock:
            self._reload(force=True)

    def _reload_if_needed(self) -> None:
        now = time.monotonic()
        if now < self._next_reload_at:
            return
        self._next_reload_at = now + self._reload_interval_seconds
        self._reload(force=False)

    def _reload(self, force: bool) -> None:
        if not self._path.exists():
            self._config = self._empty_config()
            self._mtime_ns = None
            return

        stat = self._path.stat()
        if not force and self._mtime_ns == stat.st_mtime_ns:
            return

        with self._path.open("r", encoding="utf-8") as file:
            loaded = json.load(file)
        self._config = self._normalize_config(loaded)
        self._mtime_ns = stat.st_mtime_ns
        logger.info("loaded Lark permission file: %s", self._path)

    def _save_locked(self) -> None:
        self._config = self._normalize_config(self._config)
        self._path.parent.mkdir(parents=True, exist_ok=True)

        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=self._path.parent,
            prefix=f".{self._path.name}.",
            suffix=".tmp",
            delete=False,
        ) as file:
            json.dump(self._config, file, ensure_ascii=False, indent=2)
            file.write("\n")
            temp_name = file.name

        os.replace(temp_name, self._path)
        self._mtime_ns = self._path.stat().st_mtime_ns
        self._next_reload_at = time.monotonic() + self._reload_interval_seconds

    def _find_user_locked(
        self,
        open_id: str,
        tenant_key: str | None,
    ) -> dict[str, Any] | None:
        for user in self._config["users"]:
            if not user.get("enabled", True):
                continue
            if user.get("open_id") != open_id:
                continue
            configured_tenant = user.get("tenant_key")
            if configured_tenant and tenant_key != configured_tenant:
                continue
            return user
        return None

    def _ensure_pending_pairing_locked(
        self,
        open_id: str,
        tenant_key: str | None,
    ) -> str:
        now = _utc_now()
        expires_at = now + timedelta(seconds=self._pairing_ttl_seconds)
        for pending in self._config["pending_pairings"]:
            if pending.get("open_id") != open_id:
                continue
            if pending.get("tenant_key") != tenant_key:
                continue
            if _parse_time(str(pending["expires_at"])) <= now:
                continue
            pending["last_seen_at"] = _format_time(now)
            return str(pending["code"])

        code = self._new_pairing_code_locked()
        self._config["pending_pairings"].append(
            {
                "code": code,
                "open_id": open_id,
                "tenant_key": tenant_key,
                "created_at": _format_time(now),
                "last_seen_at": _format_time(now),
                "expires_at": _format_time(expires_at),
            }
        )
        return code

    def _new_pairing_code_locked(self) -> str:
        alphabet = string.ascii_uppercase + string.digits
        existing = {
            str(pending.get("code", "")).upper()
            for pending in self._config["pending_pairings"]
        }
        while True:
            code = "".join(secrets.choice(alphabet) for _ in range(8))
            if code not in existing:
                return code

    def _pop_pending_pairing_locked(
        self,
        code: str,
    ) -> dict[str, Any] | None:
        now = _utc_now()
        kept = []
        matched = None
        for pending in self._config["pending_pairings"]:
            if _parse_time(str(pending["expires_at"])) <= now:
                continue
            if str(pending.get("code", "")).upper() == code:
                matched = pending
                continue
            kept.append(pending)
        self._config["pending_pairings"] = kept
        return matched

    def _upsert_user_locked(
        self,
        user_id: str,
        open_id: str,
        tenant_key: str | None,
        roles: list[str],
    ) -> None:
        now = _format_time(_utc_now())
        for user in self._config["users"]:
            if user.get("open_id") == open_id:
                user.update(
                    {
                        "user_id": user_id,
                        "tenant_key": tenant_key,
                        "roles": roles,
                        "enabled": True,
                        "updated_at": now,
                    }
                )
                return

        self._config["users"].append(
            {
                "user_id": user_id,
                "open_id": open_id,
                "tenant_key": tenant_key,
                "roles": roles,
                "enabled": True,
                "created_at": now,
            }
        )

    def _drop_expired_pairings_locked(self) -> None:
        now = _utc_now()
        self._config["pending_pairings"] = [
            pending
            for pending in self._config["pending_pairings"]
            if _parse_time(str(pending["expires_at"])) > now
        ]

    def _normalize_config(self, loaded: Any) -> dict[str, Any]:
        if not isinstance(loaded, dict):
            raise ValueError("permission file must contain a JSON object")
        users = loaded.get("users") or []
        pending_pairings = loaded.get("pending_pairings") or []
        if not isinstance(users, list):
            raise ValueError("permission file field users must be a list")
        if not isinstance(pending_pairings, list):
            raise ValueError(
                "permission file field pending_pairings must be a list"
            )

        return {
            "version": int(loaded.get("version", 1)),
            "users": [self._normalize_user(user) for user in users],
            "pending_pairings": [
                self._normalize_pending(pending)
                for pending in pending_pairings
            ],
        }

    def _normalize_user(self, user: Any) -> dict[str, Any]:
        if not isinstance(user, dict):
            raise ValueError("each user must be a JSON object")
        if not user.get("user_id") or not user.get("open_id"):
            raise ValueError("each user must contain user_id and open_id")
        return {
            "user_id": str(user["user_id"]),
            "open_id": str(user["open_id"]),
            "tenant_key": user.get("tenant_key"),
            "roles": _normalize_roles(user.get("roles") or ["user"]),
            "enabled": bool(user.get("enabled", True)),
            **{
                key: user[key]
                for key in ("created_at", "updated_at")
                if key in user
            },
        }

    def _normalize_pending(self, pending: Any) -> dict[str, Any]:
        if not isinstance(pending, dict):
            raise ValueError("each pending pairing must be a JSON object")
        for key in ("code", "open_id", "expires_at"):
            if not pending.get(key):
                raise ValueError(f"pending pairing must contain {key}")
        return {
            "code": str(pending["code"]).upper(),
            "open_id": str(pending["open_id"]),
            "tenant_key": pending.get("tenant_key"),
            "created_at": pending.get("created_at"),
            "last_seen_at": pending.get("last_seen_at"),
            "expires_at": pending["expires_at"],
        }

    @staticmethod
    def _empty_config() -> dict[str, Any]:
        return {
            "version": 1,
            "users": [],
            "pending_pairings": [],
        }


def extract_lark_identity(
    payload: dict[str, Any],
) -> tuple[str | None, str | None]:
    data = payload.get("data") or {}
    header = data.get("header") or {}
    event = data.get("event") or {}
    sender = event.get("sender") or {}
    sender_id = sender.get("sender_id") or {}
    return sender_id.get("open_id"), header.get("tenant_key")


def build_authorizer_from_env() -> PairingAuthorizer:
    return PairingAuthorizer(
        permission_file=os.getenv(
            "LARK_PERMISSION_FILE",
            DEFAULT_PERMISSION_FILE,
        ),
        reload_interval_seconds=int(
            os.getenv(
                "LARK_PERMISSION_RELOAD_SECONDS",
                str(DEFAULT_RELOAD_INTERVAL_SECONDS),
            )
        ),
        pairing_ttl_seconds=int(
            os.getenv(
                "LARK_PAIRING_TTL_SECONDS",
                str(DEFAULT_PAIRING_TTL_SECONDS),
            )
        ),
    )


def _normalize_roles(roles: list[str]) -> list[str]:
    normalized = []
    for role in roles:
        value = str(role).strip()
        if value and value not in normalized:
            normalized.append(value)
    return normalized or ["user"]


def _utc_now() -> datetime:
    return datetime.now(timezone.utc)


def _format_time(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat()


def _parse_time(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def _approve_from_cli(args: argparse.Namespace) -> int:
    authorizer = PairingAuthorizer(args.file)
    roles = [role.strip() for role in args.roles.split(",")]
    result = authorizer.approve_pairing(args.code, args.user_id, roles)
    if not result.authorized:
        print(f"failed: {result.reason}")
        return 1

    print(
        "paired "
        f"code={result.pairing_code} "
        f"user_id={result.user_id} "
        f"open_id={result.open_id} "
        f"roles={','.join(result.roles)}"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Manage local Lark open_id pairing records.",
    )
    parser.add_argument(
        "--file",
        default=DEFAULT_PERMISSION_FILE,
        help="permission JSON file path",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    approve = subparsers.add_parser("approve", help="approve a pairing code")
    approve.add_argument("code")
    approve.add_argument("user_id")
    approve.add_argument("--roles", default="user")
    approve.set_defaults(func=_approve_from_cli)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
