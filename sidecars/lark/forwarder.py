from __future__ import annotations

import json
import logging
from dataclasses import dataclass
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


logger = logging.getLogger(__name__)


class ForwardError(RuntimeError):
    pass


@dataclass(frozen=True)
class HttpForwarder:
    url: str
    timeout_seconds: float = 5.0
    bearer_token: str | None = None

    def forward(self, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        headers = {
            "Content-Type": "application/json; charset=utf-8",
            "User-Agent": "sac-lark-sidecar/1.0",
        }
        if self.bearer_token:
            headers["Authorization"] = f"Bearer {self.bearer_token}"

        request = Request(self.url, data=body, headers=headers, method="POST")
        try:
            with urlopen(request, timeout=self.timeout_seconds) as response:
                if not 200 <= response.status < 300:
                    raise ForwardError(
                        f"downstream returned HTTP {response.status}"
                    )
        except HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise ForwardError(
                f"downstream returned HTTP {exc.code}: {detail}"
            ) from exc
        except URLError as exc:
            raise ForwardError(
                f"failed to reach downstream: {exc.reason}"
            ) from exc
        except TimeoutError as exc:
            raise ForwardError("downstream request timed out") from exc
