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

#ifndef SEWENEW_SAC_AGENT_REQUEST_H
#define SEWENEW_SAC_AGENT_REQUEST_H

#include <string>
#include <vector>

namespace sw::sac {

struct AgentRequestSource {
    std::string platform;
    std::string tenant_id;
    std::string user_id;
    std::string chat_id;
    std::string message_id;
};

struct AgentRequestMessage {
    std::string type;
    std::string text;
};

struct AgentRequestAuth {
    std::string system_user_id;
    std::vector<std::string> roles;
};

struct ReplyTarget {
    std::string platform;
    std::string target_type;
    std::string target_id;
    std::string tenant_id;
    std::string user_id;
};

struct AgentRequest {
    std::string version;
    std::string request_id;
    AgentRequestSource source;
    AgentRequestMessage message;
    AgentRequestAuth auth;
    ReplyTarget reply;
    std::string received_at;
};

AgentRequest parse_agent_request_json(const std::string &body);

} // namespace sw::sac

#endif // end SEWENEW_SAC_AGENT_REQUEST_H
