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
#include <string>

#include <nlohmann/json.hpp>

#include "sw/sac/errors.h"
#include "sw/sac/llm_client.h"

namespace sw::sac::tools {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string write_file_content(const fs::path& file_path, const std::string& content) {
    // Create parent directories if they don't exist
    fs::path parent = file_path.parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        fs::create_directories(parent);
    }

    std::ofstream file(file_path, std::ios::trunc);
    if (!file.is_open()) {
        throw Error("Cannot open file for writing: " + file_path.string());
    }

    file << content;
    if (!file.good()) {
        throw Error("Failed to write to file: " + file_path.string());
    }

    return "Successfully wrote to " + file_path.string();
}

std::string write_tool_impl(const std::string& args_json) {
    try {
        json args = json::parse(args_json);

        if (!args.contains("file_path") || !args["file_path"].is_string()) {
            return "Error: Missing required parameter 'file_path'";
        }

        if (!args.contains("content") || !args["content"].is_string()) {
            return "Error: Missing required parameter 'content'";
        }

        std::string file_path_str = args["file_path"];
        std::string content = args["content"];
        fs::path file_path(file_path_str);

        return write_file_content(file_path, content);
    } catch (const json::exception& e) {
        return std::string("Error: Failed to parse arguments - ") + e.what();
    } catch (const Error& e) {
        return std::string("Error: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // anonymous namespace

Tool make_write_tool() {
    ToolDef def;
    def.name = "write";
    def.description = "Write content to a file. Creates the file if it doesn't exist, overwrites if it does. Creates parent directories as needed.";

    ToolParam file_path_param;
    file_path_param.name = "file_path";
    file_path_param.description = "The absolute path to the file to write";
    file_path_param.type = "string";
    file_path_param.required = true;
    def.parameters.push_back(file_path_param);

    ToolParam content_param;
    content_param.name = "content";
    content_param.description = "The content to write to the file";
    content_param.type = "string";
    content_param.required = true;
    def.parameters.push_back(content_param);

    return Tool{def, write_tool_impl};
}

} // namespace sw::sac::tools
