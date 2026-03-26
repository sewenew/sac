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
#include <fstream>
#include <iomanip>
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

std::string read_file_content(const fs::path& file_path, int offset, int limit) {
    if (!fs::exists(file_path)) {
        throw Error("File not found: " + file_path.string());
    }
    if (!fs::is_regular_file(file_path)) {
        throw Error("Path is not a file: " + file_path.string());
    }

    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw Error("Cannot open file: " + file_path.string());
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    if (lines.empty()) {
        return "<system reminder>";
    }

    // Adjust offset (1-based to 0-based)
    int start = offset > 0 ? offset - 1 : 0;
    if (start >= static_cast<int>(lines.size())) {
        return "<system reminder>";
    }

    int end = limit > 0 ? std::min(start + limit, static_cast<int>(lines.size())) : static_cast<int>(lines.size());

    std::ostringstream result;
    for (int i = start; i < end; ++i) {
        // Format like cat -n: right-aligned line numbers
        result << std::setw(6) << (i + 1) << "  " << lines[i] << "\n";
    }

    return result.str();
}

std::string read_tool_impl(const std::string& args_json) {
    try {
        json args = json::parse(args_json);

        if (!args.contains("file_path") || !args["file_path"].is_string()) {
            return "Error: Missing required parameter 'file_path'";
        }

        std::string file_path_str = args["file_path"];
        fs::path file_path(file_path_str);

        int offset = 0;
        if (args.contains("offset") && args["offset"].is_number_integer()) {
            offset = args["offset"];
        }

        int limit = 0;
        if (args.contains("limit") && args["limit"].is_number_integer()) {
            limit = args["limit"];
        }

        return read_file_content(file_path, offset, limit);
    } catch (const json::exception& e) {
        return std::string("Error: Failed to parse arguments - ") + e.what();
    } catch (const Error& e) {
        return std::string("Error: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // anonymous namespace

Tool make_read_tool() {
    ToolDef def;
    def.name = "read";
    def.description = "Read the contents of a file. Returns the file content with line numbers like 'cat -n'. If the file is empty, returns a system reminder.";

    ToolParam file_path_param;
    file_path_param.name = "file_path";
    file_path_param.description = "The absolute path to the file to read";
    file_path_param.type = "string";
    file_path_param.required = true;
    def.parameters.push_back(file_path_param);

    ToolParam offset_param;
    offset_param.name = "offset";
    offset_param.description = "The starting line number (1-based, optional)";
    offset_param.type = "integer";
    offset_param.required = false;
    def.parameters.push_back(offset_param);

    ToolParam limit_param;
    limit_param.name = "limit";
    limit_param.description = "The maximum number of lines to read (optional)";
    limit_param.type = "integer";
    limit_param.required = false;
    def.parameters.push_back(limit_param);

    return Tool{def, read_tool_impl};
}

} // namespace sw::sac::tools
