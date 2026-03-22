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

#ifndef SEWENEW_SAC_AGENT_H
#define SEWENEW_SAC_AGENT_H

#include <functional>
#include <string>
#include <vector>

#include "sw/sac/llm_client.h"

namespace sw::sac {

// Function that implements a tool.
// Receives the model's raw arguments JSON string; returns the result as a string.
using ToolFn = std::function<std::string(const std::string &args_json)>;

// Pairs a tool definition (sent to the model) with its implementation.
struct Tool {
    ToolDef def;
    ToolFn fn;
};

// Simple ReAct-style agent loop.
// Sends messages to the model, executes any requested tool calls, appends
// results, and repeats until the model replies without tool calls or
// max_steps is exceeded.
class Agent {
public:
    explicit Agent(LlmClient &client, std::vector<Tool> tools, int max_steps = 10);

    Agent(const Agent &) = delete;
    Agent &operator=(const Agent &) = delete;

    // Run the agent on a single user input. Returns the final text reply.
    // Throws LlmError if max_steps is exceeded without a final answer.
    std::string run(const std::string &user_input);

private:
    std::string _invoke_tool(const ToolCall &tc) const;

    LlmClient &_client;
    std::vector<Tool> _tools;
    int _max_steps;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_AGENT_H
