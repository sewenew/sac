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

#include "sw/sac/providers/anthropic_provider.h"

#include "sw/sac/errors.h"
#include "sw/sac/http_client.h"

namespace sw::sac {

namespace {

std::string role_to_string(Role role) {
    switch (role) {
    case Role::USER:      return "user";
    case Role::ASSISTANT: return "assistant";
    // SYSTEM is handled separately (top-level field); callers must not pass it
    // through to this helper.
    default:              return "user";
    }
}

} // namespace

AnthropicProvider::AnthropicProvider(LlmConfig config) : _config(std::move(config)) {}

// ---------------------------------------------------------------------------
// chat — blocking
// ---------------------------------------------------------------------------

std::string AnthropicProvider::chat(
        const std::vector<Message> &messages,
        HttpClient &http) {
    auto req = _build_request(messages, false);
    auto url = _config.base_url + "/messages";
    auto response = http.post(url, _auth_headers(), req.dump());
    return _extract_content(response);
}

// ---------------------------------------------------------------------------
// chat_stream — SSE
// ---------------------------------------------------------------------------

void AnthropicProvider::chat_stream(
        const std::vector<Message> &messages,
        HttpClient &http,
        StreamCallback callback) {
    auto req = _build_request(messages, true);
    auto url = _config.base_url + "/messages";

    http.post_sse(url, _auth_headers(), req.dump(),
            [this, &callback](const std::string &data_line) {
                auto token = _extract_sse_token(data_line);
                if (!token.empty()) {
                    callback(token);
                }
            });
}

// ---------------------------------------------------------------------------
// embed — not supported
// ---------------------------------------------------------------------------

std::vector<float> AnthropicProvider::embed(
        const std::string & /*text*/,
        HttpClient & /*http*/) {
    throw ApiError("Anthropic does not provide an embeddings endpoint");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

HeaderMap AnthropicProvider::_auth_headers() const {
    return {
        {"x-api-key", _config.api_key},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"},
    };
}

nlohmann::json AnthropicProvider::_build_request(
        const std::vector<Message> &messages, bool stream) const {
    nlohmann::json req = {
        {"model", _config.model},
        {"max_tokens", DEFAULT_MAX_TOKENS},
        {"stream", stream},
    };

    nlohmann::json msg_array = nlohmann::json::array();
    for (const auto &m : messages) {
        if (m.role == Role::SYSTEM) {
            // Anthropic takes system as a top-level string field.
            // If multiple SYSTEM messages are present, concatenate them.
            if (req.contains("system")) {
                req["system"] = req["system"].get<std::string>() + "\n" + m.content;
            } else {
                req["system"] = m.content;
            }
        } else {
            msg_array.push_back({
                {"role", role_to_string(m.role)},
                {"content", m.content},
            });
        }
    }

    req["messages"] = msg_array;
    return req;
}

std::string AnthropicProvider::_extract_content(const std::string &response_json) const {
    try {
        auto j = nlohmann::json::parse(response_json);

        if (j.contains("error")) {
            auto &err = j["error"];
            throw ApiError(err.value("message", response_json));
        }

        return j.at("content").at(0).at("text").get<std::string>();
    } catch (const Error &) {
        throw;
    } catch (const nlohmann::json::exception &e) {
        throw ParseError(std::string("Anthropic response parse error: ") + e.what()
                + " | body: " + response_json);
    }
}

std::string AnthropicProvider::_extract_sse_token(const std::string &data_line) const {
    try {
        auto j = nlohmann::json::parse(data_line);

        // Only "content_block_delta" events carry text.
        if (j.value("type", "") != "content_block_delta") {
            return "";
        }

        const auto &delta = j["delta"];
        if (delta.value("type", "") != "text_delta") {
            return "";
        }

        return delta.value("text", "");
    } catch (const nlohmann::json::exception &) {
        return "";
    }
}

} // namespace sw::sac
