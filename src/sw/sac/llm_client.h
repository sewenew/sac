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

#ifndef SEWENEW_SAC_LLM_CLIENT_H
#define SEWENEW_SAC_LLM_CLIENT_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "sw/sac/providers/llm_provider.h"

namespace sw::sac {

class HttpClient;
class LlmProvider;
struct AnthropicOptions;
struct OpenAiOptions;

enum class Role {
    SYSTEM,
    USER,
    ASSISTANT,
    TOOL,
};

// One function call requested by the model.
struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;  // raw JSON string
};

struct Message {
    Role role;
    std::string content;
    std::vector<ToolCall> tool_calls;  // non-empty for ASSISTANT tool-call turns
    std::string tool_call_id;          // non-empty for TOOL role
};

// A single parameter in a tool's JSON-Schema-style definition.
struct ToolParam {
    std::string name;
    std::string description;
    std::string type = "string";  // "string" | "number" | "integer" | "boolean"
    bool required = true;
};

// Describes a callable tool that can be offered to the model.
struct ToolDef {
    std::string name;
    std::string description;
    std::vector<ToolParam> parameters;
};

using StreamCallback = std::function<void(const std::string &token)>;

// Factory functions — return the appropriate provider without requiring callers
// to include provider headers directly.
std::unique_ptr<LlmProvider> make_anthropic_provider(const AnthropicOptions &opts);

// OpenAI-compatible: also handles Volcengine ARK and Kimi via LlmConfig.
std::unique_ptr<LlmProvider> make_openai_provider(const OpenAiOptions &opts);

std::unique_ptr<LlmProvider> make_kimi_provider(const OpenAiOptions &opts);

std::unique_ptr<LlmProvider> make_ark_provider(const OpenAiOptions &opts);

// Stable facade over any LlmProvider + HttpClient combination.
// Does NOT own the HttpClient; the caller manages its lifetime, which must
// exceed that of the LlmClient.
class LlmClient {
public:
    explicit LlmClient(std::unique_ptr<LlmProvider> provider, HttpClient &http);

    LlmClient(const LlmClient &) = delete;
    LlmClient &operator=(const LlmClient &) = delete;

    // Defined in .cpp where LlmProvider is complete, so unique_ptr can delete it.
    ~LlmClient();

    // Blocking chat. Returns the full assistant reply.
    std::string chat(const std::vector<Message> &messages);

    // Streaming chat. callback receives successive tokens as they arrive.
    void chat_stream(const std::vector<Message> &messages, StreamCallback callback);

    // One turn of a tool-augmented chat. Returns the assistant Message, which
    // either contains a text reply (tool_calls empty) or a list of tool calls
    // to be executed by the caller before the next turn.
    Message chat_with_tools(
            const std::vector<Message> &messages,
            const std::vector<ToolDef> &tools);

private:
    std::unique_ptr<LlmProvider> _provider;
    HttpClient &_http;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_LLM_CLIENT_H
