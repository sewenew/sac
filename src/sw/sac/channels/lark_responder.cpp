/**************************************************************************
   Copyright (c) 2026 sewenew

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 *************************************************************************/

#include "sw/sac/channels/lark_responder.h"

#include <algorithm>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "sw/sac/errors.h"

namespace sw::sac {
namespace {

constexpr int TOKEN_REFRESH_SKEW_SECONDS = 300;

std::string strip_trailing_slash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

void check_lark_response(const std::string &body, const std::string &operation) {
    auto json = nlohmann::json::parse(body);
    const auto code = json.value("code", -1);
    if (code != 0) {
        throw ApiError("Lark " + operation + " failed: code="
                + std::to_string(code) + " msg=" + json.value("msg", ""));
    }
}

} // namespace

LarkResponder::LarkResponder(const LarkResponderOptions &opts, HttpClient &http)
    : _opts(opts), _http(http) {
    if (_opts.app_id.empty() || _opts.app_secret.empty()) {
        throw std::invalid_argument("Lark app_id and app_secret are required");
    }
    _opts.api_base_url = strip_trailing_slash(_opts.api_base_url);
}

void LarkResponder::send_text(const ReplyTarget &target,
        const std::string &text) {
    if (target.target_type != "chat" || target.target_id.empty()) {
        throw std::invalid_argument("Lark reply target must be a chat target");
    }

    nlohmann::json body = {
        {"receive_id", target.target_id},
        {"msg_type", "text"},
        {"content", nlohmann::json({{"text", text}}).dump()},
    };

    HeaderMap headers = {
        {"Authorization", "Bearer " + _tenant_access_token()},
        {"Content-Type", "application/json; charset=utf-8"},
    };
    auto response = _http.post(
            _opts.api_base_url
            + "/open-apis/im/v1/messages?receive_id_type=chat_id",
            headers,
            body.dump());
    check_lark_response(response, "send message");
}

std::string LarkResponder::_tenant_access_token() {
    std::lock_guard<std::mutex> lock(_mutex);
    const auto now = std::chrono::steady_clock::now();
    if (!_tenant_access_token_value.empty() && now < _token_expires_at) {
        return _tenant_access_token_value;
    }
    _refresh_token();
    return _tenant_access_token_value;
}

void LarkResponder::_refresh_token() {
    nlohmann::json body = {
        {"app_id", _opts.app_id},
        {"app_secret", _opts.app_secret},
    };
    HeaderMap headers = {
        {"Content-Type", "application/json; charset=utf-8"},
    };
    auto response = _http.post(
            _opts.api_base_url
            + "/open-apis/auth/v3/tenant_access_token/internal",
            headers,
            body.dump());
    auto json = nlohmann::json::parse(response);
    const auto code = json.value("code", -1);
    if (code != 0) {
        throw ApiError("Lark token request failed: code="
                + std::to_string(code) + " msg=" + json.value("msg", ""));
    }
    _tenant_access_token_value = json.value("tenant_access_token", "");
    if (_tenant_access_token_value.empty()) {
        throw ParseError("Lark token response missing tenant_access_token");
    }

    const auto expire = json.value("expire", 7200);
    const auto usable_seconds = std::max(60, expire - TOKEN_REFRESH_SKEW_SECONDS);
    _token_expires_at = std::chrono::steady_clock::now()
            + std::chrono::seconds(usable_seconds);
}

} // namespace sw::sac
