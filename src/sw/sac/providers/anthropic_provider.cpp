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

// Parser for blocking chat responses.
class AnthropicChatParser : public ChatResponseParser {
public:
    std::string parse(const std::string &response_body) override {
        try {
            auto j = nlohmann::json::parse(response_body);

            if (j.contains("error")) {
                auto &err = j["error"];
                throw ApiError(err.value("message", response_body));
            }

            return j.at("content").at(0).at("text").get<std::string>();
        } catch (const Error &) {
            throw;
        } catch (const nlohmann::json::exception &e) {
            throw ParseError(std::string("Anthropic response parse error: ") + e.what()
                    + " | body: " + response_body);
        }
    }
};

// Parser for streaming chat responses.
class AnthropicStreamParser : public StreamResponseParser {
public:
    std::string parse_sse_token(const std::string &data_line) override {
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
};

} // namespace

AnthropicProvider::AnthropicProvider(const AnthropicOptions &opts) : _opts(opts) {}

// ---------------------------------------------------------------------------
// chat — blocking
// ---------------------------------------------------------------------------

ProviderRequest AnthropicProvider::build_chat_request(
        const std::vector<Message> &messages,
        ResponseParserPtr &parser) {
    parser = std::make_unique<AnthropicChatParser>();
    return {
        _opts.base_url + "/messages",
        _auth_headers(),
        _build_request(messages, false).dump()
    };
}

// ---------------------------------------------------------------------------
// chat_stream — SSE
// ---------------------------------------------------------------------------

ProviderRequest AnthropicProvider::build_chat_stream_request(
        const std::vector<Message> &messages,
        ResponseParserPtr &parser) {
    parser = std::make_unique<AnthropicStreamParser>();
    return {
        _opts.base_url + "/messages",
        _auth_headers(),
        _build_request(messages, true).dump()
    };
}

// ---------------------------------------------------------------------------
// chat_with_tools — not supported
// ---------------------------------------------------------------------------

ProviderRequest AnthropicProvider::build_chat_with_tools_request(
        const std::vector<Message> & /*messages*/,
        const std::vector<ToolDef> & /*tools*/,
        ResponseParserPtr & /*parser*/) {
    throw ApiError("Anthropic does not support tool use");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

HeaderMap AnthropicProvider::_auth_headers() const {
    return {
        {"x-api-key", _opts.api_key},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"},
    };
}

nlohmann::json AnthropicProvider::_build_request(
        const std::vector<Message> &messages, bool stream) const {
    nlohmann::json req = {
        {"model", _opts.model},
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

} // namespace sw::sac
