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

#include "sw/sac/channel_responder_registry.h"

#include <stdexcept>
#include <utility>

namespace sw::sac {

void ChannelResponderRegistry::add(const std::string &platform,
        std::unique_ptr<ChannelResponder> responder) {
    if (platform.empty()) {
        throw std::invalid_argument("platform is required");
    }
    if (responder == nullptr) {
        throw std::invalid_argument("responder is required");
    }
    _responders[platform] = std::move(responder);
}

ChannelResponder &ChannelResponderRegistry::get(const std::string &platform) const {
    auto iter = _responders.find(platform);
    if (iter == _responders.end()) {
        throw std::runtime_error("missing channel responder for platform: " + platform);
    }
    return *iter->second;
}

} // namespace sw::sac
