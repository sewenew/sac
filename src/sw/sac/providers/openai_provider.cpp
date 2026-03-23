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
    case Role::TOOL:      return "tool";
    }
    return "user";
}

// Serialise a single Message to JSON, handling all role variants.
nlohmann::json message_to_json(const Message &m) {
    if (m.role == Role::TOOL) {
        return {
            {"role", "tool"},
            {"content", m.content},
            {"tool_call_id", m.tool_call_id},
        };
    }

    if (m.role == Role::ASSISTANT && !m.tool_calls.empty()) {
        nlohmann::json tc_arr = nlohmann::json::array();
        for (const auto &tc : m.tool_calls) {
            tc_arr.push_back({
                {"id", tc.id},
                {"type", "function"},
                {"function", {{"name", tc.name}, {"arguments", tc.arguments}}},
            });
        }
        nlohmann::json j = {
            {"role", "assistant"},
            {"content", m.content.empty() ? nlohmann::json(nullptr) : nlohmann::json(m.content)},
            {"tool_calls", tc_arr},
        };
        return j;
    }

    return {{"role", role_to_string(m.role)}, {"content", m.content}};
}

// Build the "tools" array from ToolDef list.
nlohmann::json tools_to_json(const std::vector<ToolDef> &tools) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &t : tools) {
        nlohmann::json props = nlohmann::json::object();
        nlohmann::json required = nlohmann::json::array();
        for (const auto &p : t.parameters) {
            props[p.name] = {{"type", p.type}, {"description", p.description}};
            if (p.required) {
                required.push_back(p.name);
            }
        }
        arr.push_back({
            {"type", "function"},
            {"function", {
                {"name", t.name},
                {"description", t.description},
                {"parameters", {
                    {"type", "object"},
                    {"properties", props},
                    {"required", required},
                }},
            }},
        });
    }
    return arr;
}

} // namespace

OpenAiProvider::OpenAiProvider(const OpenAiOptions &opts) : _opts(opts) {}

// ---------------------------------------------------------------------------
// chat — blocking
// ---------------------------------------------------------------------------

std::string OpenAiProvider::chat(
        const std::vector<Message> &messages,
        HttpClient &http) {
    auto req = _build_chat_request(messages, false);
    auto url = _opts.base_url + "/chat/completions";
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
    auto url = _opts.base_url + "/chat/completions";

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
    auto url = _opts.base_url + "/embeddings";
    auto response = http.post(url, _auth_headers(), req.dump());
    return _extract_embedding(response);
}

// ---------------------------------------------------------------------------
// chat_with_tools — one tool-augmented turn
// ---------------------------------------------------------------------------

Message OpenAiProvider::chat_with_tools(
        const std::vector<Message> &messages,
        const std::vector<ToolDef> &tools,
        HttpClient &http) {
    nlohmann::json msg_array = nlohmann::json::array();
    for (const auto &m : messages) {
        msg_array.push_back(message_to_json(m));
    }

    nlohmann::json req = {
        {"model", _opts.model},
        {"messages", msg_array},
        {"tools", tools_to_json(tools)},
        {"tool_choice", "auto"},
    };

    auto url = _opts.base_url + "/chat/completions";
    auto response_str = http.post(url, _auth_headers(), req.dump());

    try {
        auto j = nlohmann::json::parse(response_str);

        if (j.contains("error")) {
            throw ApiError(j["error"].value("message", response_str));
        }

        const auto &msg = j.at("choices").at(0).at("message");
        Message result;
        result.role = Role::ASSISTANT;
        result.content = msg.value("content", "");

        if (msg.contains("tool_calls") && !msg["tool_calls"].is_null()) {
            for (const auto &tc : msg["tool_calls"]) {
                ToolCall call;
                call.id = tc.at("id").get<std::string>();
                call.name = tc.at("function").at("name").get<std::string>();
                call.arguments = tc.at("function").at("arguments").get<std::string>();
                result.tool_calls.push_back(std::move(call));
            }
        }

        return result;
    } catch (const Error &) {
        throw;
    } catch (const nlohmann::json::exception &e) {
        throw ParseError(std::string("OpenAI tool-call parse error: ") + e.what()
                + " | body: " + response_str);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

HeaderMap OpenAiProvider::_auth_headers() const {
    return {
        {"Authorization", "Bearer " + _opts.api_key},
        {"Content-Type", "application/json"},
    };
}

nlohmann::json OpenAiProvider::_build_chat_request(
        const std::vector<Message> &messages, bool stream) const {
    nlohmann::json msg_array = nlohmann::json::array();
    for (const auto &m : messages) {
        msg_array.push_back(message_to_json(m));
    }

    nlohmann::json req = {
        {"model", _opts.model},
        {"messages", msg_array},
        {"stream", stream},
    };
    return req;
}

nlohmann::json OpenAiProvider::_build_embed_request(const std::string &text) const {
    return {
        {"model", _opts.model},
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
