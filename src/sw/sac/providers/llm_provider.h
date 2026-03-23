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
#include <memory>
#include <string>
#include <vector>

#include "sw/sac/http_client.h"

namespace sw::sac {

struct Message;
struct ToolDef;

using StreamCallback = std::function<void(const std::string &token)>;

// Request bundle returned by a provider for a given operation.
struct ProviderRequest {
    std::string url;
    HeaderMap headers;
    std::string body;
};

// Response parser interface — providers implement this to turn raw HTTP
// response bodies into domain objects.
class ResponseParser {
public:
    virtual ~ResponseParser() = default;
};

using ResponseParserPtr = std::unique_ptr<ResponseParser>;

// Parser for blocking chat responses.
class ChatResponseParser : public ResponseParser {
public:
    virtual std::string parse(const std::string &response_body) = 0;
};

// Parser for streaming chat responses.
class StreamResponseParser : public ResponseParser {
public:
    // Called for each SSE data line. Returns the token (may be empty).
    virtual std::string parse_sse_token(const std::string &data_line) = 0;
};

// Parser for tool-augmented chat responses.
class ToolResponseParser : public ResponseParser {
public:
    virtual Message parse(const std::string &response_body) = 0;
};

// Strategy interface for LLM providers.
// Each concrete implementation knows how to:
//   - build a provider-specific request (URL, headers, body)
//   - create a parser that can interpret the response
// The actual HTTP call is made by LlmClient.
class LlmProvider {
public:
    LlmProvider() = default;
    LlmProvider(const LlmProvider &) = delete;
    LlmProvider &operator=(const LlmProvider &) = delete;
    virtual ~LlmProvider() = default;

    // Build a blocking chat request. The returned parser can extract the reply
    // text from the response body via parse_chat_response().
    virtual ProviderRequest build_chat_request(
            const std::vector<Message> &messages,
            ResponseParserPtr &parser) = 0;

    // Build a streaming chat request. The returned parser can extract tokens
    // from SSE data lines via parse_sse_token().
    virtual ProviderRequest build_chat_stream_request(
            const std::vector<Message> &messages,
            ResponseParserPtr &parser) = 0;

    // One tool-augmented turn. Returns the assistant Message (content or tool_calls).
    // Providers that do not support tool use should override and throw ApiError.
    virtual ProviderRequest build_chat_with_tools_request(
            const std::vector<Message> &messages,
            const std::vector<ToolDef> &tools,
            ResponseParserPtr &parser) = 0;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_PROVIDER_LLM_PROVIDER_H
