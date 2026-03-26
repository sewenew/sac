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
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "sw/sac/errors.h"
#include "sw/sac/llm_client.h"

namespace sw::sac::tools {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string generate_diff(const std::string& old_str, const std::string& new_str) {
    std::ostringstream diff;
    diff << "--- original\n";
    diff << "+++ modified\n";
    diff << "@@ -1 +1 @@\n";
    diff << "-" << old_str << "\n";
    diff << "+" << new_str << "\n";
    return diff.str();
}

std::string edit_file_content(const fs::path& file_path, const std::string& old_str,
                              const std::string& new_str, bool replace_all) {
    if (!fs::exists(file_path)) {
        throw Error("File not found: " + file_path.string());
    }
    if (!fs::is_regular_file(file_path)) {
        throw Error("Path is not a file: " + file_path.string());
    }

    // Read file content
    std::ifstream in_file(file_path);
    if (!in_file.is_open()) {
        throw Error("Cannot open file for reading: " + file_path.string());
    }

    std::stringstream buffer;
    buffer << in_file.rdbuf();
    std::string content = buffer.str();
    in_file.close();

    // Check if old_str exists in content
    if (content.find(old_str) == std::string::npos) {
        throw Error("Old string not found in file: " + old_str.substr(0, 50) + "...");
    }

    std::string original_content = content;

    if (replace_all) {
        size_t pos = 0;
        while ((pos = content.find(old_str, pos)) != std::string::npos) {
            content.replace(pos, old_str.length(), new_str);
            pos += new_str.length();
        }
    } else {
        size_t pos = content.find(old_str);
        content.replace(pos, old_str.length(), new_str);
    }

    // Write back
    std::ofstream out_file(file_path, std::ios::trunc);
    if (!out_file.is_open()) {
        throw Error("Cannot open file for writing: " + file_path.string());
    }

    out_file << content;
    if (!out_file.good()) {
        throw Error("Failed to write to file: " + file_path.string());
    }
    out_file.close();

    // Generate result with diff
    std::ostringstream result;
    result << "Successfully edited " << file_path.string() << "\n\n";
    result << "Diff:\n";
    result << generate_diff(old_str, new_str);
    return result.str();
}

std::string edit_tool_impl(const std::string& args_json) {
    try {
        json args = json::parse(args_json);

        if (!args.contains("file_path") || !args["file_path"].is_string()) {
            return "Error: Missing required parameter 'file_path'";
        }

        if (!args.contains("old_string") || !args["old_string"].is_string()) {
            return "Error: Missing required parameter 'old_string'";
        }

        if (!args.contains("new_string") || !args["new_string"].is_string()) {
            return "Error: Missing required parameter 'new_string'";
        }

        std::string file_path_str = args["file_path"];
        std::string old_str = args["old_string"];
        std::string new_str = args["new_string"];
        fs::path file_path(file_path_str);

        bool replace_all = false;
        if (args.contains("replace_all") && args["replace_all"].is_boolean()) {
            replace_all = args["replace_all"];
        }

        return edit_file_content(file_path, old_str, new_str, replace_all);
    } catch (const json::exception& e) {
        return std::string("Error: Failed to parse arguments - ") + e.what();
    } catch (const Error& e) {
        return std::string("Error: ") + e.what();
    } catch (const std::exception& e) {
        return std::string("Error: ") + e.what();
    }
}

} // anonymous namespace

Tool make_edit_tool() {
    ToolDef def;
    def.name = "edit";
    def.description = "Edit a file by replacing text. Replaces old_string with new_string. Returns success status and a diff of changes.";

    ToolParam file_path_param;
    file_path_param.name = "file_path";
    file_path_param.description = "The absolute path to the file to edit";
    file_path_param.type = "string";
    file_path_param.required = true;
    def.parameters.push_back(file_path_param);

    ToolParam old_string_param;
    old_string_param.name = "old_string";
    old_string_param.description = "The text to replace";
    old_string_param.type = "string";
    old_string_param.required = true;
    def.parameters.push_back(old_string_param);

    ToolParam new_string_param;
    new_string_param.name = "new_string";
    new_string_param.description = "The replacement text";
    new_string_param.type = "string";
    new_string_param.required = true;
    def.parameters.push_back(new_string_param);

    ToolParam replace_all_param;
    replace_all_param.name = "replace_all";
    replace_all_param.description = "Whether to replace all occurrences (default: false)";
    replace_all_param.type = "boolean";
    replace_all_param.required = false;
    def.parameters.push_back(replace_all_param);

    return Tool{def, edit_tool_impl};
}

} // namespace sw::sac::tools
