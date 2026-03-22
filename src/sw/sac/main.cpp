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

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "sw/sac/http_client.h"
#include "sw/sac/llm_client.h"
#include "sw/sac/errors.h"

int main() {
    const char *api_key = std::getenv("OPENAI_API_KEY");
    if (api_key == nullptr) {
        std::cerr << "OPENAI_API_KEY is not set\n";
        return 1;
    }
    const char *base_url = std::getenv("OPENAI_BASE_URL");
    if (base_url == nullptr) {
        std::cerr << "OPENAI_BASE_URL is not set\n";
        return 1;
    }
    const char *model = std::getenv("OPENAI_MODEL");
    if (model == nullptr) {
        std::cerr << "OPENAI_MODEL is not set\n";
        return 1;
    }

    sw::sac::CurlHttpClient http;
    sw::sac::LlmClient client(
        sw::sac::make_openai_provider({base_url, api_key, model}),
        http);

    std::vector<sw::sac::Message> messages = {
        {sw::sac::Role::USER, "Say hello in one sentence."},
    };

    std::cout << "--- blocking ---\n";
    try {
        std::cout << client.chat(messages) << "\n";
    } catch (const sw::sac::Error &e) {
        std::cerr << e.what() << std::endl;
    }

    std::cout << "--- streaming ---\n";
    try {
        client.chat_stream(messages, [](const std::string &token) {
            std::cout << token << std::flush;
        });
    } catch (const sw::sac::Error &e) {
        std::cerr << e.what() << std::endl;
    }
    std::cout << "\n";

    return 0;
}
