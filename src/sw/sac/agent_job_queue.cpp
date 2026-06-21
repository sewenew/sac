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

#include "sw/sac/agent_job_queue.h"

#include <iostream>
#include <utility>

namespace sw::sac {
namespace {

constexpr std::size_t RECENT_ID_LIMIT = 1024;

} // namespace

AgentJobQueue::AgentJobQueue(Agent &agent,
        ChannelResponderRegistry &responders,
        std::size_t max_queue_size)
    : _agent(agent),
      _responders(responders),
      _max_queue_size(max_queue_size),
      _worker(&AgentJobQueue::_run, this) {}

AgentJobQueue::~AgentJobQueue() {
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _stopped = true;
    }
    _cv.notify_all();
    if (_worker.joinable()) {
        _worker.join();
    }
}

bool AgentJobQueue::enqueue(AgentRequest request) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_seen_request_id(request.request_id)) {
        return true;
    }
    if (_queue.size() >= _max_queue_size) {
        return false;
    }
    _remember_request_id(request.request_id);
    _queue.push_back(std::move(request));
    _cv.notify_one();
    return true;
}

std::size_t AgentJobQueue::size() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _queue.size();
}

void AgentJobQueue::_run() {
    while (true) {
        AgentRequest request;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait(lock, [this] {
                return _stopped || !_queue.empty();
            });
            if (_stopped && _queue.empty()) {
                return;
            }
            request = std::move(_queue.front());
            _queue.pop_front();
        }
        _handle(request);
    }
}

void AgentJobQueue::_handle(const AgentRequest &request) {
    try {
        auto result = _agent.run(request.message.text);
        _responders.get(request.reply.platform).send_text(request.reply, result);
    } catch (const std::exception &e) {
        std::cerr << "agent job failed: request_id=" << request.request_id
                  << " error=" << e.what() << std::endl;
        try {
            _responders.get(request.reply.platform).send_text(
                    request.reply,
                    "处理失败，请稍后重试。");
        } catch (const std::exception &reply_error) {
            std::cerr << "failed to send error reply: request_id="
                      << request.request_id
                      << " error=" << reply_error.what() << std::endl;
        }
    }
}

void AgentJobQueue::_remember_request_id(const std::string &request_id) {
    _recent_ids.insert(request_id);
    _recent_order.push_back(request_id);
    while (_recent_order.size() > RECENT_ID_LIMIT) {
        _recent_ids.erase(_recent_order.front());
        _recent_order.pop_front();
    }
}

bool AgentJobQueue::_seen_request_id(const std::string &request_id) const {
    return _recent_ids.find(request_id) != _recent_ids.end();
}

} // namespace sw::sac
