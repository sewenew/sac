from __future__ import annotations

import json
import unittest

from message_sender import LarkMessageSender, extract_chat_id


class _Response:
    code = 0
    msg = "ok"

    def success(self) -> bool:
        return True

    def get_log_id(self):
        return "log-id"


class _Message:
    def __init__(self) -> None:
        self.request = None

    def create(self, request):
        self.request = request
        return _Response()


class _Client:
    def __init__(self) -> None:
        self.message = _Message()
        self.im = type("Im", (), {})()
        self.im.v1 = type("V1", (), {})()
        self.im.v1.message = self.message


def _payload():
    return {
        "event_id": "evt_1",
        "data": {
            "event": {
                "message": {
                    "chat_id": "oc_1",
                }
            }
        },
    }


class LarkMessageSenderTest(unittest.TestCase):
    def test_extract_chat_id(self) -> None:
        self.assertEqual(extract_chat_id(_payload()), "oc_1")

    def test_send_pairing_code_posts_text_message(self) -> None:
        client = _Client()
        sender = LarkMessageSender(client)

        self.assertTrue(sender.send_pairing_code(_payload(), "AB12CD34"))

        request = client.message.request
        self.assertEqual(request.receive_id_type, "chat_id")
        self.assertEqual(request.request_body.receive_id, "oc_1")
        self.assertEqual(request.request_body.msg_type, "text")
        content = json.loads(request.request_body.content)
        self.assertIn("AB12CD34", content["text"])


if __name__ == "__main__":
    unittest.main()
