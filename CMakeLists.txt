cmake_minimum_required(VERSION 3.16)
project(sac VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    cpp_httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.3.tar.gz
    URL_HASH SHA256=a0567bcd6c3fe5cef1b329b96245119047f876b49e06cc129a36a7a8dffe173e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cpp_httplib)

add_library(sac_lib STATIC
    src/sw/sac/agent.cpp
    src/sw/sac/agent_request.cpp
    src/sw/sac/agent_job_queue.cpp
    src/sw/sac/agent_server.cpp
    src/sw/sac/channel_responder_registry.cpp
    src/sw/sac/channels/lark_responder.cpp
    src/sw/sac/http_client.cpp
    src/sw/sac/llm_client.cpp
    src/sw/sac/providers/anthropic_provider.cpp
    src/sw/sac/providers/openai_provider.cpp
    src/sw/sac/providers/moonshot_provider.cpp
    src/sw/sac/providers/llm_provider.cpp
    src/sw/sac/tools/tools.cpp
    src/sw/sac/tools/read.cpp
    src/sw/sac/tools/write.cpp
    src/sw/sac/tools/edit.cpp
    src/sw/sac/tools/glob.cpp
    src/sw/sac/tools/grep.cpp
    src/sw/sac/tools/ls.cpp
)

target_include_directories(sac_lib PUBLIC src/)

target_compile_options(sac_lib PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(sac_lib PUBLIC
    CURL::libcurl
    httplib::httplib
    nlohmann_json::nlohmann_json
)

add_executable(sac src/sw/sac/main.cpp)

target_compile_options(sac PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(sac PRIVATE sac_lib)

add_executable(provider_parse_test tests/provider_parse_test.cpp)

target_compile_options(provider_parse_test PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(provider_parse_test PRIVATE sac_lib)
