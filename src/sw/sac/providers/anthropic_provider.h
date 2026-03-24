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

#ifndef SEWENEW_SAC_PROVIDER_ANTHROPIC_PROVIDER_H
#define SEWENEW_SAC_PROVIDER_ANTHROPIC_PROVIDER_H

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "sw/sac/llm_client.h"
#include "sw/sac/providers/llm_provider.h"

namespace sw::sac {

struct AnthropicOptions {
    std::string base_url = "https://api.anthropic.com/v1";
    std::string api_key;
    std::string model;
};

// Anthropic Messages API.
// Chat endpoint: <base_url>/messages  (default base_url: https://api.anthropic.com/v1)
//
// Wire format differences from OpenAI:
//   - Auth: "x-api-key" header + "anthropic-version" header (no "Authorization: Bearer")
//   - System message: top-level "system" field, not a messages array entry
//   - max_tokens is required
//   - Non-stream response: content[0].text
//   - SSE delta event type: "content_block_delta" with delta.text
class AnthropicProvider : public LlmProvider {
public:
    static constexpr int DEFAULT_MAX_TOKENS = 4096;

    explicit AnthropicProvider(const AnthropicOptions &opts);

    AnthropicProvider(const AnthropicProvider &) = delete;
    AnthropicProvider &operator=(const AnthropicProvider &) = delete;

    ~AnthropicProvider() override = default;

    ProviderRequest build_chat_request(
            const std::vector<Message> &messages) override;

    ProviderRequest build_chat_stream_request(
            const std::vector<Message> &messages) override;

    // Anthropic does not support tool use.
    ProviderRequest build_chat_with_tools_request(
            const std::vector<Message> &messages,
            const std::vector<ToolDef> &tools) override;

    std::string parse_chat_response(const std::string &response_body) override;

    std::string parse_chat_stream_response(const std::string &data_line) override;

    // Anthropic does not support tool use.
    Message parse_tool_response(const std::string &response_body) override;

private:
    // Returns {"x-api-key": ..., "anthropic-version": ..., "Content-Type": ...}.
    HeaderMap _auth_headers() const;

    // Pulls SYSTEM role out to the top-level "system" field; remaining messages
    // go into the "messages" array.
    nlohmann::json _build_request(
            const std::vector<Message> &messages, bool stream) const;

    AnthropicOptions _opts;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_PROVIDER_ANTHROPIC_PROVIDER_H
