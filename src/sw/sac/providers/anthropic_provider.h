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

#include "sw/sac/http_client.h"
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
// Embedding:     not supported — embed() throws ApiError.
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

    std::string chat(
            const std::vector<Message> &messages,
            HttpClient &http) override;

    void chat_stream(
            const std::vector<Message> &messages,
            HttpClient &http,
            StreamCallback callback) override;

    // Anthropic does not expose an embeddings endpoint.
    [[noreturn]] std::vector<float> embed(
            const std::string &text,
            HttpClient &http) override;

private:
    // Returns {"x-api-key": ..., "anthropic-version": ..., "Content-Type": ...}.
    HeaderMap _auth_headers() const;

    // Pulls SYSTEM role out to the top-level "system" field; remaining messages
    // go into the "messages" array.
    nlohmann::json _build_request(
            const std::vector<Message> &messages, bool stream) const;

    std::string _extract_content(const std::string &response_json) const;

    // Returns the incremental token from one SSE data line, or "" if the event
    // is not of type "content_block_delta".
    std::string _extract_sse_token(const std::string &data_line) const;

    AnthropicOptions _opts;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_PROVIDER_ANTHROPIC_PROVIDER_H
