from __future__ import annotations

import unittest
from unittest.mock import patch

import handler
from pairing import AuthResult


class _Authorizer:
    def authorize_payload(self, payload):
        return AuthResult(
            authorized=False,
            open_id="ou_1",
            tenant_key="tenant_1",
            pairing_code="AB12CD34",
            reason="pending_pairing",
        )


class _Sender:
    def __init__(self) -> None:
        self.sent = []

    def send_pairing_code(self, payload, code):
        self.sent.append((payload, code))
        return True


class HandlerTest(unittest.TestCase):
    def test_unauthorized_event_sends_pairing_code_and_stops(self) -> None:
        sender = _Sender()
        handler.configure_message_sender(sender)

        with patch.object(handler, "authorizer", _Authorizer()):
            result = handler.handle_event({"event_id": "evt_1"})

        self.assertIsNone(result)
        self.assertEqual(len(sender.sent), 1)
        self.assertEqual(sender.sent[0][1], "AB12CD34")

        handler.configure_message_sender(None)


if __name__ == "__main__":
    unittest.main()
