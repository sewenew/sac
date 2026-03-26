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

#include "sw/sac/tools/tools.h"

namespace sw::sac::tools {

std::vector<Tool> make_all_tools() {
    return {
        make_read_tool(),
        make_write_tool(),
        make_edit_tool(),
        make_glob_tool(),
        make_grep_tool(),
        make_ls_tool()
    };
}

} // namespace sw::sac::tools
