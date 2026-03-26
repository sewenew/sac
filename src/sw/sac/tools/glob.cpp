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

bool match_glob(const std::string& path, const std::string& pattern) {
    return fnmatch(pattern.c_str(), path.c_str(), FNM_PATHNAME) == 0;
}

void glob_recursive(const fs::path& base_path, const std::string& pattern,
                    std::vector<std::pair<fs::path, fs::file_time_type>>& results) {
    if (!fs::exists(base_path) || !fs::is_directory(base_path)) {
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(base_path)) {
        std::string relative_path = entry.path().lexically_relative(base_path).string();
        if (match_glob(relative_path, pattern) || match_glob(entry.path().filename().string(), pattern)) {
            results.emplace_back(entry.path(), fs::last_write_time(entry));
        }
    }
}

std::string glob_files(const fs::path& search_path, const std::string& pattern) {
    if (!fs::exists(search_path)) {
        throw Error("Path not found: " + search_path.string());
    }
    if (!fs::is_directory(search_path)) {
        throw Error("Path is not a directory: " + search_path.string());
    }

    std::vector<std::pair<fs::path, fs::file_time_type>> matches;
    glob_recursive(search_path, pattern, matches);

    // Sort by modification time (newest first)
    std::sort(matches.begin(), matches.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    std::ostringstream result;
    result << "Found " << matches.size() << " match(es):\n";
    for (const auto& [path, mtime] : matches) {
        result << path.string() << "\n";
    }

    return result.str();
}

std::string glob_tool_impl(const std::string& args_json) {
    try {
        json args = json::parse(args_json);

        if (!args.contains("pattern") || !args["pattern"].is_string()) {
            return "Error: Missing required parameter 'pattern'";
        }

        std::string pattern = args["pattern"];

        fs::path search_path = fs::current_path();
        if (args.contains("path") && args["path"].is_string()) {
            search_path = args["path"].get<std::string>();
        }

        return glob_files(search_path, pattern);
    } catch (const json::exception& e) {
        return std::string("Error: Failed to parse arguments - ") + e.what();
    } catch (const Error& e) {
        return std::string("Error: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // anonymous namespace

Tool make_glob_tool() {
    ToolDef def;
    def.name = "glob";
    def.description = "Find files matching a glob pattern. Returns file paths sorted by modification time (newest first).";

    ToolParam pattern_param;
    pattern_param.name = "pattern";
    pattern_param.description = "The glob pattern to match (e.g., '*.cpp', 'src/**/*.h')";
    pattern_param.type = "string";
    pattern_param.required = true;
    def.parameters.push_back(pattern_param);

    ToolParam path_param;
    path_param.name = "path";
    path_param.description = "The directory to search in (default: current working directory)";
    path_param.type = "string";
    path_param.required = false;
    def.parameters.push_back(path_param);

    return Tool{def, glob_tool_impl};
}

} // namespace sw::sac::tools
