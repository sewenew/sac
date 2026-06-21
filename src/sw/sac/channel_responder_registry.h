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

#ifndef SEWENEW_SAC_CHANNEL_RESPONDER_REGISTRY_H
#define SEWENEW_SAC_CHANNEL_RESPONDER_REGISTRY_H

#include <memory>
#include <string>
#include <unordered_map>

#include "sw/sac/channel_responder.h"

namespace sw::sac {

class ChannelResponderRegistry {
public:
    ChannelResponderRegistry() = default;
    ChannelResponderRegistry(const ChannelResponderRegistry &) = delete;
    ChannelResponderRegistry &operator=(const ChannelResponderRegistry &) = delete;

    void add(const std::string &platform,
            std::unique_ptr<ChannelResponder> responder);

    ChannelResponder &get(const std::string &platform) const;

private:
    std::unordered_map<std::string, std::unique_ptr<ChannelResponder>> _responders;
};

} // namespace sw::sac

#endif // end SEWENEW_SAC_CHANNEL_RESPONDER_REGISTRY_H
