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

#ifndef SEWENEW_SAC_AGENT_SERVER_H
#define SEWENEW_SAC_AGENT_SERVER_H

#include <string>

#include "sw/sac/agent_job_queue.h"

namespace sw::sac {

struct AgentServerOptions {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string bearer_token;
};

class AgentServer {
public:
    AgentServer(const AgentServerOptions &opts, AgentJobQueue &queue);

    AgentServer(const AgentServer &) = delete;
    AgentServer &operator=(const AgentServer &) = delete;

    void listen();

private:
    bool _authorized(const std::string &authorization) const;

    AgentServerOptions _opts;
    AgentJobQueue &_queue;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_AGENT_SERVER_H
