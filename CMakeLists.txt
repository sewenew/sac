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
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(sac_lib STATIC
    src/sw/sac/agent.cpp
    src/sw/sac/http_client.cpp
    src/sw/sac/llm_client.cpp
    src/sw/sac/providers/anthropic_provider.cpp
    src/sw/sac/providers/openai_provider.cpp
    src/sw/sac/providers/moonshot_provider.cpp
)

target_include_directories(sac_lib PUBLIC src/)

target_compile_options(sac_lib PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(sac_lib PUBLIC
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

add_executable(sac src/sw/sac/main.cpp)

target_compile_options(sac PRIVATE
    -Wall -Wextra -Wpedantic
)

target_link_libraries(sac PRIVATE sac_lib)
