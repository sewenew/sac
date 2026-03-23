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
    ResponseParserPtr parser;
    auto req = _provider->build_chat_request(messages, parser);
    auto response = _http.post(req.url, req.headers, req.body);
    auto *chat_parser = dynamic_cast<ChatResponseParser *>(parser.get());
    assert(chat_parser != nullptr);
    return chat_parser->parse(response);
}

void LlmClient::chat_stream(const std::vector<Message> &messages, StreamCallback callback) {
    ResponseParserPtr parser;
    auto req = _provider->build_chat_stream_request(messages, parser);
    auto *stream_parser = dynamic_cast<StreamResponseParser *>(parser.get());
    assert(stream_parser != nullptr);
    _http.post_sse(req.url, req.headers, req.body,
            [stream_parser, callback](const std::string &data_line) {
                auto token = stream_parser->parse_sse_token(data_line);
                if (!token.empty()) {
                    callback(token);
                }
            });
}

Message LlmClient::chat_with_tools(
        const std::vector<Message> &messages,
        const std::vector<ToolDef> &tools) {
    ResponseParserPtr parser;
    auto req = _provider->build_chat_with_tools_request(messages, tools, parser);
    auto response = _http.post(req.url, req.headers, req.body);
    auto *tool_parser = dynamic_cast<ToolResponseParser *>(parser.get());
    assert(tool_parser != nullptr);
    return tool_parser->parse(response);
}

} // namespace sw::sac
