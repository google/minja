/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja/chat-template.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>

#undef NDEBUG
#include <cassert>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h" // For GetParseError_En

#define TEST_DATE (getenv("TEST_DATE") ? getenv("TEST_DATE") : "2024-07-26")

using Document = rapidjson::Document;
using RValue = rapidjson::Value;
// Forward declare nlohmann::json temporarily for the bridge, as minja::Value constructor might still use it.
namespace nlohmann { template<typename, typename, class> class basic_json; using ordered_json = basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator, adl_serializer, std::vector<std::uint8_t>>; }


template <class T>
static void assert_equals(const T &expected, const T &actual){
    if (expected != actual) {
        std::cerr << "Expected: " << expected << "\n\n";
        std::cerr << "Actual: " << actual << "\n\n";
        auto i_divergence = std::min(expected.size(), actual.size());
        for (size_t i = 0; i < i_divergence; i++) {
            if (expected[i] != actual[i]) {
                i_divergence = i;
                break;
            }
        }
        std::cerr << "Divergence at index " << i_divergence << "\n\n";
        std::cerr << "Expected suffix: " << expected.substr(i_divergence) << "\n\n";
        std::cerr << "Actual suffix: " << actual.substr(i_divergence) << "\n\n";

        std::cerr << std::flush;
        throw std::runtime_error("Test failed");
    }
}

