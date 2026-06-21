from __future__ import annotations

import json
import unittest

from agent_request import build_agent_request
from pairing import AuthResult


class AgentRequestTest(unittest.TestCase):
    def test_build_agent_request_from_lark_payload(self) -> None:
        payload = {
            "event_id": "evt_1",
            "received_at": "2026-06-21T00:00:00+00:00",
            "data": {
                "header": {
                    "tenant_key": "tenant_1",
                },
                "event": {
                    "sender": {
                        "sender_id": {
                            "open_id": "ou_1",
                        }
                    },
                    "message": {
                        "message_id": "om_1",
                        "chat_id": "oc_1",
                        "message_type": "text",
                        "content": json.dumps({"text": "hello"}),
                    },
                },
            },
        }
        auth = AuthResult(
            authorized=True,
            open_id="ou_1",
            tenant_key="tenant_1",
            user_id="alice",
            roles=("admin",),
        )

        request = build_agent_request(payload, auth)

        self.assertEqual(request["request_id"], "lark:evt_1")
        self.assertEqual(request["source"]["platform"], "lark")
        self.assertEqual(request["message"]["text"], "hello")
        self.assertEqual(request["auth"]["system_user_id"], "alice")
        self.assertEqual(request["reply"]["target_id"], "oc_1")


if __name__ == "__main__":
    unittest.main()
