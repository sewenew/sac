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

#ifndef SEWENEW_SAC_TOOLS_H
#define SEWENEW_SAC_TOOLS_H

#include <vector>
#include "sw/sac/agent.h"

namespace sw::sac::tools {

// Create all available tools
std::vector<Tool> make_all_tools();

// Individual tool creators
Tool make_read_tool();
Tool make_write_tool();
Tool make_edit_tool();
Tool make_glob_tool();
Tool make_grep_tool();
Tool make_ls_tool();

} // namespace sw::sac::tools

#endif // end SEWENEW_SAC_TOOLS_H
