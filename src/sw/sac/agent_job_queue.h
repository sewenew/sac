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

#ifndef SEWENEW_SAC_AGENT_JOB_QUEUE_H
#define SEWENEW_SAC_AGENT_JOB_QUEUE_H

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include "sw/sac/agent.h"
#include "sw/sac/agent_request.h"
#include "sw/sac/channel_responder_registry.h"

namespace sw::sac {

class AgentJobQueue {
public:
    AgentJobQueue(Agent &agent, ChannelResponderRegistry &responders,
            std::size_t max_queue_size = 100);

    AgentJobQueue(const AgentJobQueue &) = delete;
    AgentJobQueue &operator=(const AgentJobQueue &) = delete;

    ~AgentJobQueue();

    bool enqueue(AgentRequest request);

    std::size_t size() const;

private:
    void _run();
    void _handle(const AgentRequest &request);
    void _remember_request_id(const std::string &request_id);
    bool _seen_request_id(const std::string &request_id) const;

    Agent &_agent;
    ChannelResponderRegistry &_responders;
    std::size_t _max_queue_size;
    std::deque<AgentRequest> _queue;
    std::deque<std::string> _recent_order;
    std::unordered_set<std::string> _recent_ids;
    bool _stopped = false;
    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _worker;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_AGENT_JOB_QUEUE_H
