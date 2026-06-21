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

#ifndef SEWENEW_SAC_CHANNELS_LARK_RESPONDER_H
#define SEWENEW_SAC_CHANNELS_LARK_RESPONDER_H

#include <chrono>
#include <mutex>
#include <string>

#include "sw/sac/channel_responder.h"
#include "sw/sac/http_client.h"

namespace sw::sac {

struct LarkResponderOptions {
    std::string app_id;
    std::string app_secret;
    std::string api_base_url = "https://open.feishu.cn";
};

class LarkResponder : public ChannelResponder {
public:
    LarkResponder(const LarkResponderOptions &opts, HttpClient &http);

    LarkResponder(const LarkResponder &) = delete;
    LarkResponder &operator=(const LarkResponder &) = delete;

    void send_text(const ReplyTarget &target,
            const std::string &text) override;

private:
    std::string _tenant_access_token();
    void _refresh_token();

    LarkResponderOptions _opts;
    HttpClient &_http;
    std::mutex _mutex;
    std::string _tenant_access_token_value;
    std::chrono::steady_clock::time_point _token_expires_at{};
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_CHANNELS_LARK_RESPONDER_H
