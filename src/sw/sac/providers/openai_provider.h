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

#ifndef SEWENEW_SAC_PROVIDER_OPENAI_PROVIDER_H
#define SEWENEW_SAC_PROVIDER_OPENAI_PROVIDER_H

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "sw/sac/http_client.h"
#include "sw/sac/llm_client.h"
#include "sw/sac/providers/llm_provider.h"

namespace sw::sac {

struct OpenAiOptions {
    std::string base_url = "https://api.openai.com/v1";
    std::string api_key;
    std::string model;
};

// Handles any OpenAI-compatible API: OpenAI, Volcengine ARK, Kimi (Moonshot AI).
// The wire format is identical across all three; only OpenAiOptions.base_url,
// api_key, and model differ.
//
// Chat endpoint:      <base_url>/chat/completions
// Embedding endpoint: <base_url>/embeddings
class OpenAiProvider : public LlmProvider {
public:
    explicit OpenAiProvider(const OpenAiOptions &opts);

    OpenAiProvider(const OpenAiProvider &) = delete;
    OpenAiProvider &operator=(const OpenAiProvider &) = delete;

    ~OpenAiProvider() override = default;

    std::string chat(
            const std::vector<Message> &messages,
            HttpClient &http) override;

    void chat_stream(
            const std::vector<Message> &messages,
            HttpClient &http,
            StreamCallback callback) override;

    std::vector<float> embed(
            const std::string &text,
            HttpClient &http) override;

    Message chat_with_tools(
            const std::vector<Message> &messages,
            const std::vector<ToolDef> &tools,
            HttpClient &http) override;

private:
    // Returns {"Authorization": "Bearer <key>", "Content-Type": "application/json"}.
    HeaderMap _auth_headers() const;

    nlohmann::json _build_chat_request(
            const std::vector<Message> &messages, bool stream) const;

    nlohmann::json _build_embed_request(const std::string &text) const;

    std::string _extract_content(const std::string &response_json) const;

    // Returns the incremental token from one SSE data line, or "" if the chunk
    // carries no text (role-only / finish_reason chunks).
    std::string _extract_sse_token(const std::string &data_line) const;

    std::vector<float> _extract_embedding(const std::string &response_json) const;

    OpenAiOptions _opts;
};

} // namespace sw::sac

#endif //end SEWENEW_SAC_PROVIDER_OPENAI_PROVIDER_H
