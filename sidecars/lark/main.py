from __future__ import annotations

import json
import logging
import os
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any

import lark_oapi as lark

try:
    from .agent_request import build_agent_request
    from .forwarder import ForwardError, HttpForwarder
    from .handler import configure_message_sender, handle_event
    from .message_sender import LarkMessageSender
except ImportError:
    from agent_request import build_agent_request
    from forwarder import ForwardError, HttpForwarder
    from handler import configure_message_sender, handle_event
    from message_sender import LarkMessageSender


logger = logging.getLogger(__name__)


@dataclass(frozen=True)
class Config:
    app_id: str
    app_secret: str
    verification_token: str = ""
    encrypt_key: str = ""
    forward_url: str | None = None
    forward_token: str | None = None
    forward_timeout_seconds: float = 5.0
    log_level: str = "INFO"

    @classmethod
    def from_env(cls) -> "Config":
        app_id = os.getenv("LARK_APP_ID", "").strip()
        app_secret = os.getenv("LARK_APP_SECRET", "").strip()
        if not app_id or not app_secret:
            raise ValueError(
                "LARK_APP_ID and LARK_APP_SECRET must be configured"
            )

        return cls(
            app_id=app_id,
            app_secret=app_secret,
            verification_token=os.getenv(
                "LARK_VERIFICATION_TOKEN", ""
            ).strip(),
            encrypt_key=os.getenv("LARK_ENCRYPT_KEY", "").strip(),
            forward_url=os.getenv("LARK_FORWARD_URL") or None,
            forward_token=os.getenv("LARK_FORWARD_TOKEN") or None,
            forward_timeout_seconds=float(
                os.getenv("LARK_FORWARD_TIMEOUT_SECONDS", "5")
            ),
            log_level=os.getenv("LOG_LEVEL", "INFO").upper(),
        )


def _to_dict(data: Any) -> dict[str, Any]:
    serialized = lark.JSON.marshal(data)
    result = json.loads(serialized)
    if not isinstance(result, dict):
        raise TypeError("Feishu event payload must be a JSON object")
    return result


def normalize_message_event(
    data: lark.im.v1.P2ImMessageReceiveV1,
) -> dict[str, Any]:
    raw_event = _to_dict(data)
    header = raw_event.get("header") or {}
    return {
        "source": "lark",
        "event_type": header.get(
            "event_type", "im.message.receive_v1"
        ),
        "event_id": header.get("event_id"),
        "tenant_key": header.get("tenant_key"),
        "received_at": datetime.now(timezone.utc).isoformat(),
        "data": raw_event,
    }


def build_event_handler(
    config: Config,
    forwarder: HttpForwarder | None = None,
) -> lark.EventDispatcherHandler:
    def on_message(data: lark.im.v1.P2ImMessageReceiveV1) -> None:
        payload = normalize_message_event(data)

        try:
            auth = handle_event(payload)
            if auth is None:
                return
        except Exception:
            logger.exception("local Feishu event handler failed")
            return

        if forwarder is None:
            return

        try:
            agent_request = build_agent_request(payload, auth)
            forwarder.forward(agent_request)
            logger.info(
                "forwarded agent request: type=%s event_id=%s",
                payload["event_type"],
                payload["event_id"],
            )
        except ForwardError:
            logger.exception(
                "failed to forward Feishu event: event_id=%s",
                payload["event_id"],
            )

    return (
        lark.EventDispatcherHandler.builder(
            config.verification_token,
            config.encrypt_key,
        )
        .register_p2_im_message_receive_v1(on_message)
        .build()
    )


def _sdk_log_level(log_level: str) -> lark.LogLevel:
    return getattr(lark.LogLevel, log_level, lark.LogLevel.INFO)


def main() -> None:
    config = Config.from_env()
    logging.basicConfig(
        level=getattr(logging, config.log_level, logging.INFO),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )

    forwarder = None
    if config.forward_url:
        forwarder = HttpForwarder(
            url=config.forward_url,
            timeout_seconds=config.forward_timeout_seconds,
            bearer_token=config.forward_token,
        )

    event_handler = build_event_handler(config, forwarder)
    openapi_client = (
        lark.Client.builder()
        .app_id(config.app_id)
        .app_secret(config.app_secret)
        .log_level(_sdk_log_level(config.log_level))
        .build()
    )
    configure_message_sender(LarkMessageSender(openapi_client))

    client = lark.ws.Client(
        config.app_id,
        config.app_secret,
        event_handler=event_handler,
        log_level=_sdk_log_level(config.log_level),
    )

    logger.info(
        "starting Feishu WebSocket client%s",
        f", forwarding to {config.forward_url}"
        if config.forward_url
        else "",
    )
    client.start()


if __name__ == "__main__":
    main()
