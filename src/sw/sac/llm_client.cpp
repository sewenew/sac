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

#include "sw/sac/llm_client.h"

#include <cassert>

#include "sw/sac/providers/anthropic_provider.h"
#include "sw/sac/providers/llm_provider.h"
#include "sw/sac/providers/openai_provider.h"

namespace sw::sac {

// ---------------------------------------------------------------------------
// Factory functions
// ---------------------------------------------------------------------------

std::unique_ptr<LlmProvider> make_anthropic_provider(const AnthropicOptions &opts) {
    return std::make_unique<AnthropicProvider>(opts);
}

std::unique_ptr<LlmProvider> make_openai_provider(const OpenAiOptions &opts) {
    return std::make_unique<OpenAiProvider>(opts);
}

std::unique_ptr<LlmProvider> make_ark_provider(const OpenAiOptions &opts) {
    return std::make_unique<OpenAiProvider>(opts);
}

std::unique_ptr<LlmProvider> make_kimi_provider(const OpenAiOptions &opts) {
    return std::make_unique<OpenAiProvider>(opts);
}

// ---------------------------------------------------------------------------
// LlmClient
// ---------------------------------------------------------------------------

LlmClient::~LlmClient() = default;

LlmClient::LlmClient(std::unique_ptr<LlmProvider> provider, HttpClient &http)
        : _provider(std::move(provider)), _http(http) {
    assert(_provider != nullptr);
}

std::string LlmClient::chat(const std::vector<Message> &messages) {
    auto req = _provider->build_chat_request(messages);
    auto response = _http.post(req.url, req.headers, req.body);
    return _provider->parse_chat_response(response);
}

void LlmClient::chat_stream(const std::vector<Message> &messages, StreamCallback callback) {
    auto req = _provider->build_chat_stream_request(messages);
    _http.post_sse(req.url, req.headers, req.body,
            [this, callback](const std::string &data_line) {
                auto token = _provider->parse_chat_stream_response(data_line);
                if (!token.empty()) {
                    callback(token);
                }
            });
}

Message LlmClient::chat_with_tools(
        const std::vector<Message> &messages,
        const std::vector<ToolDef> &tools) {
    auto req = _provider->build_chat_with_tools_request(messages, tools);
    auto response = _http.post(req.url, req.headers, req.body);
    return _provider->parse_tool_response(response);
}

} // namespace sw::sac
