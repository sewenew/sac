from __future__ import annotations

import logging
from typing import Any

try:
    from .message_sender import LarkMessageSender
    from .pairing import AuthResult, build_authorizer_from_env
except ImportError:
    from message_sender import LarkMessageSender
    from pairing import AuthResult, build_authorizer_from_env


logger = logging.getLogger(__name__)
authorizer = build_authorizer_from_env()
message_sender: LarkMessageSender | None = None


def configure_message_sender(sender: LarkMessageSender | None) -> None:
    global message_sender
    message_sender = sender


def handle_event(payload: dict[str, Any]) -> AuthResult | None:
    """Handle a normalized Feishu event locally.

    Replace or extend this function when the bot needs in-process business
    logic. HTTP forwarding is configured separately with LARK_FORWARD_URL.
    """
    auth = authorizer.authorize_payload(payload)
    if not auth.authorized:
        if auth.pairing_code:
            logger.warning(
                "unauthorized Lark user: open_id=%s tenant_key=%s "
                "pairing_code=%s",
                auth.open_id,
                auth.tenant_key,
                auth.pairing_code,
            )
            if message_sender is not None:
                message_sender.send_pairing_code(payload, auth.pairing_code)
        else:
            logger.warning(
                "rejected Lark event: reason=%s event_id=%s",
                auth.reason,
                payload.get("event_id"),
            )
        return None

    logger.info(
        "received Lark event: type=%s event_id=%s user_id=%s roles=%s",
        payload.get("event_type"),
        payload.get("event_id"),
        auth.user_id,
        ",".join(auth.roles),
    )
    return auth
