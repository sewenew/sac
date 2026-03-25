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

#include "http_client.h"
#include "sw/sac/errors.h"

namespace sw::sac {

void HttpConnection::CurlDeleter::operator()(void *c) {
    if (c != nullptr) {
        curl_easy_cleanup(static_cast<CURL *>(c));
    }
}

HttpConnectionHandle::~HttpConnectionHandle() noexcept {
    if (_pool && _connection) {
        _pool->release(std::move(_connection));
    }
}

class CurlInit {
public:
    CurlInit() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~CurlInit() {
        curl_global_cleanup();
    }
};

HttpConnection::HttpConnection(void *c) :
    _curl(c),
    _create_time(std::chrono::steady_clock::now()),
    _last_active_time(_create_time) {
    if (_curl == nullptr) {
        throw HttpError(0, "cannot create HttpConnection with null curl");
    }
}

void HttpConnection::_touch() {
    _last_active_time = std::chrono::steady_clock::now();
}

HttpConnectionPool::HttpConnectionPool(
        const PoolKey &key,
        const HttpConnectionPoolOptions &pool_opts)
    : _key(key), _pool_opts(pool_opts) {
    if (_pool_opts.size == 0) {
        throw HttpError(0, "Cannot create an empty connection pool");
    }
}

HttpConnectionUPtr HttpConnectionPool::fetch() {
    std::unique_lock<std::mutex> lock(_mutex);

    auto c = _fetch(lock);

    assert(c);

    lock.unlock();

    if (_need_reconnect(c.get())) {
        auto new_curl = curl_easy_init();
        if (new_curl == nullptr) {
            release(std::move(c));
            throw HttpError(0, "curl_easy_init() failed");
        }
        c = std::make_unique<HttpConnection>(new_curl);
    }

    return c;
}

HttpConnectionUPtr HttpConnectionPool::_fetch(std::unique_lock<std::mutex> &lock) {
    if (_pool.empty()) {
        if (_used_connections == _pool_opts.size) {
            _wait_for_connection(lock);
        } else {
            // Lazily create a new connection.
            auto c = curl_easy_init();
            if (c == nullptr) {
                throw HttpError(0, "curl_easy_init() failed");
            }
            ++_used_connections;
            return std::make_unique<HttpConnection>(c);
        }
    }

    // Pool is not empty.
    return _fetch_from_pool();
}

HttpConnectionUPtr HttpConnectionPool::_fetch_from_pool() {
    assert(!_pool.empty());

    auto c = std::move(_pool.front());
    _pool.pop_front();

    return c;
}

void HttpConnectionPool::_wait_for_connection(std::unique_lock<std::mutex> &lock) {
    auto timeout = _pool_opts.wait_timeout;
    if (timeout > std::chrono::milliseconds(0)) {
        if (!_cv.wait_for(lock, timeout,
                          [this] { return !this->_pool.empty(); })) {
            throw HttpError(0, "Failed to fetch a connection in " +
                               std::to_string(timeout.count()) + " milliseconds");
        }
    } else {
        _cv.wait(lock, [this] { return !this->_pool.empty(); });
    }
}

void HttpConnectionPool::release(HttpConnectionUPtr &&conn) {
    if (!conn) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pool.push_back(std::move(conn));
    }

    _cv.notify_one();
}

bool HttpConnectionPool::_need_reconnect(HttpConnection *conn) const {
    if (_pool_opts.connection_lifetime > std::chrono::milliseconds(0)) {
        if (std::chrono::steady_clock::now() - conn->create_time() > _pool_opts.connection_lifetime) {
            return true;
        }
    }

    if (_pool_opts.connection_idle_time > std::chrono::milliseconds(0)) {
        if (std::chrono::steady_clock::now() - conn->last_active_time() > _pool_opts.connection_idle_time) {
            return true;
        }
    }

    return false;
}

void CurlHttpClient::CurlListDeleter::operator()(::curl_slist *list) const noexcept {
    if (list != nullptr) {
        curl_slist_free_all(list);
    }
}

CurlHttpClient::CurlHttpClient(const HttpConnectionPoolOptions &pool_opts)
    : _pool_opts(pool_opts) {
    static CurlInit init;
}

CurlHttpClient::~CurlHttpClient() {
    // Cleanup all connection pools.
    // Note: CURL handles will be cleaned up when pools are destroyed.
}

// Extract scheme://host:port from a full URL.
// Examples:
//   "https://api.anthropic.com/v1/messages" -> "https://api.anthropic.com:443"
//   "http://localhost:8080/path" -> "http://localhost:8080"
std::string CurlHttpClient::_extract_url_base(const std::string &url) {
    // Find scheme
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return url;  // Invalid URL, return as-is
    }

    std::string scheme = url.substr(0, scheme_end);
    std::size_t host_start = scheme_end + 3;

    // Find path start
    auto path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        // No path, use entire URL
        path_start = url.size();
    }

    // Extract host:port part
    std::string host_port = url.substr(host_start, path_start - host_start);

    // Check if port is already specified
    if (host_port.find(':') != std::string::npos) {
        return scheme + "://" + host_port;
    }

    // Add default port
    std::string default_port;
    if (scheme == "https") {
        default_port = ":443";
    } else if (scheme == "http") {
        default_port = ":80";
    }

    return scheme + "://" + host_port + default_port;
}

