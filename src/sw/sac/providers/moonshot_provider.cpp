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

#include "sw/sac/providers/moonshot_provider.h"

#include <nlohmann/json.hpp>

#include "sw/sac/errors.h"

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
            {"reasoning_content", m.reasoning_content},
        };
        return j;
    }

    // For regular assistant messages in tool-based conversations, also add reasoning_content
    if (m.role == Role::ASSISTANT) {
        return {
            {"role", "assistant"},
            {"content", m.content},
            {"reasoning_content", m.reasoning_content},
        };
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

MoonshotProvider::MoonshotProvider(const MoonshotOptions &opts) : _opts(opts) {}

// ---------------------------------------------------------------------------
// chat — blocking
// ---------------------------------------------------------------------------

ProviderRequest MoonshotProvider::build_chat_request(
        const std::vector<Message> &messages) {
    return {
        _opts.base_url + "/chat/completions",
        _auth_headers(),
        _build_chat_request(messages, false).dump()
    };
}

std::string MoonshotProvider::parse_chat_response(const std::string &response_body) {
    try {
        auto j = nlohmann::json::parse(response_body);

        if (j.contains("error")) {
            throw ApiError(j["error"].value("message", response_body));
        }

        return j.at("choices").at(0).at("message").at("content").get<std::string>();
    } catch (const Error &) {
        throw;
    } catch (const nlohmann::json::exception &e) {
        throw ParseError(std::string("OpenAI response parse error: ") + e.what()
                + " | body: " + response_body);
    }
}

// ---------------------------------------------------------------------------
// chat_stream — SSE
// ---------------------------------------------------------------------------

ProviderRequest MoonshotProvider::build_chat_stream_request(
        const std::vector<Message> &messages) {
    return {
        _opts.base_url + "/chat/completions",
        _auth_headers(),
        _build_chat_request(messages, true).dump()
    };
}

std::string MoonshotProvider::parse_chat_stream_response(const std::string &data_line) {
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

// ---------------------------------------------------------------------------
// chat_with_tools — one tool-augmented turn
// ---------------------------------------------------------------------------

ProviderRequest MoonshotProvider::build_chat_with_tools_request(
        const std::vector<Message> &messages,
        const std::vector<ToolDef> &tools) {
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

    return {
        _opts.base_url + "/chat/completions",
        _auth_headers(),
        req.dump()
    };
}

Message MoonshotProvider::parse_tool_response(const std::string &response_body) {
    try {
        auto j = nlohmann::json::parse(response_body);

        if (j.contains("error")) {
            throw ApiError(j["error"].value("message", response_body));
        }

        const auto &msg = j.at("choices").at(0).at("message");
        Message result;
        result.role = Role::ASSISTANT;
        result.content = msg.value("content", "");
        result.reasoning_content = msg.value("reasoning_content", "");

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
                + " | body: " + response_body);
    }
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

HeaderMap MoonshotProvider::_auth_headers() const {
    return {
        {"Authorization", "Bearer " + _opts.api_key},
        {"Content-Type", "application/json"},
    };
}

nlohmann::json MoonshotProvider::_build_chat_request(
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

} // namespace sw::sac
