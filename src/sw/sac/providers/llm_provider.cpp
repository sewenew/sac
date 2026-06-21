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

#include "sw/sac/providers/llm_provider.h"

namespace sw::sac {

std::string optional_json_string(const nlohmann::json &json,
        const std::string &field) {
    if (!json.contains(field) || json.at(field).is_null()) {
        return "";
    }
    return json.at(field).get<std::string>();
}

}
