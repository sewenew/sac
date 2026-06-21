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

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sw/sac/agent.h"
#include "sw/sac/agent_job_queue.h"
#include "sw/sac/agent_server.h"
#include "sw/sac/channel_responder_registry.h"
#include "sw/sac/channels/lark_responder.h"
#include "sw/sac/http_client.h"
#include "sw/sac/llm_client.h"
#include "sw/sac/errors.h"
#include "sw/sac/providers/openai_provider.h"
#include "sw/sac/providers/moonshot_provider.h"
#include "sw/sac/tools/tools.h"

namespace {

std::string require_env(const char *name) {
    const char *value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        throw std::runtime_error(std::string(name) + " is not set");
    }
    return value;
}

std::string optional_env(const char *name, const std::string &default_value) {
    const char *value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return default_value;
    }
    return value;
}

int optional_int_env(const char *name, int default_value) {
    const char *value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return default_value;
    }
    return std::stoi(value);
}

std::size_t optional_size_env(const char *name, std::size_t default_value) {
    const char *value = std::getenv(name);
    if (value == nullptr || std::string(value).empty()) {
        return default_value;
    }
    return static_cast<std::size_t>(std::stoull(value));
}

} // namespace

int main() {
    try {
        auto api_key = require_env("OPENAI_API_KEY");
        auto base_url = require_env("OPENAI_BASE_URL");
        auto model = require_env("OPENAI_MODEL");

        sw::sac::CurlHttpClient http;
        sw::sac::MoonshotOptions opts{base_url, api_key, model};
        sw::sac::LlmClient client(
            sw::sac::make_moonshot_provider(opts),
            http);

        auto tools = sw::sac::tools::make_all_tools();
        sw::sac::Agent agent(client, std::move(tools), 10);

        sw::sac::ChannelResponderRegistry responders;
        sw::sac::LarkResponderOptions lark_opts;
        lark_opts.app_id = require_env("LARK_APP_ID");
        lark_opts.app_secret = require_env("LARK_APP_SECRET");
        lark_opts.api_base_url = optional_env(
                "LARK_API_BASE_URL",
                "https://open.feishu.cn");
        responders.add(
                "lark",
                std::make_unique<sw::sac::LarkResponder>(lark_opts, http));

        sw::sac::AgentJobQueue queue(
                agent,
                responders,
                optional_size_env("SAC_AGENT_QUEUE_SIZE", 100));

        sw::sac::AgentServerOptions server_opts;
        server_opts.host = optional_env("SAC_AGENT_HOST", "127.0.0.1");
        server_opts.port = optional_int_env("SAC_AGENT_PORT", 8080);
        server_opts.bearer_token = optional_env("SAC_AGENT_FORWARD_TOKEN", "");

        sw::sac::AgentServer server(server_opts, queue);
        server.listen();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
