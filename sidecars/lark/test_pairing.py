from __future__ import annotations

import json
import tempfile
import time
import unittest
from pathlib import Path

from pairing import PairingAuthorizer, extract_lark_identity


def _payload(open_id: str = "ou_1", tenant_key: str = "tenant_1"):
    return {
        "event_id": "evt_1",
        "data": {
            "header": {
                "tenant_key": tenant_key,
            },
            "event": {
                "sender": {
                    "sender_id": {
                        "open_id": open_id,
                    }
                }
            },
        },
    }


class PairingAuthorizerTest(unittest.TestCase):
    def test_unknown_user_creates_pending_pairing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "permissions.json"
            authorizer = PairingAuthorizer(path)

            result = authorizer.authorize_payload(_payload())

            self.assertFalse(result.authorized)
            self.assertEqual(result.reason, "pending_pairing")
            self.assertIsNotNone(result.pairing_code)

            config = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(
                config["pending_pairings"][0]["code"],
                result.pairing_code,
            )
            self.assertEqual(
                config["pending_pairings"][0]["open_id"],
                "ou_1",
            )

    def test_approve_pairing_authorizes_user(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "permissions.json"
            authorizer = PairingAuthorizer(path)
            pending = authorizer.authorize_payload(_payload())

            approved = authorizer.approve_pairing(
                pending.pairing_code or "",
                "alice",
                ["admin", "operator"],
            )
            authorized = authorizer.authorize_payload(_payload())

            self.assertTrue(approved.authorized)
            self.assertTrue(authorized.authorized)
            self.assertEqual(authorized.user_id, "alice")
            self.assertEqual(authorized.roles, ("admin", "operator"))

    def test_external_file_change_reloads_after_interval(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "permissions.json"
            path.write_text(
                json.dumps(
                    {
                        "version": 1,
                        "users": [],
                        "pending_pairings": [],
                    }
                ),
                encoding="utf-8",
            )
            authorizer = PairingAuthorizer(
                path,
                reload_interval_seconds=1,
            )
            self.assertFalse(authorizer.authorize_payload(_payload()).authorized)

            path.write_text(
                json.dumps(
                    {
                        "version": 1,
                        "users": [
                            {
                                "user_id": "bob",
                                "open_id": "ou_1",
                                "tenant_key": "tenant_1",
                                "roles": ["user"],
                                "enabled": True,
                            }
                        ],
                        "pending_pairings": [],
                    }
                ),
                encoding="utf-8",
            )
            time.sleep(1.1)

            result = authorizer.authorize_payload(_payload())

            self.assertTrue(result.authorized)
            self.assertEqual(result.user_id, "bob")

    def test_extract_lark_identity(self) -> None:
        self.assertEqual(
            extract_lark_identity(_payload("ou_2", "tenant_2")),
            ("ou_2", "tenant_2"),
        )


if __name__ == "__main__":
    unittest.main()
