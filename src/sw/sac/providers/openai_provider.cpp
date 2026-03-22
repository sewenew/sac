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

#include "sw/sac/providers/openai_provider.h"

#include <nlohmann/json.hpp>

#include "sw/sac/errors.h"
#include "sw/sac/http_client.h"

namespace sw::sac {

namespace {

std::string role_to_string(Role role) {
    switch (role) {
    case Role::SYSTEM:    return "system";
    case Role::USER:      return "user";
    case Role::ASSISTANT: return "assistant";
    }
    return "user";
}

} // namespace

OpenAiProvider::OpenAiProvider(LlmConfig config) : _config(std::move(config)) {}

// ---------------------------------------------------------------------------
// chat — blocking
// ---------------------------------------------------------------------------

std::string OpenAiProvider::chat(
        const std::vector<Message> &messages,
        HttpClient &http) {
    auto req = _build_chat_request(messages, false);
    auto url = _config.base_url + "/chat/completions";
    auto response = http.post(url, _auth_headers(), req.dump());
    return _extract_content(response);
}

// ---------------------------------------------------------------------------
// chat_stream — SSE
// ---------------------------------------------------------------------------

void OpenAiProvider::chat_stream(
        const std::vector<Message> &messages,
        HttpClient &http,
        StreamCallback callback) {
    auto req = _build_chat_request(messages, true);
    auto url = _config.base_url + "/chat/completions";

    http.post_sse(url, _auth_headers(), req.dump(),
            [this, &callback](const std::string &data_line) {
                auto token = _extract_sse_token(data_line);
                if (!token.empty()) {
                    callback(token);
                }
            });
}

// ---------------------------------------------------------------------------
// embed — blocking
// ---------------------------------------------------------------------------

std::vector<float> OpenAiProvider::embed(
        const std::string &text,
        HttpClient &http) {
    auto req = _build_embed_request(text);
    auto url = _config.base_url + "/embeddings";
    auto response = http.post(url, _auth_headers(), req.dump());
    return _extract_embedding(response);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

HeaderMap OpenAiProvider::_auth_headers() const {
    return {
        {"Authorization", "Bearer " + _config.api_key},
        {"Content-Type", "application/json"},
    };
}

nlohmann::json OpenAiProvider::_build_chat_request(
        const std::vector<Message> &messages, bool stream) const {
    nlohmann::json msg_array = nlohmann::json::array();
    for (const auto &m : messages) {
        msg_array.push_back({{"role", role_to_string(m.role)}, {"content", m.content}});
    }

    nlohmann::json req = {
        {"model", _config.model},
        {"messages", msg_array},
        {"stream", stream},
    };
    return req;
}

nlohmann::json OpenAiProvider::_build_embed_request(const std::string &text) const {
    return {
        {"model", _config.model},
        {"input", text},
    };
}

std::string OpenAiProvider::_extract_content(const std::string &response_json) const {
    try {
        auto j = nlohmann::json::parse(response_json);

        if (j.contains("error")) {
            throw ApiError(j["error"].value("message", response_json));
        }

        return j.at("choices").at(0).at("message").at("content").get<std::string>();
    } catch (const Error &) {
        throw;
    } catch (const nlohmann::json::exception &e) {
        throw ParseError(std::string("OpenAI response parse error: ") + e.what()
                + " | body: " + response_json);
    }
}

std::string OpenAiProvider::_extract_sse_token(const std::string &data_line) const {
    try {
        auto j = nlohmann::json::parse(data_line);

        if (!j.contains("choices") || j["choices"].empty()) {
            return "";
        }

        const auto &delta = j["choices"][0]["delta"];
        if (!delta.contains("content") || delta["content"].is_null()) {
            return "";
        }

        return delta["content"].get<std::string>();
    } catch (const nlohmann::json::exception &) {
        // Malformed SSE chunk — skip silently rather than aborting the stream.
        return "";
    }
}

std::vector<float> OpenAiProvider::_extract_embedding(
        const std::string &response_json) const {
    try {
        auto j = nlohmann::json::parse(response_json);

        if (j.contains("error")) {
            throw ApiError(j["error"].value("message", response_json));
        }

        const auto &arr = j.at("data").at(0).at("embedding");
        std::vector<float> result;
        result.reserve(arr.size());
        for (const auto &v : arr) {
            result.push_back(v.get<float>());
        }
        return result;
    } catch (const Error &) {
        throw;
    } catch (const nlohmann::json::exception &e) {
        throw ParseError(std::string("OpenAI embedding parse error: ") + e.what()
                + " | body: " + response_json);
    }
}

} // namespace sw::sac
