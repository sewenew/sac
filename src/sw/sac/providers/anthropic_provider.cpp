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

// Serialise a single Message to JSON for Anthropic API.
// Anthropic uses a different format for tool use:
// - Assistant tool calls are content blocks with type "tool_use"
// - Tool results are content blocks with type "tool_result"
nlohmann::json message_to_json(const Message &m) {
    if (m.role == Role::TOOL) {
        // Tool result: content block with type "tool_result"
        nlohmann::json content_block = {
            {"type", "tool_result"},
            {"tool_use_id", m.tool_call_id},
            {"content", m.content},
        };
        return {
            {"role", "user"},
            {"content", nlohmann::json::array({content_block})},
        };
    }

    if (m.role == Role::ASSISTANT && !m.tool_calls.empty()) {
        // Assistant with tool calls: content blocks with type "tool_use"
        nlohmann::json content_arr = nlohmann::json::array();
        
        // Add text content if present
        if (!m.content.empty()) {
            content_arr.push_back({
                {"type", "text"},
                {"text", m.content},
            });
        }
        
        // Add tool_use blocks
        for (const auto &tc : m.tool_calls) {
            // Parse arguments from JSON string to JSON object
            nlohmann::json input;
            try {
                input = nlohmann::json::parse(tc.arguments);
            } catch (const nlohmann::json::exception &) {
                input = nlohmann::json::object();
            }
            
            content_arr.push_back({
                {"type", "tool_use"},
                {"id", tc.id},
                {"name", tc.name},
                {"input", input},
            });
        }
        
        return {
            {"role", "assistant"},
            {"content", content_arr},
        };
    }

    // Regular message
    nlohmann::json content_arr = nlohmann::json::array();
    content_arr.push_back({
        {"type", "text"},
        {"text", m.content},
    });
    
    return {
        {"role", role_to_string(m.role)},
        {"content", content_arr},
    };
}

// Build the "tools" array from ToolDef list for Anthropic API.
nlohmann::json tools_to_json(const std::vector<ToolDef> &tools) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &t : tools) {
        nlohmann::json input_schema = {
            {"type", "object"},
        };
        
        nlohmann::json props = nlohmann::json::object();
        nlohmann::json required = nlohmann::json::array();
        for (const auto &p : t.parameters) {
            props[p.name] = {
                {"type", p.type},
                {"description", p.description},
            };
            if (p.required) {
                required.push_back(p.name);
            }
        }
        input_schema["properties"] = props;
        if (!required.empty()) {
            input_schema["required"] = required;
        }
        
        arr.push_back({
            {"name", t.name},
            {"description", t.description},
            {"input_schema", input_schema},
        });
    }
    return arr;
}

} // namespace

AnthropicProvider::AnthropicProvider(const AnthropicOptions &opts) : _opts(opts) {}

// ---------------------------------------------------------------------------
// chat — blocking
// ---------------------------------------------------------------------------

ProviderRequest AnthropicProvider::build_chat_request(
        const std::vector<Message> &messages) {
    return {
        _opts.base_url + "/messages",
        _auth_headers(),
        _build_request(messages, false).dump()
    };
}

std::string AnthropicProvider::parse_chat_response(const std::string &response_body) {
    try {
        auto j = nlohmann::json::parse(response_body);

        if (j.contains("error")) {
            auto &err = j["error"];
            throw ApiError(err.value("message", response_body));
        }

        // For non-tool responses, content is an array with text blocks
        const auto &content = j.at("content");
        if (content.empty()) {
            return "";
        }
        
        // Concatenate all text blocks
        std::string result;
        for (const auto &block : content) {
            if (block.value("type", "") == "text") {
                if (!result.empty()) {
                    result += "\n";
                }
                result += block.value("text", "");
            }
        }
        return result;
    } catch (const Error &) {
        throw;
    } catch (const nlohmann::json::exception &e) {
        throw ParseError(std::string("Anthropic response parse error: ") + e.what()
                + " | body: " + response_body);
    }
}

// ---------------------------------------------------------------------------
// chat_stream — SSE
// ---------------------------------------------------------------------------

ProviderRequest AnthropicProvider::build_chat_stream_request(
        const std::vector<Message> &messages) {
    return {
        _opts.base_url + "/messages",
        _auth_headers(),
        _build_request(messages, true).dump()
    };
}

std::string AnthropicProvider::parse_chat_stream_response(const std::string &data_line) {
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

// ---------------------------------------------------------------------------
// chat_with_tools
// ---------------------------------------------------------------------------

ProviderRequest AnthropicProvider::build_chat_with_tools_request(
        const std::vector<Message> &messages,
        const std::vector<ToolDef> &tools) {
    nlohmann::json req = {
        {"model", _opts.model},
        {"max_tokens", DEFAULT_MAX_TOKENS},
        {"stream", false},
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
            msg_array.push_back(message_to_json(m));
        }
    }

    req["messages"] = msg_array;
    req["tools"] = tools_to_json(tools);

    return {
        _opts.base_url + "/messages",
        _auth_headers(),
        req.dump()
    };
}

Message AnthropicProvider::parse_tool_response(const std::string &response_body) {
    try {
        auto j = nlohmann::json::parse(response_body);

        if (j.contains("error")) {
            auto &err = j["error"];
            throw ApiError(err.value("message", response_body));
        }

        Message result;
        result.role = Role::ASSISTANT;
        result.content = "";

        const auto &content = j.at("content");
        
        for (const auto &block : content) {
            const std::string block_type = block.value("type", "");
            
            if (block_type == "text") {
                // Text content
                if (!result.content.empty()) {
                    result.content += "\n";
                }
                result.content += block.value("text", "");
            } else if (block_type == "tool_use") {
                // Tool call
                ToolCall call;
                call.id = block.at("id").get<std::string>();
                call.name = block.at("name").get<std::string>();
                
                // Convert input JSON to string
                if (block.contains("input")) {
                    call.arguments = block["input"].dump();
                } else {
                    call.arguments = "{}";
                }
                
                result.tool_calls.push_back(std::move(call));
            }
        }

        return result;
    } catch (const Error &) {
        throw;
    } catch (const nlohmann::json::exception &e) {
        throw ParseError(std::string("Anthropic tool-call parse error: ") + e.what()
                + " | body: " + response_body);
    }
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
            msg_array.push_back(message_to_json(m));
        }
    }

    req["messages"] = msg_array;
    return req;
}

} // namespace sw::sac
