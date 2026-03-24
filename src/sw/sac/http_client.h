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

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

// Forward-declare at global scope so ::curl_slist inside sw::sac resolves
// to the libcurl type, not an incomplete sw::sac::curl_slist.
struct curl_slist;

namespace sw::sac {

using HeaderMap = std::unordered_map<std::string, std::string>;

// Invoked once per SSE `data:` payload (prefix stripped, no trailing newline).
using SseCallback = std::function<void(const std::string &data)>;

// Options for HTTP connection pool.
struct HttpConnectionPoolOptions {
    // Max number of connections per pool, including both in-use and idle ones.
    std::size_t size = 10;

    // Max time to wait for a connection. 0ms means client waits forever.
    std::chrono::milliseconds wait_timeout{0};

    // Max lifetime of a connection. 0ms means we never expire the connection.
    std::chrono::milliseconds connection_lifetime{0};

    // Max idle time of a connection. 0ms means we never expire the connection.
    std::chrono::milliseconds connection_idle_time{0};
};

// Key for identifying a connection pool. Connections with the same key can be reused.
// Contains all options that affect the underlying TCP/TLS connection.
struct PoolKey {
    std::string url_base;        // scheme://host:port (e.g., "https://api.anthropic.com:443")
    std::string proxy_url;       // proxy URL (empty if no proxy)
};

inline bool operator==(const PoolKey &lhs, const PoolKey &rhs) {
    return lhs.url_base == rhs.url_base && lhs.proxy_url == rhs.proxy_url;
}

struct PoolKeyHash {
    std::size_t operator()(const PoolKey &pool_key) const noexcept {
        auto url_base_hash = std::hash<std::string>{}(pool_key.url_base);
        auto proxy_url_hash = std::hash<std::string>{}(pool_key.proxy_url);
        return url_base_hash ^ (proxy_url_hash << 1);
    }
};

// Forward declaration
class HttpConnection;
class HttpConnectionPool;

using HttpConnectionUPtr = std::unique_ptr<HttpConnection>;
using HttpConnectionPoolSPtr = std::shared_ptr<HttpConnectionPool>;

class HttpConnection {
public:
    explicit HttpConnection(void *curl);
    ~HttpConnection() = default;

    HttpConnection(HttpConnection &&) = default;
    HttpConnection& operator=(HttpConnection &&) = default;
    HttpConnection(const HttpConnection &) = delete;
    HttpConnection& operator=(const HttpConnection &) = delete;

    void* get() const noexcept {
        return _curl.get();
    }

    std::chrono::steady_clock::time_point last_active_time() const noexcept {
        return _last_active_time;
    }

    std::chrono::steady_clock::time_point create_time() const noexcept {
        return _create_time;
    }

private:
    void _touch();

    struct CurlDeleter {
        void operator()(void *c);
    };
    using CurlUPtr = std::unique_ptr<void, CurlDeleter>;

    CurlUPtr _curl;
    std::chrono::steady_clock::time_point _create_time{};
    std::chrono::steady_clock::time_point _last_active_time{};
};

// Thread-safe pool of reusable CURL connections.
class HttpConnectionPool {
public:
    HttpConnectionPool(const PoolKey &key,
                       const HttpConnectionPoolOptions &pool_opts);

    HttpConnectionPool(const HttpConnectionPool &) = delete;
    HttpConnectionPool &operator=(const HttpConnectionPool &) = delete;

    ~HttpConnectionPool() = default;

    HttpConnectionUPtr fetch();

    // Release a CURL handle back to the pool.
    void release(HttpConnectionUPtr &&connection);

    const PoolKey &key() const noexcept { return _key; }

private:
    HttpConnectionUPtr _fetch(std::unique_lock<std::mutex> &lock);

    HttpConnectionUPtr _fetch_from_pool();

    void _wait_for_connection(std::unique_lock<std::mutex> &lock);

    bool _need_reconnect(HttpConnection *conn) const;

    PoolKey _key;

    HttpConnectionPoolOptions _pool_opts;

    std::deque<HttpConnectionUPtr> _pool;
    
    std::size_t _used_connections = 0;

    std::mutex _mutex;
    std::condition_variable _cv;
};

// RAII wrapper for fetching and releasing a connection from the pool.
class HttpConnectionHandle {
public:
    explicit HttpConnectionHandle(HttpConnectionPool &pool) :
        _pool(&pool), _connection(pool.fetch()) {
        assert(_connection != nullptr);
    }

    HttpConnectionHandle(const HttpConnectionHandle &) = delete;
    HttpConnectionHandle &operator=(const HttpConnectionHandle &) = delete;

    ~HttpConnectionHandle() noexcept;

    void* get() const noexcept { return _connection->get(); }

private:
    HttpConnectionPool *_pool = nullptr;
    HttpConnectionUPtr _connection = nullptr;
};

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

// libcurl-backed implementation with connection pooling.
class CurlHttpClient : public HttpClient {
public:
    // Constructor with custom pool options.
    explicit CurlHttpClient(const HttpConnectionPoolOptions &pool_opts = {});

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

    // Set proxy for all connections.
    void set_proxy(const std::string &proxy_url) { _proxy_url = proxy_url; }

private:
    // SSE parser state passed as libcurl WRITEFUNCTION userdata.
    struct SseState {
        std::string buffer;
        SseCallback callback;
    };

    struct CurlListDeleter {
        void operator()(::curl_slist *list) const noexcept;
    };
    using CurlListUPtr = std::unique_ptr<::curl_slist, CurlListDeleter>;

    // Extract scheme://host:port from a full URL.
    static std::string _extract_url_base(const std::string &url);

    // Get or create a connection pool for the given key.
    HttpConnectionPoolSPtr _get_or_create_pool(const PoolKey &key);

    // Set request-specific options on a CURL handle.
    void _set_request_options(
            void *curl,
            const std::string &url,
            const HeaderMap &headers,
            const std::string &body,
            ::curl_slist *header_list,
            const PoolKey &key);

    // Validates curl result and HTTP status; throws on error.
    void _check_result(void *curl, int curl_code, const std::string &url) const;

    static size_t _write_callback(
            char *ptr, size_t size, size_t nmemb, void *userdata);

    static size_t _sse_write_callback(
            char *ptr, size_t size, size_t nmemb, void *userdata);

    // Dispatches complete SSE event blocks from SseState::buffer.
    static void _dispatch_events(SseState &state);

    // Pool options
    HttpConnectionPoolOptions _pool_opts;

    // Connection pools indexed by PoolKey.
    std::unordered_map<PoolKey, HttpConnectionPoolSPtr, PoolKeyHash> _pools;
    std::mutex _pools_mutex;

    // Connection configuration (part of PoolKey).
    std::string _proxy_url;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_HTTP_CLIENT_H
