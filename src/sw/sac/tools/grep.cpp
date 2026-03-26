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

#include <filesystem>
#include <fnmatch.h>
#include <fstream>
#include <regex>
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

bool match_include_pattern(const std::string& filename, const std::string& pattern) {
    return fnmatch(pattern.c_str(), filename.c_str(), 0) == 0;
}

void grep_in_file(const fs::path& file_path, const std::regex& pattern_regex,
                  std::ostringstream& result, int& total_matches) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    int line_num = 0;
    bool file_header_printed = false;

    while (std::getline(file, line)) {
        ++line_num;
        if (std::regex_search(line, pattern_regex)) {
            if (!file_header_printed) {
                result << "\n" << file_path.string() << ":\n";
                file_header_printed = true;
            }
            result << "  " << line_num << ":" << line << "\n";
            ++total_matches;
        }
    }
}

void grep_recursive(const fs::path& base_path, const std::regex& pattern_regex,
                    const std::string& include_pattern,
                    std::ostringstream& result, int& total_matches) {
    if (!fs::exists(base_path) || !fs::is_directory(base_path)) {
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(base_path)) {
        if (!fs::is_regular_file(entry)) {
            continue;
        }

        // Check include pattern if specified
        if (!include_pattern.empty()) {
            std::string filename = entry.path().filename().string();
            if (!match_include_pattern(filename, include_pattern)) {
                continue;
            }
        }

        grep_in_file(entry.path(), pattern_regex, result, total_matches);
    }
}

std::string grep_files(const fs::path& search_path, const std::string& pattern,
                       const std::string& include_pattern) {
    if (!fs::exists(search_path)) {
        throw Error("Path not found: " + search_path.string());
    }
    if (!fs::is_directory(search_path)) {
        throw Error("Path is not a directory: " + search_path.string());
    }

    std::regex pattern_regex;
    try {
        pattern_regex = std::regex(pattern, std::regex::ECMAScript);
    } catch (const std::regex_error& e) {
        throw Error("Invalid regex pattern: " + std::string(e.what()));
    }

    std::ostringstream result;
    int total_matches = 0;

    grep_recursive(search_path, pattern_regex, include_pattern, result, total_matches);

    std::ostringstream final_result;
    final_result << "Found " << total_matches << " match(es) in the following files:";
    final_result << result.str();

    return final_result.str();
}

std::string grep_tool_impl(const std::string& args_json) {
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

        std::string include_pattern;
        if (args.contains("include") && args["include"].is_string()) {
            include_pattern = args["include"];
        }

        return grep_files(search_path, pattern, include_pattern);
    } catch (const json::exception& e) {
        return std::string("Error: Failed to parse arguments - ") + e.what();
    } catch (const Error& e) {
        return std::string("Error: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // anonymous namespace

Tool make_grep_tool() {
    ToolDef def;
    def.name = "grep";
    def.description = "Search for a regex pattern in files. Returns matching file paths and line contents.";

    ToolParam pattern_param;
    pattern_param.name = "pattern";
    pattern_param.description = "The regex pattern to search for";
    pattern_param.type = "string";
    pattern_param.required = true;
    def.parameters.push_back(pattern_param);

    ToolParam path_param;
    path_param.name = "path";
    path_param.description = "The directory to search in (default: current working directory)";
    path_param.type = "string";
    path_param.required = false;
    def.parameters.push_back(path_param);

    ToolParam include_param;
    include_param.name = "include";
    include_param.description = "File filter pattern, e.g., '*.cpp' (optional)";
    include_param.type = "string";
    include_param.required = false;
    def.parameters.push_back(include_param);

    return Tool{def, grep_tool_impl};
}

} // namespace sw::sac::tools
