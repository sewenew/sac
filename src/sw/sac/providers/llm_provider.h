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

#ifndef SEWENEW_SAC_PROVIDER_LLM_PROVIDER_H
#define SEWENEW_SAC_PROVIDER_LLM_PROVIDER_H

#include <functional>
#include <string>
#include <vector>

namespace sw::sac {

class HttpClient;
struct Message;

using StreamCallback = std::function<void(const std::string &token)>;

// Strategy interface for LLM providers.
// Each concrete implementation knows how to:
//   - serialise messages to a provider-specific JSON request body
//   - set the correct auth headers
//   - parse the provider-specific response
// It does NOT own an HttpClient; LlmClient passes one in.
class LlmProvider {
public:
    LlmProvider() = default;
    LlmProvider(const LlmProvider &) = delete;
    LlmProvider &operator=(const LlmProvider &) = delete;
    virtual ~LlmProvider() = default;

    // Blocking chat. Returns the assistant reply text.
    virtual std::string chat(
            const std::vector<Message> &messages,
            HttpClient &http) = 0;

    // Streaming chat. callback receives successive tokens as they arrive.
    virtual void chat_stream(
            const std::vector<Message> &messages,
            HttpClient &http,
            StreamCallback callback) = 0;

    // Blocking embedding. Returns the embedding vector.
    virtual std::vector<float> embed(
            const std::string &text,
            HttpClient &http) = 0;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_PROVIDER_LLM_PROVIDER_H
