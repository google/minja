/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include <minja/chat-template.hpp>
#include <iostream>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h" // For debugging if needed
#include "rapidjson/error/en.h"


// using json = nlohmann::ordered_json; // Replaced
// No top-level using Document = rapidjson::Document; needed if interaction is via minja types

int main() {
    minja::chat_template tmpl(
        "{% for message in messages %}"
        "{{ '<|' + message['role'] + '|>\\n' + message['content'] + '<|end|>' + '\\n' }}"
        "{% endfor %}",
        /* bos_token= */ "<|start|>",
        /* eos_token= */ "<|end|>"
    );

    minja::chat_template_inputs inputs;
    
    // For messages
    rapidjson::Document messages_doc;
    const char* messages_json_str = R"([
        {"role": "user", "content": "Hello"},
        {"role": "assistant", "content": "Hi there"}
    ])";
    if (messages_doc.Parse(messages_json_str).HasParseError()) {
        fprintf(stderr, "JSON parse error for messages: %s (offset %u)\n",
                rapidjson::GetParseError_En(messages_doc.GetParseError()),
                static_cast<unsigned>(messages_doc.GetErrorOffset()));
        return 1;
    }
    // Assuming inputs.messages is a rapidjson::Value and needs an allocator,
    // or chat_template_inputs constructor/assignment handles it.
    // If inputs.messages needs to be self-contained or modified by `apply`,
    // it might need its own allocator or copy from messages_doc using an allocator.
    // Let's assume chat_template_inputs is designed to take ownership or copy.
    // The `chat_template_inputs` struct was defined with its own allocator member `allocator_for_inputs`
    // and its members `messages`, `tools`, `extra_context` are `rapidjson::Value`.
    // We need to ensure an allocator is available for these members.
    // Simplest for an example: create a main Document that owns all data for inputs.
    rapidjson::Document input_data_owner_doc;
    inputs.allocator_for_inputs = &input_data_owner_doc.GetAllocator();

    inputs.messages.CopyFrom(messages_doc, *inputs.allocator_for_inputs);

    inputs.add_generation_prompt = true;

    // For tools
    rapidjson::Document tools_doc;
    const char* tools_json_str = R"([
        {"type": "function", "function": {"name": "google_search", "arguments": {"query": "2+2"}}}
    ])";
    if (tools_doc.Parse(tools_json_str).HasParseError()) {
        fprintf(stderr, "JSON parse error for tools: %s (offset %u)\n",
                rapidjson::GetParseError_En(tools_doc.GetParseError()),
                static_cast<unsigned>(tools_doc.GetErrorOffset()));
        return 1;
    }
    inputs.tools.CopyFrom(tools_doc, *inputs.allocator_for_inputs);
    // inputs.extra_context is already kNullType by default in chat_template_inputs constructor.

    std::cout << tmpl.apply(inputs) << std::endl;
}
