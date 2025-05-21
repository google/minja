
/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja/chat-template.hpp"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <fstream>
#include <iostream>
#include <string>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include "rapidjson/ostreamwrapper.h" // For std::ofstream

using namespace minja;
using namespace testing;

// using json = nlohmann::ordered_json; // Replaced

static std::string render_python(const std::string & template_str, const chat_template_inputs & inputs) {
    // All rapidjson objects will be owned by this central document for simplicity in this function.
    rapidjson::Document d; 
    rapidjson::Document::AllocatorType& allocator = d.GetAllocator();

    rapidjson::Value bindings(rapidjson::kObjectType);
    
    // inputs.extra_context, inputs.messages, inputs.tools are already rapidjson::Value.
    // They need to be deep copied into the 'd' document's ownership if they are from elsewhere.
    // Assuming chat_template_inputs provides valid rapidjson::Value objects.
    // If inputs.allocator_for_inputs is different from &allocator, CopyFrom is essential.
    // If they are null, we create empty structures or add null members.

    if (inputs.extra_context.IsObject()) {
        bindings.CopyFrom(inputs.extra_context, allocator); // Start with extra_context
    } else {
        // Ensure bindings is an object even if extra_context is not or is null.
        // CopyFrom would make bindings a NullValue if extra_context is Null.
        // So, if extra_context is not an object, initialize bindings as an empty object.
        bindings.SetObject();
    }

    if (inputs.messages.IsArray()) {
        rapidjson::Value messages_copy;
        messages_copy.CopyFrom(inputs.messages, allocator);
        bindings.AddMember("messages", messages_copy, allocator);
    } else {
        bindings.AddMember("messages", rapidjson::Value(rapidjson::kArrayType), allocator); // Add empty array if null/not array
    }

    if (inputs.tools.IsArray()) {
        rapidjson::Value tools_copy;
        tools_copy.CopyFrom(inputs.tools, allocator);
        bindings.AddMember("tools", tools_copy, allocator);
    } else {
        bindings.AddMember("tools", rapidjson::Value(rapidjson::kArrayType), allocator); // Add empty array if null/not array
    }
    
    bindings.AddMember("add_generation_prompt", inputs.add_generation_prompt, allocator);

    rapidjson::Value data(rapidjson::kObjectType);
    data.AddMember("template", rapidjson::StringRef(template_str.c_str()), allocator);
    data.AddMember("bindings", bindings, allocator); // bindings already uses 'allocator'

    rapidjson::Value options(rapidjson::kObjectType);
    options.AddMember("trim_blocks", true, allocator);
    options.AddMember("lstrip_blocks", true, allocator);
    options.AddMember("keep_trailing_newline", false, allocator);
    data.AddMember("options", options, allocator);

    {
        std::ofstream ofs("data.json");
        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 2);
        data.Accept(writer);
        // ofs is closed when osw goes out of scope, then ofs goes out of scope.
    }

    auto pyExeEnv = getenv("PYTHON_EXECUTABLE");
    std::string pyExe = pyExeEnv ? pyExeEnv : "python3";

    std::remove("out.txt");
    // For debugging the JSON sent to python:
    // rapidjson::StringBuffer s_debug;
    // rapidjson::PrettyWriter<rapidjson::StringBuffer> writer_debug(s_debug);
    // data.Accept(writer_debug);
    // std::string data_dump_str = s_debug.GetString();

    auto res = std::system((pyExe + " -m scripts.render data.json out.txt").c_str());
    if (res != 0) {
        // Construct the error string using rapidjson serialization
        rapidjson::StringBuffer err_buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> err_writer(err_buffer);
        data.Accept(err_writer);
        throw std::runtime_error("Failed to run python script with data: " + std::string(err_buffer.GetString()));
    }

    std::ifstream f("out.txt");
    std::string out((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return out;
}

static std::string render(const std::string & template_str, const chat_template_inputs & inputs, const chat_template_options & opts) {
  if (getenv("USE_JINJA2")) {
      return render_python(template_str, inputs);
  }
  chat_template tmpl(
      template_str,
      "",
      "");
  return tmpl.apply(inputs, opts);
}

TEST(ChatTemplateTest, SimpleCases) {
    EXPECT_THAT(render("{{ strftime_now('%Y-%m-%d %H:%M:%S') }}", {}, {}), MatchesRegex(R"([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2})"));
}
