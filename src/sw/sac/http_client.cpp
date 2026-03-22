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

#include "sw/sac/http_client.h"

#include <cassert>
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>

#include "sw/sac/errors.h"

namespace sw::sac {

// ---------------------------------------------------------------------------
// CurlHandleDeleter / CurlListDeleter
// ---------------------------------------------------------------------------

void CurlHttpClient::CurlHandleDeleter::operator()(void *handle) const noexcept {
    if (handle != nullptr) {
        curl_easy_cleanup(static_cast<CURL *>(handle));
    }
}

void CurlHttpClient::CurlListDeleter::operator()(::curl_slist *list) const noexcept {
    if (list != nullptr) {
        curl_slist_free_all(list);
    }
}

// ---------------------------------------------------------------------------
// CurlHttpClient
// ---------------------------------------------------------------------------

class CurlInit {
public:
    CurlInit() {
        // curl_global_init is not thread-safe; call once from the main thread
        // before constructing any CurlHttpClient.
        // We call it here for convenience in single-threaded usage; callers that
        // need strict control should call curl_global_init themselves.
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~CurlInit() {
        curl_global_cleanup();
    }
};

CurlHttpClient::CurlHttpClient() {
    static CurlInit init;
}

CurlHttpClient::~CurlHttpClient() {}

// ---------------------------------------------------------------------------
// post — blocking
// ---------------------------------------------------------------------------

std::string CurlHttpClient::post(
        const std::string &url,
        const HeaderMap &headers,
        const std::string &body) {
    CurlListUPtr header_list;
    auto curl = _make_handle(url, headers, body, header_list);

    std::string response;
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, _write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl.get());
    _check_result(curl.get(), static_cast<int>(res), url);

    return response;
}

// ---------------------------------------------------------------------------
// post_sse — streaming
// ---------------------------------------------------------------------------

void CurlHttpClient::post_sse(
        const std::string &url,
        const HeaderMap &headers,
        const std::string &body,
        SseCallback callback) {
    CurlListUPtr header_list;
    auto curl = _make_handle(url, headers, body, header_list);

    SseState state;
    state.callback = std::move(callback);

    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, _sse_write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &state);

    CURLcode res = curl_easy_perform(curl.get());
    _check_result(curl.get(), static_cast<int>(res), url);
}

// ---------------------------------------------------------------------------
// _make_handle — shared CURL* setup
// ---------------------------------------------------------------------------

CurlHttpClient::CurlHandleUPtr CurlHttpClient::_make_handle(
        const std::string &url,
        const HeaderMap &headers,
        const std::string &body,
        CurlListUPtr &out_headers) {
    CURL *raw = curl_easy_init();
    if (raw == nullptr) {
        throw HttpError(0, "curl_easy_init() failed");
    }
    CurlHandleUPtr curl(raw);

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

    // Build header list.
    ::curl_slist *list = nullptr;
    for (const auto &kv : headers) {
        std::string header = kv.first + ": " + kv.second;
        list = curl_slist_append(list, header.c_str());
    }
    out_headers.reset(list);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, out_headers.get());

    // Follow redirects and enforce TLS.
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);

    return curl;
}

// ---------------------------------------------------------------------------
// _check_result
// ---------------------------------------------------------------------------

void CurlHttpClient::_check_result(void *curl, int curl_code, const std::string &url) const {
    if (curl_code != CURLE_OK) {
        throw HttpError(0,
                std::string("curl error for ") + url + ": " +
                curl_easy_strerror(static_cast<CURLcode>(curl_code)));
    }

    long http_code = 0;
    curl_easy_getinfo(static_cast<CURL *>(curl), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 300) {
        throw HttpError(http_code,
                "HTTP " + std::to_string(http_code) + " from " + url);
    }
}

// ---------------------------------------------------------------------------
// _write_callback — accumulates into std::string
// ---------------------------------------------------------------------------

size_t CurlHttpClient::_write_callback(
        char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *response = static_cast<std::string *>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

// ---------------------------------------------------------------------------
// _sse_write_callback — feeds the SSE state machine
// ---------------------------------------------------------------------------

size_t CurlHttpClient::_sse_write_callback(
        char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *state = static_cast<SseState *>(userdata);
    state->buffer.append(ptr, size * nmemb);
    _dispatch_events(*state);
    return size * nmemb;
}

// ---------------------------------------------------------------------------
// _dispatch_events — SSE line/event parser
//
// SSE wire format:
//   data: <json>\n
//   \n
// Events are delimited by a blank line ("\n\n").  Within an event block,
// lines beginning with "data: " carry the payload.  "[DONE]" is the stream
// terminator used by OpenAI-compatible APIs and is silently skipped.
// ---------------------------------------------------------------------------

void CurlHttpClient::_dispatch_events(SseState &state) {
    while (true) {
        auto pos = state.buffer.find("\n\n");
        if (pos == std::string::npos) {
            break;
        }

        std::string event_block = state.buffer.substr(0, pos);
        state.buffer.erase(0, pos + 2);

        // Iterate lines within the event block.
        std::istringstream stream(event_block);
        std::string line;
        while (std::getline(stream, line)) {
            // Strip trailing \r for CRLF line endings.
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            const std::string prefix = "data: ";
            if (line.rfind(prefix, 0) != 0) {
                // Not a data line (event:, id:, retry:, comment).
                continue;
            }

            std::string payload = line.substr(prefix.size());
            if (payload == "[DONE]") {
                continue;
            }

            state.callback(payload);
        }
    }
}

} // namespace sw::sac
