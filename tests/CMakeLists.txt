# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT

add_executable(test-syntax test-syntax.cpp)
target_compile_features(test-syntax PUBLIC cxx_std_17)
target_link_libraries(test-syntax PRIVATE
    nlohmann_json::nlohmann_json
    gtest_main
    gmock
)
gtest_discover_tests(test-syntax)

add_executable(test-chat-template test-chat-template.cpp)
target_compile_features(test-chat-template PUBLIC cxx_std_17)
target_link_libraries(test-chat-template PRIVATE nlohmann_json::nlohmann_json)

set(MODEL_IDS
    # List of model IDs to test the chat template of.
    # For each of them, the tokenizer_config.json file will be fetched, and the template
    # will be used to render each of the (relevant) test contexts into a golden file with
    # the official Python jinja2 library. Then a test case will be created to run the C++
    # minja implementation on the same template and context, and compare the output with the golden.
    #
    # For Gated models, you'll need to run `huggingface-cli login` (and be granted access) to download their template.
    
    abacusai/Fewshot-Metamath-OrcaVicuna-Mistral
    bofenghuang/vigogne-2-70b-chat
    CohereForAI/c4ai-command-r-plus # Gated
    databricks/dbrx-instruct # Gated
    deepseek-ai/deepseek-coder-33b-instruct
    deepseek-ai/DeepSeek-Coder-V2-Instruct
    deepseek-ai/DeepSeek-V2.5
    google/gemma-2-2b-it # Gated
    google/gemma-7b-it # Gated
    indischepartij/MiniCPM-3B-OpenHermes-2.5-v2
    mattshumer/Reflection-Llama-3.1-70B
    meetkai/functionary-medium-v3.1
    meetkai/functionary-medium-v3.2
    meta-llama/Llama-3.2-3B-Instruct # Gated
    meta-llama/Meta-Llama-3.1-8B-Instruct # Gated
    microsoft/Phi-3-medium-4k-instruct
    microsoft/Phi-3-mini-4k-instruct
    microsoft/Phi-3-small-8k-instruct
    microsoft/Phi-3.5-mini-instruct
    microsoft/Phi-3.5-vision-instruct
    mistralai/Mistral-7B-Instruct-v0.2 # Gated
    mistralai/Mistral-Large-Instruct-2407 # Gated
    mistralai/Mistral-Large-Instruct-2411 # Gated
    mistralai/Mistral-Nemo-Instruct-2407 # Gated
    mistralai/Mixtral-8x7B-Instruct-v0.1 # Gated
    mlabonne/AlphaMonarch-7B
    NexaAIDev/Octopus-v2
    NousResearch/Hermes-2-Pro-Llama-3-8B
    NousResearch/Hermes-2-Pro-Mistral-7B
    NousResearch/Hermes-3-Llama-3.1-70B
    nvidia/Llama-3.1-Nemotron-70B-Instruct-HF
    openchat/openchat-3.5-0106
    OrionStarAI/Orion-14B-Chat
    Qwen/Qwen2-7B-Instruct
    Qwen/Qwen2-VL-7B-Instruct
    Qwen/Qwen2.5-7B-Instruct
    Qwen/Qwen2.5-Math-7B-Instruct
    Qwen/QwQ-32B-Preview
    teknium/OpenHermes-2.5-Mistral-7B
    TheBloke/FusionNet_34Bx2_MoE-AWQ

    # Broken, TODO:
    # fireworks-ai/llama-3-firefunction-v2 

    # Can't find template(s), TODO:
    # ai21labs/Jamba-v0.1
    # apple/OpenELM-1_1B-Instruct 
    # dreamgen/WizardLM-2-7B
    # xai-org/grok-1
)

# Create one test case for each {template, context} combination
file(GLOB CONTEXT_FILES "${CMAKE_SOURCE_DIR}/tests/contexts/*.json")
execute_process(
    COMMAND ${Python_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/fetch_templates_and_goldens.py
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CONTEXT_FILES}
        ${MODEL_IDS}
    OUTPUT_VARIABLE CHAT_TEMPLATE_TEST_CASES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
string(REPLACE "\n" ";" CHAT_TEMPLATE_TEST_CASES "${CHAT_TEMPLATE_TEST_CASES}")
list(LENGTH CHAT_TEMPLATE_TEST_CASES CHAT_TEMPLATE_TEST_CASES_COUNT)
message(STATUS "Found ${CHAT_TEMPLATE_TEST_CASES_COUNT} chat template test cases")
if (CHAT_TEMPLATE_TEST_CASES_COUNT LESS 10)
    message(ERROR "Not enough chat template test cases found")
endif()
foreach(test_case ${CHAT_TEMPLATE_TEST_CASES})
    separate_arguments(test_args UNIX_COMMAND "${test_case}")
    list(GET test_args -1 last_arg)
    string(REGEX REPLACE "^[^ ]+/([^ /\\]+)\\.[^.]+$" "\\1" test_name "${last_arg}")
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:test-chat-template> ${test_args})
    set_tests_properties(${test_name} PROPERTIES SKIP_RETURN_CODE 127)
endforeach()

if (MINJA_FUZZTEST_ENABLED)
    if (MINJA_FUZZTEST_FUZZING_MODE)
        message(STATUS "Fuzzing mode enabled")
        fuzztest_setup_fuzzing_flags()
    endif()
    add_executable(test-fuzz test-fuzz.cpp)
    target_compile_features(test-fuzz PUBLIC cxx_std_17)
    target_include_directories(test-fuzz PRIVATE ${fuzztest_BINARY_DIR})
    target_link_libraries(test-fuzz PRIVATE nlohmann_json::nlohmann_json)
    link_fuzztest(test-fuzz)
    gtest_discover_tests(test-fuzz)
endif()
