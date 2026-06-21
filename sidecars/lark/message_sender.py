from __future__ import annotations

import json
import logging
from typing import Any

import lark_oapi as lark


logger = logging.getLogger(__name__)


class LarkMessageSender:
    def __init__(self, client: lark.Client) -> None:
        self._client = client

    def send_text(self, chat_id: str, text: str) -> bool:
        request = (
            lark.im.v1.CreateMessageRequest.builder()
            .receive_id_type("chat_id")
            .request_body(
                lark.im.v1.CreateMessageRequestBody.builder()
                .receive_id(chat_id)
                .msg_type("text")
                .content(json.dumps({"text": text}, ensure_ascii=False))
                .build()
            )
            .build()
        )
        response = self._client.im.v1.message.create(request)
        if response.success():
            return True

        logger.error(
            "failed to send Lark message: chat_id=%s code=%s msg=%s log_id=%s",
            chat_id,
            response.code,
            response.msg,
            response.get_log_id(),
        )
        return False

    def send_pairing_code(self, payload: dict[str, Any], code: str) -> bool:
        chat_id = extract_chat_id(payload)
        if not chat_id:
            logger.warning(
                "cannot send pairing code because chat_id is missing: "
                "event_id=%s",
                payload.get("event_id"),
            )
            return False

        return self.send_text(
            chat_id,
            "你的配对码是："
            f"{code}\n"
            "请联系管理员完成绑定。配对码会在几分钟后过期。",
        )


def extract_chat_id(payload: dict[str, Any]) -> str | None:
    data = payload.get("data") or {}
    event = data.get("event") or {}
    message = event.get("message") or {}
    chat_id = message.get("chat_id")
    return str(chat_id) if chat_id else None
