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

#include "sw/sac/agent_server.h"

#include <iostream>
#include <stdexcept>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "sw/sac/agent_request.h"

namespace sw::sac {

AgentServer::AgentServer(const AgentServerOptions &opts, AgentJobQueue &queue)
    : _opts(opts), _queue(queue) {}

void AgentServer::listen() {
    httplib::Server server;

    server.Get("/healthz", [](const httplib::Request &,
            httplib::Response &response) {
        response.status = 200;
        response.set_content("{\"ok\":true}\n", "application/json");
    });

    server.Post("/agent/requests", [this](const httplib::Request &request,
            httplib::Response &response) {
        std::cout << "received agent request: remote_addr="
                  << request.remote_addr
                  << " content_length=" << request.body.size()
                  << std::endl;
        if (!_authorized(request.get_header_value("Authorization"))) {
            response.status = 401;
            response.set_content("{\"error\":\"unauthorized\"}\n",
                    "application/json");
            return;
        }

        try {
            auto agent_request = parse_agent_request_json(request.body);
            const auto request_id = agent_request.request_id;
            if (!_queue.enqueue(std::move(agent_request))) {
                std::cerr << "agent request queue full: request_id="
                          << request_id
                          << " queue_size=" << _queue.size()
                          << std::endl;
                response.status = 503;
                response.set_content("{\"error\":\"queue_full\"}\n",
                        "application/json");
                return;
            }
            std::cout << "accepted agent request: request_id="
                      << request_id
                      << " queue_size=" << _queue.size()
                      << std::endl;
            response.status = 202;
            response.set_content("{\"status\":\"accepted\"}\n",
                    "application/json");
        } catch (const nlohmann::json::exception &e) {
            response.status = 400;
            response.set_content(
                    std::string("{\"error\":\"invalid_json\",\"detail\":\"")
                    + e.what() + "\"}\n",
                    "application/json");
        } catch (const std::invalid_argument &e) {
            response.status = 422;
            response.set_content(
                    std::string("{\"error\":\"invalid_request\",\"detail\":\"")
                    + e.what() + "\"}\n",
                    "application/json");
        }
    });

    std::cout << "agent server listening on " << _opts.host << ":"
              << _opts.port << std::endl;
    if (!server.listen(_opts.host, _opts.port)) {
        throw std::runtime_error("failed to listen on " + _opts.host + ":"
                + std::to_string(_opts.port));
    }
}

bool AgentServer::_authorized(const std::string &authorization) const {
    if (_opts.bearer_token.empty()) {
        return true;
    }
    return authorization == "Bearer " + _opts.bearer_token;
}

} // namespace sw::sac
