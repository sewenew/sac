from __future__ import annotations

import json
import unittest
from io import BytesIO
from urllib.error import HTTPError
from unittest.mock import patch

from forwarder import ForwardError, HttpForwarder


class _Response:
    status = 204

    def __enter__(self) -> "_Response":
        return self

    def __exit__(self, *args: object) -> None:
        return None


class HttpForwarderTest(unittest.TestCase):
    @patch("forwarder.urlopen", return_value=_Response())
    def test_forward_posts_json_and_bearer_token(self, mock_urlopen) -> None:
        forwarder = HttpForwarder(
            url="http://127.0.0.1:8080/events",
            bearer_token="secret",
        )
        forwarder.forward({"event_type": "im.message.receive_v1"})

        request = mock_urlopen.call_args.args[0]
        self.assertEqual(request.method, "POST")
        self.assertEqual(
            json.loads(request.data),
            {"event_type": "im.message.receive_v1"},
        )
        self.assertEqual(
            request.get_header("Authorization"),
            "Bearer secret",
        )
        self.assertEqual(mock_urlopen.call_args.kwargs["timeout"], 5.0)

    @patch(
        "forwarder.urlopen",
        side_effect=HTTPError(
            url="http://127.0.0.1/events",
            code=503,
            msg="Service Unavailable",
            hdrs={},
            fp=BytesIO(b'{"error":"queue_full"}\n'),
        ),
    )
    def test_forward_error_includes_response_body(self, mock_urlopen) -> None:
        forwarder = HttpForwarder(url="http://127.0.0.1/events")

        with self.assertRaisesRegex(ForwardError, "queue_full"):
            forwarder.forward({"event_type": "im.message.receive_v1"})


if __name__ == "__main__":
    unittest.main()
