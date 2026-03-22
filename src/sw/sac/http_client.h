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

#ifndef SEWENEW_SAC_HTTP_CLIENT_H
#define SEWENEW_SAC_HTTP_CLIENT_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Forward-declare at global scope so ::curl_slist inside sw::sac resolves
// to the libcurl type, not an incomplete sw::sac::curl_slist.
struct curl_slist;

namespace sw::sac {

using HeaderMap = std::unordered_map<std::string, std::string>;

// Invoked once per SSE `data:` payload (prefix stripped, no trailing newline).
using SseCallback = std::function<void(const std::string &data)>;

// Abstract base — callers depend only on this interface.
// Swap the concrete impl (CurlHttpClient -> CppHttplibClient) without
// touching any provider or LlmClient code.
class HttpClient {
public:
    HttpClient() = default;
    HttpClient(const HttpClient &) = delete;
    HttpClient &operator=(const HttpClient &) = delete;
    virtual ~HttpClient() = default;

    // Blocking POST. Throws HttpError on non-2xx or transport failure.
    virtual std::string post(
            const std::string &url,
            const HeaderMap &headers,
            const std::string &body) = 0;

    // Streaming POST over SSE. callback is invoked once per `data:` payload.
    // Throws HttpError on non-2xx or transport failure.
    virtual void post_sse(
            const std::string &url,
            const HeaderMap &headers,
            const std::string &body,
            SseCallback callback) = 0;
};

// libcurl-backed implementation.
class CurlHttpClient : public HttpClient {
public:
    CurlHttpClient();

    CurlHttpClient(const CurlHttpClient &) = delete;
    CurlHttpClient &operator=(const CurlHttpClient &) = delete;
    CurlHttpClient(CurlHttpClient &&) = delete;
    CurlHttpClient &operator=(CurlHttpClient &&) = delete;

    ~CurlHttpClient() override;

    std::string post(
            const std::string &url,
            const HeaderMap &headers,
            const std::string &body) override;

    void post_sse(
            const std::string &url,
            const HeaderMap &headers,
            const std::string &body,
            SseCallback callback) override;

private:
    struct CurlHandleDeleter {
        void operator()(void *handle) const noexcept;
    };
    using CurlHandleUPtr = std::unique_ptr<void, CurlHandleDeleter>;

    struct CurlListDeleter {
        void operator()(::curl_slist *list) const noexcept;
    };
    using CurlListUPtr = std::unique_ptr<::curl_slist, CurlListDeleter>;

    // SSE parser state passed as libcurl WRITEFUNCTION userdata.
    struct SseState {
        std::string buffer;
        SseCallback callback;
    };

    // Shared CURL* setup for both post() and post_sse().
    CurlHandleUPtr _make_handle(
            const std::string &url,
            const HeaderMap &headers,
            const std::string &body,
            CurlListUPtr &out_headers);

    // Validates curl result and HTTP status; throws on error.
    void _check_result(void *curl, int curl_code, const std::string &url) const;

    static size_t _write_callback(
            char *ptr, size_t size, size_t nmemb, void *userdata);

    static size_t _sse_write_callback(
            char *ptr, size_t size, size_t nmemb, void *userdata);

    // Dispatches complete SSE event blocks from SseState::buffer.
    static void _dispatch_events(SseState &state);
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_HTTP_CLIENT_H
