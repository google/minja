/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja/minja.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <fstream>
#include <iostream>
#include <string>
#include "minja/chat-template.hpp"

using namespace minja;

static std::string read_file(const std::string &path)
{
    std::ifstream fs(path, std::ios_base::binary);
    if (!fs.is_open())
    {
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

#define TEMPLATE_CHATML \
    "{%- for message in messages -%}\n" \
    "  {{- '<|im_start|>' + message.role + '\n' + message.content + '<|im_end|>\n' -}}\n" \
    "{%- endfor -%}\n" \
    "{%- if add_generation_prompt -%}\n" \
    "  {{- '<|im_start|>assistant\n' -}}\n" \
    "{%- endif -%}"


#define TEMPLATE_CHATML_NO_SYSTEM \
    "{%- for message in messages -%}\n" \
    "  {%- if message.role == 'system' -%}\n" \
    "    {{- raise_exception('System role not supported') -}}\n" \
    "  {%- endif -%}\n" \
    "  {{- '<|im_start|>' + message.role + '\n' + message.content + '<|im_end|>\n' -}}\n" \
    "{%- endfor -%}\n" \
    "{%- if add_generation_prompt -%}\n" \
    "  {{- '<|im_start|>assistant\n' -}}\n" \
    "{%- endif -%}"


#define TEMPLATE_DUMMY \
    "{%- for tool in tools -%}\n" \
    "  {{- 'tool: ' + (tool | tojson(indent=2)) + '\n'  -}}\n" \
    "{%- endfor -%}\n" \
    "{%- for message in messages -%}\n" \
    "  {{- 'message: ' + (message | tojson(indent=2)) + '\n' -}}\n" \
    "{%- endfor -%}\n" \
    "{%- if add_generation_prompt -%}\n" \
    "  {{- 'message: ' -}}\n" \
    "{%- endif -%}"


const json message_user_text {
    { "role",    "user"     },
    { "content", "I need help" },
};
const json message_assistant_text {
    { "role",    "assistant"     },
    { "content", "Hello, world!" },
};
const json message_system {
    { "role",    "system"     },
    { "content", "I am The System!" },
};
const json tool_calls = json::array({{
    { "type", "function" },
    { "function", { { "name", "special_function" }, { "arguments", "{\"arg1\": 1}" } } },
}});

const json message_assistant_call {
    { "role",       "assistant"},
    { "content",    {}},
    { "tool_calls", {
        {
            { "type", "function" },
            { "function", {
                { "name", "special_function" },
                { "arguments", "{\"arg1\": 1}" },
            }},
        },
    }},
};
const json message_assistant_call_id {
    { "role",       "assistant"},
    { "content",    {}},
    { "tool_calls", {
        {
            { "type", "function" },
            { "function", {
                { "name", "special_function" },
                { "arguments", "{\"arg1\": 1}" },
            }},
            {"id", "123456789"},
        },
    }},
    { "role",       "assistant"                },
    { "content",    {}                         },
    { "tool_calls", tool_calls                  }
};
const json message_assistant_call_idx {
    { "role",       "assistant"},
    { "content",    {}},
    { "tool_plan",  "I'm not so sure"},
    { "tool_calls", {
        {
            { "type", "function" },
            { "function", {
                { "name", "special_function" },
                { "arguments", "{\"arg1\": 1}" },
            }},
            {"id", "0"},
        },
    }},
    { "role",       "assistant"                },
    { "content",    {}                         },
    { "tool_calls", tool_calls                  }
};
const json message_tool {
    { "role",       "tool"},
    { "content",    {
      {"result", 123},
    }},
    { "tool_call_id", "123456789"},
};

const auto special_function_tool = json::parse(R"({
  "type": "function",
  "function": {
    "name": "special_function",
    "description": "I'm special",
    "parameters": {
      "type": "object",
      "properties": {
        "arg1": {
          "type": "integer",
          "description": "The arg."
        }
      },
      "required": ["arg1"]
    }
  }
})");

auto ThrowsWithSubstr = [](const std::string & expected_substr) {
    return testing::Throws<std::runtime_error>(Property(&std::runtime_error::what, testing::HasSubstr(expected_substr)));
};

static chat_template_options options_no_polyfills() {
    chat_template_options opts;
    opts.apply_polyfills = false;
    opts.polyfill_system_role = false;
    opts.polyfill_tools = false;
    opts.polyfill_tool_call_examples = false;
    opts.polyfill_tool_calls = false;
    opts.polyfill_tool_responses = false;
    opts.polyfill_object_arguments = false;
    opts.polyfill_typed_content = false;
    return opts;
};

TEST(PolyfillTest, NoPolyFill) {
    chat_template tmpl(TEMPLATE_CHATML, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text});

    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs, options_no_polyfills()));

    inputs.add_generation_prompt = false;
    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n",
        tmpl.apply(inputs, options_no_polyfills()));

    inputs.messages = json::array({message_user_text, message_assistant_text});
    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n"
        "Hello, world!<|im_end|>\n",
        tmpl.apply(inputs, options_no_polyfills()));
}

