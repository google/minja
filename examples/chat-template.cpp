/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include <minja/chat-template.hpp>
#include <iostream>

using json = nlohmann::ordered_json;

int main() {
    minja::chat_template tmpl(
        "{% for message in messages %}"
        "{{ '<|' + message['role'] + '|>\\n' + message['content'] + '<|end|>' + '\\n' }}"
        "{% endfor %}",
        /* bos_token= */ "<|start|>",
        /* eos_token= */ "<|end|>"
    );

    minja::chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {"role": "user", "content": "Hello"},
        {"role": "assistant", "content": "Hi there"}
    ])");
    inputs.add_generation_prompt = true;
    inputs.tools = json::parse(R"([
        {"type": "function", "function": {"name": "google_search", "arguments": {"query": "2+2"}}}
    ])");

    std::cout << tmpl.apply(inputs) << std::endl;
}
