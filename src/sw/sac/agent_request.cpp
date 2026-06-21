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

#include "sw/sac/agent_request.h"

#include <stdexcept>

#include <nlohmann/json.hpp>

namespace sw::sac {
namespace {

std::string required_string(const nlohmann::json &json,
        const std::string &field) {
    if (!json.contains(field) || !json.at(field).is_string()
            || json.at(field).get<std::string>().empty()) {
        throw std::invalid_argument("missing required string field: " + field);
    }
    return json.at(field).get<std::string>();
}

std::string optional_string(const nlohmann::json &json,
        const std::string &field) {
    if (!json.contains(field) || json.at(field).is_null()) {
        return "";
    }
    if (!json.at(field).is_string()) {
        throw std::invalid_argument("field must be a string: " + field);
    }
    return json.at(field).get<std::string>();
}

std::vector<std::string> optional_string_array(const nlohmann::json &json,
        const std::string &field) {
    std::vector<std::string> values;
    if (!json.contains(field) || json.at(field).is_null()) {
        return values;
    }
    if (!json.at(field).is_array()) {
        throw std::invalid_argument("field must be an array: " + field);
    }
    for (const auto &item : json.at(field)) {
        if (!item.is_string()) {
            throw std::invalid_argument("array field must contain strings: " + field);
        }
        values.push_back(item.get<std::string>());
    }
    return values;
}

} // namespace

AgentRequest parse_agent_request_json(const std::string &body) {
    auto json = nlohmann::json::parse(body);
    if (!json.is_object()) {
        throw std::invalid_argument("agent request must be a JSON object");
    }

    const auto &source = json.at("source");
    const auto &message = json.at("message");
    const auto &auth = json.value("auth", nlohmann::json::object());
    const auto &reply = json.at("reply");
    const auto &metadata = json.value("metadata", nlohmann::json::object());

    AgentRequest request;
    request.version = required_string(json, "version");
    request.request_id = required_string(json, "request_id");

    request.source.platform = required_string(source, "platform");
    request.source.tenant_id = optional_string(source, "tenant_id");
    request.source.user_id = required_string(source, "user_id");
    request.source.chat_id = required_string(source, "chat_id");
    request.source.message_id = optional_string(source, "message_id");

    request.message.type = required_string(message, "type");
    request.message.text = required_string(message, "text");
    if (request.message.type != "text") {
        throw std::invalid_argument("only text agent requests are supported");
    }

    request.auth.system_user_id = required_string(auth, "system_user_id");
    request.auth.roles = optional_string_array(auth, "roles");

    request.reply.platform = required_string(reply, "platform");
    request.reply.target_type = required_string(reply, "target_type");
    request.reply.target_id = required_string(reply, "target_id");
    request.reply.tenant_id = optional_string(reply, "tenant_id");
    request.reply.user_id = optional_string(reply, "user_id");

    request.received_at = optional_string(metadata, "received_at");
    return request;
}

} // namespace sw::sac