TEST(PolyfillTest, SystemRoleSupported) {
    chat_template chatml(TEMPLATE_CHATML, "", "");
    chat_template dummy(TEMPLATE_DUMMY, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_system, message_user_text});

    EXPECT_EQ(
        "<|im_start|>system\n"
        "I am The System!<|im_end|>\n"
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        chatml.apply(inputs));
    EXPECT_EQ(
        "message: {\n"
        "  \"role\": \"system\",\n"
        "  \"content\": \"I am The System!\"\n"
        "}\n"
        "message: {\n"
        "  \"role\": \"user\",\n"
        "  \"content\": \"I need help\"\n"
        "}\n"
        "message: ",
        dummy.apply(inputs));
}

TEST(PolyfillTest, SystemRolePolyfill) {
    chat_template tmpl(TEMPLATE_CHATML_NO_SYSTEM, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_system, message_user_text});

    EXPECT_THAT(
        [&]() { tmpl.apply(inputs, options_no_polyfills()); },
        ThrowsWithSubstr("System role not supported"));

    EXPECT_EQ(
        "<|im_start|>user\n"
        "I am The System!\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolCallSupported) {
    chat_template tmpl(TEMPLATE_DUMMY, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text, message_assistant_call_id});

    EXPECT_EQ(
        "message: {\n"
        "  \"role\": \"user\",\n"
        "  \"content\": \"I need help\"\n"
        "}\n"
        "message: {\n"
        "  \"role\": \"assistant\",\n"
        "  \"content\": null,\n"
        "  \"tool_calls\": [\n"
        "    {\n"
        "      \"type\": \"function\",\n"
        "      \"function\": {\n"
        "        \"name\": \"special_function\",\n"
        "        \"arguments\": {\n"
        "          \"arg1\": 1\n"
        "        }\n"
        "      },\n"
        "      \"id\": \"123456789\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
        "message: ",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolCallPolyfill) {
    chat_template tmpl(TEMPLATE_CHATML, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text, message_assistant_call_id});

    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n"
        "{\n"
        "  \"tool_calls\": [\n"
        "    {\n"
        "      \"name\": \"special_function\",\n"
        "      \"arguments\": {\n"
        "        \"arg1\": 1\n"
        "      },\n"
        "      \"id\": \"123456789\"\n"
        "    }\n"
        "  ]\n"
        "}<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolsPolyfill) {
    chat_template tmpl(TEMPLATE_CHATML, "", "<|im_end|>");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text});
    inputs.tools = json::array({special_function_tool});

    EXPECT_EQ(
        "<|im_start|>system\n"
        "You can call any of the following tools to satisfy the user's requests: [\n"
        "  {\n"
        "    \"type\": \"function\",\n"
        "    \"function\": {\n"
        "      \"name\": \"special_function\",\n"
        "      \"description\": \"I'm special\",\n"
        "      \"parameters\": {\n"
        "        \"type\": \"object\",\n"
        "        \"properties\": {\n"
        "          \"arg1\": {\n"
        "            \"type\": \"integer\",\n"
        "            \"description\": \"The arg.\"\n"
        "          }\n"
        "        },\n"
        "        \"required\": [\n"
        "          \"arg1\"\n"
        "        ]\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "]\n"
        "\n"
        "Example tool call syntax:\n"
        "\n"
        "{\n"
        "  \"tool_calls\": [\n"
        "    {\n"
        "      \"name\": \"tool_name\",\n"
        "      \"arguments\": {\n"
        "        \"arg1\": \"some_value\"\n"
        "      },\n"
        "      \"id\": \"call_1___\"\n"
        "    }\n"
        "  ]\n"
        "}\n\n<|im_end|>\n"
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolSupported) {
    chat_template tmpl(TEMPLATE_DUMMY, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "message: {\n"
        "  \"role\": \"tool\",\n"
        "  \"content\": {\n"
        "    \"result\": 123\n"
        "  },\n"
        "  \"tool_call_id\": \"123456789\"\n"
        "}\n"
        "message: ",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolPolyfill) {
    chat_template tmpl(TEMPLATE_CHATML_NO_SYSTEM, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|im_start|>user\n{\n"
        "  \"tool_response\": {\n"
        "    \"content\": {\n"
        "      \"result\": 123\n"
        "    },\n"
        "    \"tool_call_id\": \"123456789\"\n"
        "  }\n"
        "}<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}

#ifndef _WIN32
TEST(ToolTest, DeepSeekR1) {
    chat_template tmpl(read_file("tests/deepseek-ai-DeepSeek-R1-Distill-Qwen-32B.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<｜tool▁outputs▁begin｜><｜tool▁output▁begin｜>{'result': 123}<｜tool▁output▁end｜><｜tool▁outputs▁end｜>",
        tmpl.apply(inputs));
}

TEST(ToolTest, CommandR7b) {
    chat_template tmpl(read_file("tests/CohereForAI-c4ai-command-r7b-12-2024-tool_use.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|># System Preamble\n"
        "You are in contextual safety mode. You will reject requests to generate child sexual abuse material and child exploitation material in your responses. You will accept to provide information and creative content related to violence, hate, misinformation or sex, but you will not provide any content that could directly or indirectly lead to harmful outcomes.\n"
        "\n"
        "Your information cutoff date is June 2024.\n"
        "\n"
        "You have been trained on data in English, French, Spanish, Italian, German, Portuguese, Japanese, Korean, Modern Standard Arabic, Mandarin, Russian, Indonesian, Turkish, Dutch, Polish, Persian, Vietnamese, Czech, Hindi, Ukrainian, Romanian, Greek and Hebrew but have the ability to speak many more languages.\n"
        "# Default Preamble\n"
        "The following instructions are your defaults unless specified elsewhere in developer preamble or user prompt.\n"
        "- Your name is Command.\n"
        "- You are a large language model built by Cohere.\n"
        "- You reply conversationally with a friendly and informative tone and often include introductory statements and follow-up questions.\n"
        "- If the input is ambiguous, ask clarifying follow-up questions.\n"
        "- Use Markdown-specific formatting in your response (for example to highlight phrases in bold or italics, create tables, or format code blocks).\n"
        "- Use LaTeX to generate mathematical notation for complex equations.\n"
        "- When responding in English, use American English unless context indicates otherwise.\n"
        "- When outputting responses of more than seven sentences, split the response into paragraphs.\n"
        "- Prefer the active voice.\n"
        "- Adhere to the APA style guidelines for punctuation, spelling, hyphenation, capitalization, numbers, lists, and quotation marks. Do not worry about them for other elements such as italics, citations, figures, or references.\n"
        "- Use gender-neutral pronouns for unspecified persons.\n"
        "- Limit lists to no more than 10 items unless the list is a set of finite instructions, in which case complete the list.\n"
        "- Use the third person when asked to write a summary.\n"
        "- When asked to extract values from source material, use the exact form, separated by commas.\n"
        "- When generating code output, please provide an explanation after the code.\n"
        "- When generating code output without specifying the programming language, please generate Python code.\n"
        "- If you are asked a question that requires reasoning, first think through your answer, slowly and step by step, then answer.<|END_OF_TURN_TOKEN|><|START_OF_TURN_TOKEN|><|SYSTEM_TOKEN|><|START_TOOL_RESULT|>[\n"
        "    {\n"
        "        \"tool_call_id\": \"\",\n"
        "        \"results\": {\n"
        "            \"0\": {\"result\": 123}\n"
        "        },\n"
        "        \"is_error\": null\n"
        "    }\n"
        "]<|END_TOOL_RESULT|><|END_OF_TURN_TOKEN|><|START_OF_TURN_TOKEN|><|CHATBOT_TOKEN|>",
        tmpl.apply(inputs));
}
#endif // NOT _WIN32

TEST(ToolTest, MistralNemo) {
    chat_template tmpl(read_file("tests/mistralai-Mistral-Nemo-Instruct-2407.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "[TOOL_RESULTS]{\"content\": {'result': 123}, \"call_id\": \"123456789\"}[/TOOL_RESULTS]",
        tmpl.apply(inputs));
}

TEST(ToolTest, NousResearchHermes3) {
    chat_template tmpl(read_file("tests/NousResearch-Hermes-3-Llama-3.1-70B-tool_use.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|im_start|>system\n"
        "You are a function calling AI model. You are provided with function signatures within <tools></tools> XML tags. You may call one or more functions to assist with the user query. Don't make assumptions about what values to plug into functions. Here are the available tools: <tools>  </tools>Use the following pydantic model json schema for each tool call you will make: {\"properties\": {\"name\": {\"title\": \"Name\", \"type\": \"string\"}, \"arguments\": {\"title\": \"Arguments\", \"type\": \"object\"}}, \"required\": [\"name\", \"arguments\"], \"title\": \"FunctionCall\", \"type\": \"object\"}}\n"
        "For each function call return a json object with function name and arguments within <tool_call></tool_call> XML tags as follows:\n"
        "<tool_call>\n"
        "{\"name\": <function-name>, \"arguments\": <args-dict>}\n"
        "</tool_call><|im_end|>\n"
        "<tool_response>\n"
        "{'result': 123}\n"
        "</tool_response><|im_end|><|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(ToolTest, NousResearchHermes2) {
    chat_template tmpl(read_file("tests/NousResearch-Hermes-2-Pro-Llama-3-8B-tool_use.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|im_start|>system\n"
        "You are a function calling AI model. You are provided with function signatures within <tools></tools> XML tags. You may call one or more functions to assist with the user query. Don't make assumptions about what values to plug into functions. Here are the available tools: <tools>  </tools>Use the following pydantic model json schema for each tool call you will make: {\"properties\": {\"name\": {\"title\": \"Name\", \"type\": \"string\"}, \"arguments\": {\"title\": \"Arguments\", \"type\": \"object\"}}, \"required\": [\"name\", \"arguments\"], \"title\": \"FunctionCall\", \"type\": \"object\"}}\n"
        "For each function call return a json object with function name and arguments within <tool_call></tool_call> XML tags as follows:\n"
        "<tool_call>\n"
        "{\"name\": <function-name>, \"arguments\": <args-dict>}\n"
        "</tool_call><|im_end|>\n"
        "<tool_response>\n"
        "{'result': 123}\n"
        "</tool_response><|im_end|><|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(ToolTest, Llama3_3) {
    chat_template tmpl(read_file("tests/meta-llama-Llama-3.3-70B-Instruct.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|start_header_id|>system<|end_header_id|>\n"
        "\n"
        "Cutting Knowledge Date: December 2023\n"
        "Today Date: 26 Jul 2024\n"
        "\n"
        "<|eot_id|><|start_header_id|>ipython<|end_header_id|>\n"
        "\n"
        "{\"result\": 123}<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n"
        "\n",
        tmpl.apply(inputs));
}

TEST(ToolTest, MeetkaiFunctionary3_1) {
    chat_template tmpl(read_file("tests/meetkai-functionary-medium-v3.1.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|start_header_id|>system<|end_header_id|>\n"
        "\n"
        "\n"
        "Cutting Knowledge Date: December 2023\n"
        "\n"
        "<|eot_id|><|start_header_id|>ipython<|end_header_id|>\n"
        "\n"
        "{'result': 123}<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n"
        "\n",
        tmpl.apply(inputs));
}

TEST(ToolTest, MeetkaiFunctionary3_2) {
    chat_template tmpl(read_file("tests/meetkai-functionary-medium-v3.2.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|start_header_id|>system<|end_header_id|>\n"
        "\n"
        "You are capable of executing available function(s) if required.\n"
        "Only execute function(s) when absolutely necessary.\n"
        "Ask for the required input to:recipient==all\n"
        "Use JSON for function arguments.\n"
        "Respond in this format:\n"
        ">>>${recipient}\n"
        "${content}\n"
        "Available functions:\n"
        "// Supported function definitions that should be called when necessary.\n"
        "namespace functions {\n"
        "\n"
        "} // namespace functions<|eot_id|><|start_header_id|>tool<|end_header_id|>\n"
        "\n"
        "{'result': 123}<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n"
        "\n"
        ">>>",
        tmpl.apply(inputs));
}

/*
https://github.com/google/minja/issues/7
TEST(ToolTest, FirefunctionV2) {
    chat_template tmpl(read_file("tests/fireworks-ai-llama-3-firefunction-v2.jinja"), "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|im_start|>tool\n"
        "{\n"
        "  \"result\": 123\n"
        "}\n"
        "<|im_end|>",
        tmpl.apply(inputs));
}
*/
