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

#ifndef SEWENEW_SAC_CHANNEL_RESPONDER_H
#define SEWENEW_SAC_CHANNEL_RESPONDER_H

#include <string>

#include "sw/sac/agent_request.h"

namespace sw::sac {

class ChannelResponder {
public:
    ChannelResponder() = default;
    ChannelResponder(const ChannelResponder &) = delete;
    ChannelResponder &operator=(const ChannelResponder &) = delete;
    virtual ~ChannelResponder() = default;

    virtual void send_text(const ReplyTarget &target,
            const std::string &text) = 0;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_CHANNEL_RESPONDER_H