static std::string read_file(const std::string &path) {
    std::ifstream fs(path, std::ios_base::binary);
    if (!fs.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    fs.seekg(0, std::ios_base::end);
    auto size = fs.tellg();
    fs.seekg(0);
    std::string out;
    out.resize(static_cast<size_t>(size));
    fs.read(&out[0], static_cast<std::streamsize>(size));
    return out;
}

static void write_file(const std::string &path, const std::string &content) {
    std::ofstream fs(path, std::ios_base::binary);
    if (!fs.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    fs.write(content.data(), content.size());
}

#ifndef _WIN32
// Returns a JSON string
static std::string caps_to_json_string(const minja::chat_template_caps &caps) {
    Document d;
    d.SetObject();
    Document::AllocatorType& allocator = d.GetAllocator();

    d.AddMember("supports_tools", caps.supports_tools, allocator);
    d.AddMember("supports_tool_calls", caps.supports_tool_calls, allocator);
    d.AddMember("supports_tool_responses", caps.supports_tool_responses, allocator);
    d.AddMember("supports_system_role", caps.supports_system_role, allocator);
    d.AddMember("supports_parallel_tool_calls", caps.supports_parallel_tool_calls, allocator);
    d.AddMember("supports_tool_call_id", caps.supports_tool_call_id, allocator);
    d.AddMember("requires_object_arguments", caps.requires_object_arguments, allocator);
    // d.AddMember("requires_non_null_content", caps.requires_non_null_content, allocator);
    d.AddMember("requires_typed_content", caps.requires_typed_content, allocator);
    
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetIndent(' ', 2); // Mimic nlohmann::json::dump(2)
    d.Accept(writer);
    return buffer.GetString();
}
#endif

int main(int argc, char *argv[]) {
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <template_file.jinja> <template_file.jinja.caps.json> <context_file.json> <golden_file.txt>" << "\n";
        for (int i = 0; i < argc; i++)
        {
            std::cerr << "argv[" << i << "] = " << argv[i] << "\n";
        }
        return 1;
    }

    try {
        std::string tmpl_file = argv[1];
        std::string caps_file = argv[2];
        std::string ctx_file = argv[3];
        std::string golden_file = argv[4];

        auto tmpl_str = read_file(tmpl_file);

        if (ctx_file == "n/a")
        {
            std::cout << "# Skipping template: " << tmpl_file << "\n" << tmpl_str << "\n";
            return 127;
        }

        std::cout << "# Testing template:\n";
        Document args_doc_debug; 
        args_doc_debug.SetArray();
        Document::AllocatorType& args_alloc = args_doc_debug.GetAllocator();
        args_doc_debug.PushBack(RValue(tmpl_file.c_str(), args_alloc).Move(), args_alloc);
        args_doc_debug.PushBack(RValue(caps_file.c_str(), args_alloc).Move(), args_alloc);
        args_doc_debug.PushBack(RValue(ctx_file.c_str(), args_alloc).Move(), args_alloc);
        args_doc_debug.PushBack(RValue(golden_file.c_str(), args_alloc).Move(), args_alloc);
        rapidjson::StringBuffer args_buffer_debug;
        rapidjson::Writer<rapidjson::StringBuffer> args_writer_debug(args_buffer_debug);
        args_doc_debug.Accept(args_writer_debug);
        std::cout << "# ./build/bin/test-supported-template " << args_buffer_debug.GetString() << std::endl << std::flush;


        Document ctx_doc; 
        std::string ctx_json_str = read_file(ctx_file);
        if (ctx_doc.Parse(ctx_json_str.c_str()).HasParseError()) {
            fprintf(stderr, "JSON parse error for context file %s: %s (offset %u)\n",
                    ctx_file.c_str(),
                    rapidjson::GetParseError_En(ctx_doc.GetParseError()),
                    static_cast<unsigned>(ctx_doc.GetErrorOffset()));
            return 1;
        }

        if (!ctx_doc.HasMember("bos_token") || !ctx_doc["bos_token"].IsString() ||
            !ctx_doc.HasMember("eos_token") || !ctx_doc["eos_token"].IsString() ||
            !ctx_doc.HasMember("messages") || 
            !ctx_doc.HasMember("add_generation_prompt") || !ctx_doc["add_generation_prompt"].IsBool()) {
            std::cerr << "Context JSON missing required fields or has wrong types.\n";
            return 1;
        }

        minja::chat_template tmpl(
            tmpl_str,
            ctx_doc["bos_token"].GetString(),
            ctx_doc["eos_token"].GetString());

        std::string expected;
        try {
            expected = minja::normalize_newlines(read_file(golden_file));
        } catch (const std::exception &e) {
            std::cerr << "Failed to read golden file: " << golden_file << "\n";
            std::cerr << e.what() << "\n";
            return 1;
        }
        
        Document inputs_owner_doc; 
        minja::chat_template_inputs inputs;
        inputs.allocator_for_inputs = &inputs_owner_doc.GetAllocator();
        
        if (ctx_doc.HasMember("messages") && ctx_doc["messages"].IsArray()) { // Ensure messages is an array
            inputs.messages.CopyFrom(ctx_doc["messages"], *inputs.allocator_for_inputs);
        } else if (ctx_doc.HasMember("messages") && ctx_doc["messages"].IsNull()) {
             inputs.messages.SetNull();
        }
         else {
            std::cerr << "Warning: 'messages' field in context is not an array or null. Defaulting to empty array.\n";
            inputs.messages.SetArray(*inputs.allocator_for_inputs);
        }
        if(ctx_doc.HasMember("messages")) ctx_doc.RemoveMember("messages");


        if (ctx_doc.HasMember("tools")) {
            if (ctx_doc["tools"].IsArray() || ctx_doc["tools"].IsNull()) { 
                 inputs.tools.CopyFrom(ctx_doc["tools"], *inputs.allocator_for_inputs);
            } else {
                std::cerr << "Warning: 'tools' field in context is not an array or null. Defaulting to null tools.\n";
                inputs.tools.SetNull();
            }
            ctx_doc.RemoveMember("tools");
        } else {
            inputs.tools.SetNull(); 
        }

        inputs.add_generation_prompt = ctx_doc["add_generation_prompt"].GetBool();
        ctx_doc.RemoveMember("add_generation_prompt");
        if (ctx_doc.HasMember("bos_token")) ctx_doc.RemoveMember("bos_token");
        if (ctx_doc.HasMember("eos_token")) ctx_doc.RemoveMember("eos_token");


        std::istringstream ss(TEST_DATE);
        std::tm tm_struct = {}; // Initialize to avoid uninitialized values
        ss >> std::get_time(&tm_struct, "%Y-%m-%d");
        if (ss.fail()) {
            std::cerr << "Failed to parse TEST_DATE: " << TEST_DATE << std::endl;
            // Handle error, e.g., use current time or a fixed default
            inputs.now = std::chrono::system_clock::now();
        } else {
            inputs.now = std::chrono::system_clock::from_time_t(std::mktime(&tm_struct));
        }


        inputs.extra_context.CopyFrom(ctx_doc, *inputs.allocator_for_inputs);


        std::string actual;
        try {
            actual = tmpl.apply(inputs);
        } catch (const std::exception &e) {
            std::cerr << "Error applying template: " << e.what() << "\n";
            return 1;
        }

        if (expected != actual) {
            if (getenv("WRITE_GOLDENS")) {
                write_file(golden_file, actual);
                std::cerr << "Updated golden file: " << golden_file << "\n";
            } else {
                assert_equals(expected, actual);
            }
        }

#ifndef _WIN32
        auto expected_caps_str = minja::normalize_newlines(read_file(caps_file));
        auto actual_caps_str = caps_to_json_string(tmpl.original_caps()); 
        assert_equals(expected_caps_str, actual_caps_str);
#endif

        std::cout << "Test passed successfully." << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
