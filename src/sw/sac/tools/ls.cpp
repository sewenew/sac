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

#include <algorithm>
#include <filesystem>
#include <fnmatch.h>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sw/sac/errors.h"
#include "sw/sac/llm_client.h"

namespace sw::sac::tools {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

bool should_ignore(const std::string& name, const std::vector<std::string>& ignore_patterns) {
    for (const auto& pattern : ignore_patterns) {
        if (fnmatch(pattern.c_str(), name.c_str(), 0) == 0) {
            return true;
        }
    }
    return false;
}

std::string list_directory(const fs::path& dir_path, const std::vector<std::string>& ignore_patterns) {
    if (!fs::exists(dir_path)) {
        throw Error("Path not found: " + dir_path.string());
    }
    if (!fs::is_directory(dir_path)) {
        throw Error("Path is not a directory: " + dir_path.string());
    }

    std::vector<std::string> files;
    std::vector<std::string> directories;

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        std::string name = entry.path().filename().string();

        // Skip if matches ignore patterns
        if (should_ignore(name, ignore_patterns)) {
            continue;
        }

        if (fs::is_directory(entry)) {
            directories.push_back(name + "/");
        } else {
            files.push_back(name);
        }
    }

    // Sort alphabetically
    std::sort(directories.begin(), directories.end());
    std::sort(files.begin(), files.end());

    std::ostringstream result;
    result << "Contents of " << dir_path.string() << ":\n";

    // List directories first
    for (const auto& dir : directories) {
        result << "  [DIR]  " << dir << "\n";
    }

    // Then files
    for (const auto& file : files) {
        result << "  [FILE] " << file << "\n";
    }

    result << "\nTotal: " << directories.size() << " directorie(s), " << files.size() << " file(s)";

    return result.str();
}

std::string ls_tool_impl(const std::string& args_json) {
    try {
        json args = json::parse(args_json);

        if (!args.contains("path") || !args["path"].is_string()) {
            return "Error: Missing required parameter 'path'";
        }

        std::string path_str = args["path"];
        fs::path dir_path(path_str);

        std::vector<std::string> ignore_patterns;
        if (args.contains("ignore") && args["ignore"].is_array()) {
            for (const auto& item : args["ignore"]) {
                if (item.is_string()) {
                    ignore_patterns.push_back(item.get<std::string>());
                }
            }
        }

        return list_directory(dir_path, ignore_patterns);
    } catch (const json::exception& e) {
        return std::string("Error: Failed to parse arguments - ") + e.what();
    } catch (const Error& e) {
        return std::string("Error: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // anonymous namespace

Tool make_ls_tool() {
    ToolDef def;
    def.name = "ls";
    def.description = "List files and directories in a given path. Returns a formatted list with directories marked.";

    ToolParam path_param;
    path_param.name = "path";
    path_param.description = "The absolute path to the directory to list";
    path_param.type = "string";
    path_param.required = true;
    def.parameters.push_back(path_param);

    ToolParam ignore_param;
    ignore_param.name = "ignore";
    ignore_param.description = "List of glob patterns to ignore (optional)";
    ignore_param.type = "string";
    ignore_param.required = false;
    def.parameters.push_back(ignore_param);

    return Tool{def, ls_tool_impl};
}

} // namespace sw::sac::tools
