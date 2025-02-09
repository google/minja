# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT

add_executable(test-syntax test-syntax.cpp)
target_compile_features(test-syntax PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-syntax PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gtest PRIVATE -Wno-language-extension-token)
endif()
target_link_libraries(test-syntax PRIVATE
    nlohmann_json::nlohmann_json
    gtest_main
    gmock
)

add_executable(test-polyfills test-polyfills.cpp)
target_compile_features(test-polyfills PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-polyfills PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gtest PRIVATE -Wno-language-extension-token)
endif()
target_link_libraries(test-polyfills PRIVATE
    nlohmann_json::nlohmann_json
    gtest_main
    gmock
)
if (NOT CMAKE_CROSSCOMPILING)
    gtest_discover_tests(test-syntax)
    add_test(NAME test-polyfills COMMAND test-polyfills)
    set_tests_properties(test-polyfills PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

add_executable(test-capabilities test-capabilities.cpp)
target_compile_features(test-capabilities PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-capabilities PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gtest PRIVATE -Wno-language-extension-token)
endif()
target_link_libraries(test-capabilities PRIVATE
    nlohmann_json::nlohmann_json
    gtest_main
    gmock
)
add_test(NAME test-capabilities COMMAND test-capabilities)
set_tests_properties(test-capabilities PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME test-syntax-jinja2 COMMAND test-syntax)
set_tests_properties(test-syntax-jinja2 PROPERTIES ENVIRONMENT "USE_JINJA2=1;PYTHON_EXECUTABLE=${Python_EXECUTABLE};PYTHONPATH=${CMAKE_SOURCE_DIR}")


add_executable(test-supported-template test-supported-template.cpp)
target_compile_features(test-supported-template PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-supported-template PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
target_link_libraries(test-supported-template PRIVATE nlohmann_json::nlohmann_json)

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
    google/gemma-2-2b-it # Gated
    google/gemma-7b-it # Gated
    BEE-spoke-data/tFINE-900m-instruct-orpo
    MiniMaxAI/MiniMax-Text-01
    indischepartij/MiniCPM-3B-OpenHermes-2.5-v2
    mattshumer/Reflection-Llama-3.1-70B
    meetkai/functionary-medium-v3.1
    meetkai/functionary-medium-v3.2
    meta-llama/Llama-3.1-8B-Instruct # Gated
    meta-llama/Llama-3.2-3B-Instruct # Gated
    meta-llama/Llama-3.3-70B-Instruct # Gated
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
    # meetkai/functionary-medium-v3.1      # jinja2 expectation is computed w/ wrong escapes
    # fireworks-ai/llama-3-firefunction-v2 # https://github.com/google/minja/issues/7
    # ai21labs/AI21-Jamba-1.5-Large        # https://github.com/google/minja/issues/8
)

if(NOT WIN32)
    list(APPEND MODEL_IDS
        # Needs investigation (https://github.com/google/minja/issues/40)
        CohereForAI/c4ai-command-r7b-12-2024 # Gated
        deepseek-ai/deepseek-coder-33b-instruct
        deepseek-ai/DeepSeek-Coder-V2-Instruct
        deepseek-ai/DeepSeek-V2.5
        deepseek-ai/DeepSeek-R1-Distill-Llama-8B
        deepseek-ai/DeepSeek-R1-Distill-Qwen-7B
        deepseek-ai/DeepSeek-R1-Distill-Qwen-32B
    )
endif()

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
    add_test(NAME test-supported-template-${test_name} COMMAND $<TARGET_FILE:test-supported-template> ${test_args})
    set_tests_properties(test-supported-template-${test_name} PROPERTIES SKIP_RETURN_CODE 127)
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
    if (NOT CMAKE_CROSSCOMPILING)
        gtest_discover_tests(test-fuzz)
    endif()
endif()
