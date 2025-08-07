/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja/chat-template.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>

#undef NDEBUG
#include <cassert>

using json = nlohmann::ordered_json;

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

static minja::chat_template_caps get_caps(const std::string &path)
{
    auto caps = minja::chat_template(read_file(path), "", "").original_caps();

    auto print = [](const std::string &name, bool value) {
        std::cout << "    " << (value ? "EXPECT_TRUE" : "EXPECT_FALSE") << "(caps." << name << ");" << std::endl;
    };
    auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    std::cout << "TEST(" << test_info->test_suite_name() << ", " << test_info->name() << ") {" << std::endl;
    std::cout << "    auto caps = get_caps(\"" << path << "\");" << std::endl;
    print("supports_system_role",         caps.supports_system_role);
    print("supports_tools",               caps.supports_tools);
    print("supports_tool_calls",          caps.supports_tool_calls);
    print("supports_tool_responses",      caps.supports_tool_responses);
    print("supports_parallel_tool_calls", caps.supports_parallel_tool_calls);
    print("requires_object_arguments",    caps.requires_object_arguments);
    // print("requires_non_null_content",    caps.requires_non_null_content);
    print("requires_typed_content",       caps.requires_typed_content);
    std::cout << "}\n" << std::endl;

    return caps;
}

TEST(CapabilitiesTest, Gemma7b) {
    auto caps = get_caps("tests/google-gemma-7b-it.jinja");
    EXPECT_FALSE(caps.supports_system_role);
    EXPECT_FALSE(caps.supports_tools);
    EXPECT_FALSE(caps.supports_tool_calls);
    EXPECT_FALSE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, QwQ32B) {
    auto caps = get_caps("tests/Qwen-QwQ-32B.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_FALSE(caps.supports_tool_responses);
    EXPECT_TRUE(caps.supports_parallel_tool_calls);
    EXPECT_TRUE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

#ifndef _WIN32
TEST(CapabilitiesTest, DeepSeekR1Distill)
{
    auto caps = get_caps("tests/deepseek-ai-DeepSeek-R1-Distill-Qwen-32B.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_FALSE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_TRUE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_FALSE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}
#endif

TEST(CapabilitiesTest, FunctionaryMediumV3_2) {
    auto caps = get_caps("tests/meetkai-functionary-medium-v3.2.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_TRUE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_FALSE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, MetaLlama3_1_8BInstruct) {
    auto caps = get_caps("tests/meta-llama-Llama-3.1-8B-Instruct.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_TRUE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, MetaLlama3_2_3BInstruct) {
    auto caps = get_caps("tests/meta-llama-Llama-3.2-3B-Instruct.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_TRUE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, MetaLlama3_3_70BInstruct) {
    auto caps = get_caps("tests/meta-llama-Llama-3.3-70B-Instruct.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_TRUE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, MiniMaxAIText01) {
    auto caps = get_caps("tests/MiniMaxAI-MiniMax-Text-01.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_FALSE(caps.supports_tool_calls);
    EXPECT_FALSE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_FALSE(caps.requires_non_null_content);
    EXPECT_TRUE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, Mistral7BInstruct) {
    auto caps = get_caps("tests/mistralai-Mistral-7B-Instruct-v0.2.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_FALSE(caps.supports_tools);
    EXPECT_FALSE(caps.supports_tool_calls);
    EXPECT_FALSE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, MistralNemoInstruct) {
    auto caps = get_caps("tests/mistralai-Mistral-Nemo-Instruct-2407.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_TRUE(caps.supports_parallel_tool_calls);
    EXPECT_TRUE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, NousResearchHermes3Llama3_1_70BToolUse) {
    auto caps = get_caps("tests/NousResearch-Hermes-3-Llama-3.1-70B-tool_use.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_TRUE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, NousResearchHermes2ProLlama3_8BToolUse) {
    auto caps = get_caps("tests/NousResearch-Hermes-2-Pro-Llama-3-8B-tool_use.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_TRUE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, CommandRPlusDefault) {
    auto caps = get_caps("tests/CohereForAI-c4ai-command-r-plus-default.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_FALSE(caps.supports_tools);
    EXPECT_FALSE(caps.supports_tool_calls);
    EXPECT_FALSE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, CommandRPlusRag) {
    auto caps = get_caps("tests/CohereForAI-c4ai-command-r-plus-rag.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_FALSE(caps.supports_tools);
    EXPECT_FALSE(caps.supports_tool_calls);
    EXPECT_FALSE(caps.supports_tool_responses);
    EXPECT_FALSE(caps.supports_parallel_tool_calls);
    EXPECT_FALSE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}

TEST(CapabilitiesTest, CommandRPlusToolUse) {
    auto caps = get_caps("tests/CohereForAI-c4ai-command-r-plus-tool_use.jinja");
    EXPECT_TRUE(caps.supports_system_role);
    EXPECT_TRUE(caps.supports_tools);
    EXPECT_TRUE(caps.supports_tool_calls);
    EXPECT_TRUE(caps.supports_tool_responses);
    EXPECT_TRUE(caps.supports_parallel_tool_calls);
    EXPECT_TRUE(caps.requires_object_arguments);
    // EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_FALSE(caps.requires_typed_content);
}