HttpConnectionPoolSPtr CurlHttpClient::_get_or_create_pool(const PoolKey &key) {
    {
        std::lock_guard<std::mutex> lock(_pools_mutex);
        auto it = _pools.find(key);
        if (it != _pools.end()) {
            return it->second;
        }
    }

    // Pool doesn't exist, create it.
    auto pool = std::make_shared<HttpConnectionPool>(key, _pool_opts);

    {
        std::lock_guard<std::mutex> lock(_pools_mutex);
        // Double-check in case another thread created it.
        auto it = _pools.find(key);
        if (it != _pools.end()) {
            return it->second;
        }
        _pools[key] = pool;
    }

    return pool;
}

void CurlHttpClient::_set_request_options(
        void *curl,
        const std::string &url,
        const HeaderMap &/*headers*/,
        const std::string &body,
        ::curl_slist *header_list,
        const PoolKey &key) {
    CURL *raw = static_cast<CURL *>(curl);

    // Set connection-specific options (from PoolKey)
    if (!key.proxy_url.empty()) {
        curl_easy_setopt(raw, CURLOPT_PROXY, key.proxy_url.c_str());
    }
    //curl_easy_setopt(raw, CURLOPT_SSL_VERIFYPEER, key.ssl_verifypeer ? 1L : 0L);
    //curl_easy_setopt(raw, CURLOPT_SSL_VERIFYHOST, static_cast<long>(key.ssl_verifyhost));
    curl_easy_setopt(raw, CURLOPT_FOLLOWLOCATION, 1L);

    // Set URL (full URL including path)
    curl_easy_setopt(raw, CURLOPT_URL, url.c_str());

    // Set request method and body
    curl_easy_setopt(raw, CURLOPT_POST, 1L);
    curl_easy_setopt(raw, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(raw, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

    // Set headers
    curl_easy_setopt(raw, CURLOPT_HTTPHEADER, header_list);
}

// ---------------------------------------------------------------------------
// post — blocking
// ---------------------------------------------------------------------------

std::string CurlHttpClient::post(
        const std::string &url,
        const HeaderMap &headers,
        const std::string &body) {
    // Extract URL base for pool key
    auto url_base = _extract_url_base(url);

    // Build pool key
    PoolKey key;
    key.url_base = std::move(url_base);
    key.proxy_url = _proxy_url;

    // Get or create connection pool
    auto pool = _get_or_create_pool(key);

    // Fetch connection from pool
    HttpConnectionHandle handle(*pool);
    void *curl = handle.get();

    // Build header list
    ::curl_slist *list = nullptr;
    for (const auto &kv : headers) {
        std::string header = kv.first + ": " + kv.second;
        list = curl_slist_append(list, header.c_str());
    }
    CurlListUPtr header_list(list);

    // Set request-specific options
    _set_request_options(curl, url, headers, body, header_list.get(), key);

    // Set response callback
    std::string response;
    curl_easy_setopt(static_cast<CURL *>(curl), CURLOPT_WRITEFUNCTION, _write_callback);
    curl_easy_setopt(static_cast<CURL *>(curl), CURLOPT_WRITEDATA, &response);

    // Perform request
    CURLcode res = curl_easy_perform(static_cast<CURL *>(curl));
    _check_result(curl, static_cast<int>(res), url, response);

    // Connection is automatically released when handle goes out of scope
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
    // Extract URL base for pool key
    auto url_base = _extract_url_base(url);

    // Build pool key
    PoolKey key;
    key.url_base = std::move(url_base);
    key.proxy_url = _proxy_url;

    // Get or create connection pool
    auto pool = _get_or_create_pool(key);

    // Fetch connection from pool
    HttpConnectionHandle handle(*pool);
    void *curl = handle.get();

    // Build header list
    ::curl_slist *list = nullptr;
    for (const auto &kv : headers) {
        std::string header = kv.first + ": " + kv.second;
        list = curl_slist_append(list, header.c_str());
    }
    CurlListUPtr header_list(list);

    // Set request-specific options
    _set_request_options(curl, url, headers, body, header_list.get(), key);

    // Set SSE callback
    SseState state;
    state.callback = std::move(callback);

    curl_easy_setopt(static_cast<CURL *>(curl), CURLOPT_WRITEFUNCTION, _sse_write_callback);
    curl_easy_setopt(static_cast<CURL *>(curl), CURLOPT_WRITEDATA, &state);

    // Perform request
    CURLcode res = curl_easy_perform(static_cast<CURL *>(curl));
    _check_result(curl, static_cast<int>(res), url, state.buffer);
}

// ---------------------------------------------------------------------------
// _check_result
// ---------------------------------------------------------------------------

void CurlHttpClient::_check_result(void *conn, int curl_code,
                                   const std::string &url,
                                   const std::string &response) const {
    if (curl_code != CURLE_OK) {
        throw HttpError(0,
                std::string("curl error for ") + url + ": " +
                curl_easy_strerror(static_cast<CURLcode>(curl_code)));
    }

    long http_code = 0;
    curl_easy_getinfo(static_cast<CURL *>(conn), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code < 200 || http_code >= 300) {
        throw HttpError(http_code,
                "HTTP " + std::to_string(http_code) + " from " + url +
                ", response: " + response);
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
