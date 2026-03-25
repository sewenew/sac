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

#include "sw/sac/agent.h"

#include <cassert>

#include "sw/sac/errors.h"
#include "sw/sac/llm_client.h"

namespace sw::sac {

Agent::Agent(LlmClient &client, std::vector<Tool> tools, int max_steps)
    : _client(client), _tools(std::move(tools)), _max_steps(max_steps) {
    assert(_max_steps > 0);
}

std::string Agent::run(const std::string &user_input) {
    // Build conversation history.
    std::vector<Message> messages;
    messages.push_back(Message{Role::USER, user_input, {}, "", ""});

    // Extract ToolDef list for the LLM.
    std::vector<ToolDef> tool_defs;
    for (const auto &tool : _tools) {
        tool_defs.push_back(tool.def);
    }

    // ReAct loop.
    for (int step = 0; step < _max_steps; ++step) {
        // Ask the model for the next action.
        Message assistant_msg = _client.chat_with_tools(messages, tool_defs);

        if (assistant_msg.tool_calls.empty()) {
            // No tool calls: we have the final answer.
            return assistant_msg.content;
        }

        // Append the assistant's tool-call request to history.
        messages.push_back(std::move(assistant_msg));

        // Execute each tool call and append results.
        for (const auto &tc : messages.back().tool_calls) {
            std::string result = _invoke_tool(tc);
            messages.push_back(Message{
                Role::TOOL,
                result,
                {},
                tc.id,
                ""
            });
        }
    }

    throw LlmError("Agent exceeded maximum steps (" + std::to_string(_max_steps) + ") without final answer");
}

std::string Agent::_invoke_tool(const ToolCall &tc) const {
    // Find the tool by name.
    for (const auto &tool : _tools) {
        if (tool.def.name == tc.name) {
            return tool.fn(tc.arguments);
        }
    }
    return "Error: Unknown tool '" + tc.name + "'";
}

} // namespace sw::sac
