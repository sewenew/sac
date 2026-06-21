from __future__ import annotations

import json
from typing import Any

try:
    from .pairing import AuthResult
except ImportError:
    from pairing import AuthResult


def build_agent_request(
    payload: dict[str, Any],
    auth: AuthResult,
) -> dict[str, Any]:
    data = payload.get("data") or {}
    header = data.get("header") or {}
    event = data.get("event") or {}
    message = event.get("message") or {}
    sender = event.get("sender") or {}
    sender_id = sender.get("sender_id") or {}

    message_id = _required_string(message, "message_id")
    chat_id = _required_string(message, "chat_id")
    text = _extract_text(message)

    tenant_key = header.get("tenant_key") or auth.tenant_key or ""
    open_id = sender_id.get("open_id") or auth.open_id or ""
    request_id = payload.get("event_id") or message_id

    return {
        "version": "1",
        "request_id": f"lark:{request_id}",
        "source": {
            "platform": "lark",
            "tenant_id": tenant_key,
            "user_id": open_id,
            "chat_id": chat_id,
            "message_id": message_id,
        },
        "message": {
            "type": "text",
            "text": text,
            "raw": message,
        },
        "auth": {
            "system_user_id": auth.user_id,
            "roles": list(auth.roles),
        },
        "reply": {
            "platform": "lark",
            "target_type": "chat",
            "target_id": chat_id,
            "tenant_id": tenant_key,
            "user_id": open_id,
        },
        "metadata": {
            "received_at": payload.get("received_at"),
            "sidecar_source": "sidecars/lark",
        },
    }


def _extract_text(message: dict[str, Any]) -> str:
    if message.get("message_type") != "text":
        raise ValueError("only text Lark messages can be forwarded")

    content = message.get("content")
    if not isinstance(content, str) or not content:
        raise ValueError("Lark text message content is missing")

    parsed = json.loads(content)
    text = parsed.get("text")
    if not isinstance(text, str) or not text.strip():
        raise ValueError("Lark text message content.text is missing")
    return text.strip()


def _required_string(data: dict[str, Any], key: str) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value:
        raise ValueError(f"Lark message missing {key}")
    return value
