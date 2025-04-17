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
    minja
    gtest_main
    gmock
)

if (WIN32)
    message(STATUS "Skipping test-chat-template on Win32")
else()
    add_executable(test-chat-template test-chat-template.cpp)
    target_compile_features(test-chat-template PUBLIC cxx_std_17)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        target_compile_definitions(test-chat-template PUBLIC _CRT_SECURE_NO_WARNINGS)
        target_compile_options(gtest PRIVATE -Wno-language-extension-token)
    endif()
    target_link_libraries(test-chat-template PRIVATE
        minja
        gtest_main
        gmock
    )
endif()

add_executable(test-polyfills test-polyfills.cpp)
target_compile_features(test-polyfills PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-polyfills PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gtest PRIVATE -Wno-language-extension-token)
endif()
target_link_libraries(test-polyfills PRIVATE
    minja
    gtest_main
    gmock
)
if (NOT CMAKE_CROSSCOMPILING)
    gtest_discover_tests(test-syntax)
    if (NOT WIN32)
        gtest_discover_tests(test-chat-template)
    endif()
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
    minja
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
target_link_libraries(test-supported-template PRIVATE minja)

# https://huggingface.co/models?other=conversational
# https://huggingface.co/spaces/open-llm-leaderboard/open_llm_leaderboard#/?types=fine-tuned%2Cchat

set(MODEL_IDS
    # List of model IDs to test the chat template of.
    # For each of them, the tokenizer_config.json file will be fetched, and the template
    # will be used to render each of the (relevant) test contexts into a golden file with
    # the official Python jinja2 library. Then a test case will be created to run the C++
    # minja implementation on the same template and context, and compare the output with the golden.
    #
    # For Gated models, you'll need to run `huggingface-cli login` (and be granted access) to download their template.

    abacusai/Fewshot-Metamath-OrcaVicuna-Mistral
    allenai/Llama-3.1-Tulu-3-405B
    allenai/Llama-3.1-Tulu-3-405B-SFT
    allenai/Llama-3.1-Tulu-3-8B
    arcee-ai/Virtuoso-Lite
    arcee-ai/Virtuoso-Medium-v2
    arcee-ai/Virtuoso-Small-v2
    AtlaAI/Selene-1-Mini-Llama-3.1-8B
    avemio/GRAG-NEMO-12B-ORPO-HESSIAN-AI
    BEE-spoke-data/tFINE-900m-instruct-orpo
    bespokelabs/Bespoke-Stratos-7B
    bfuzzy1/acheron-m1a-llama
    bofenghuang/vigogne-2-70b-chat
    bytedance-research/UI-TARS-72B-DPO
    bytedance-research/UI-TARS-7B-DPO
    bytedance-research/UI-TARS-7B-SFT
    carsenk/phi3.5_mini_exp_825_uncensored
    CohereForAI/aya-expanse-8b
    CohereForAI/c4ai-command-r-plus
    CohereForAI/c4ai-command-r7b-12-2024
    cyberagent/DeepSeek-R1-Distill-Qwen-14B-Japanese
    cyberagent/DeepSeek-R1-Distill-Qwen-32B-Japanese
    databricks/dbrx-instruct
    DavieLion/Llama-3.2-1B-SPIN-iter3
    deepseek-ai/deepseek-coder-33b-instruct
    deepseek-ai/deepseek-coder-6.7b-instruct
    deepseek-ai/deepseek-coder-7b-instruct-v1.5
    deepseek-ai/DeepSeek-Coder-V2-Instruct
    deepseek-ai/DeepSeek-Coder-V2-Lite-Base
    deepseek-ai/DeepSeek-Coder-V2-Lite-Instruct
    deepseek-ai/deepseek-llm-67b-chat
    deepseek-ai/deepseek-llm-7b-chat
    deepseek-ai/DeepSeek-R1-Distill-Llama-70B
    deepseek-ai/DeepSeek-R1-Distill-Llama-8B
    deepseek-ai/DeepSeek-R1-Distill-Qwen-1.5B
    deepseek-ai/DeepSeek-R1-Distill-Qwen-14B
    deepseek-ai/DeepSeek-R1-Distill-Qwen-32B
    deepseek-ai/DeepSeek-R1-Distill-Qwen-7B
    deepseek-ai/DeepSeek-V2-Lite
    deepseek-ai/DeepSeek-V2.5
    deepseek-ai/DeepSeek-V3
    Delta-Vector/Rei-12B
    dicta-il/dictalm2.0-instruct
    ehristoforu/Falcon3-8B-Franken-Basestruct
    EpistemeAI/Mistral-Nemo-Instruct-12B-Philosophy-Math
    FlofloB/83k_continued_pretraining_Qwen2.5-0.5B-Instruct_Unsloth_merged_16bit
    FlofloB/test_continued_pretraining_Phi-3-mini-4k-instruct_Unsloth_merged_16bit
    godlikehhd/alpaca_data_sampled_ifd_new_5200
    godlikehhd/alpaca_data_score_max_0.7_2600
    google/gemma-2-27b-it
    google/gemma-2-2b-it
    google/gemma-2-2b-jpn-it
    google/gemma-7b-it
    HelpingAI/HAI-SER
    HuggingFaceTB/SmolLM2-1.7B-Instruct
    HuggingFaceTB/SmolLM2-135M-Instruct
    HuggingFaceTB/SmolLM2-360M-Instruct
    huihui-ai/DeepSeek-R1-Distill-Llama-70B-abliterated
    huihui-ai/DeepSeek-R1-Distill-Llama-8B-abliterated
    huihui-ai/DeepSeek-R1-Distill-Qwen-14B-abliterated-v2
    huihui-ai/DeepSeek-R1-Distill-Qwen-32B-abliterated
    huihui-ai/DeepSeek-R1-Distill-Qwen-7B-abliterated-v2
    huihui-ai/Qwen2.5-14B-Instruct-1M-abliterated
    ibm-granite/granite-3.1-8b-instruct
    Ihor/Text2Graph-R1-Qwen2.5-0.5b
    inclusionAI/Ling-Coder-lite
    indischepartij/MiniCPM-3B-OpenHermes-2.5-v2
    Infinigence/Megrez-3B-Instruct
    inflatebot/MN-12B-Mag-Mell-R1
    INSAIT-Institute/BgGPT-Gemma-2-27B-IT-v1.0
    jinaai/ReaderLM-v2
    Josephgflowers/TinyLlama_v1.1_math_code-world-test-1
    kms7530/chemeng_qwen-math-7b_24_1_100_1_nonmath
    knifeayumu/Cydonia-v1.3-Magnum-v4-22B
    langgptai/qwen1.5-7b-chat-sa-v0.1
    LatitudeGames/Wayfarer-12B
    LGAI-EXAONE/EXAONE-3.5-2.4B-Instruct
    LGAI-EXAONE/EXAONE-3.5-7.8B-Instruct
    lightblue/DeepSeek-R1-Distill-Qwen-7B-Japanese
    Magpie-Align/Llama-3-8B-Magpie-Align-v0.1
    Magpie-Align/Llama-3.1-8B-Magpie-Align-v0.1
    mattshumer/Reflection-Llama-3.1-70B
    MaziyarPanahi/calme-3.2-instruct-78b
    meetkai/functionary-medium-v3.1
    meetkai/functionary-medium-v3.2
    meta-llama/Llama-2-7b-chat-hf
    meta-llama/Llama-3.1-8B-Instruct
    meta-llama/Llama-3.2-1B-Instruct
    meta-llama/Llama-3.2-3B-Instruct
    meta-llama/Llama-3.3-70B-Instruct
    meta-llama/Meta-Llama-3-8B-Instruct
    meta-llama/Meta-Llama-3.1-8B-Instruct
    microsoft/Phi-3-medium-4k-instruct
    microsoft/Phi-3-mini-4k-instruct
    microsoft/Phi-3-small-8k-instruct
    microsoft/Phi-3.5-mini-instruct
    microsoft/Phi-3.5-vision-instruct
    microsoft/phi-4
    migtissera/Tess-3-Mistral-Nemo-12B
    MiniMaxAI/MiniMax-Text-01
    MiniMaxAI/MiniMax-VL-01
    ministral/Ministral-3b-instruct
    mistralai/Codestral-22B-v0.1
    mistralai/Mistral-7B-Instruct-v0.1
    mistralai/Mistral-7B-Instruct-v0.2
    mistralai/Mistral-7B-Instruct-v0.3
    mistralai/Mistral-Large-Instruct-2407
    mistralai/Mistral-Large-Instruct-2411
    mistralai/Mistral-Nemo-Instruct-2407
    mistralai/Mistral-Small-24B-Instruct-2501
    mistralai/Mixtral-8x7B-Instruct-v0.1
    mkurman/Qwen2.5-14B-DeepSeek-R1-1M
    mlabonne/AlphaMonarch-7B
    mlx-community/Josiefied-Qwen2.5-0.5B-Instruct-abliterated-v1-float32
    mlx-community/Qwen2.5-VL-7B-Instruct-8bit
    mobiuslabsgmbh/DeepSeek-R1-ReDistill-Qwen-1.5B-v1.1
    NaniDAO/deepseek-r1-qwen-2.5-32B-ablated
    netcat420/MFANNv0.20
    netcat420/MFANNv0.24
    netease-youdao/Confucius-o1-14B
    NexaAIDev/Octopus-v2
    NousResearch/Hermes-2-Pro-Llama-3-8B
    NousResearch/Hermes-2-Pro-Mistral-7B
    NousResearch/Hermes-3-Llama-3.1-70B
    NovaSky-AI/Sky-T1-32B-Flash
    NovaSky-AI/Sky-T1-32B-Preview
    nvidia/AceMath-7B-RM
    nvidia/Eagle2-1B
    nvidia/Eagle2-9B
    nvidia/Llama-3.1-Nemotron-70B-Instruct-HF
    OnlyCheeini/greesychat-turbo
    onnx-community/DeepSeek-R1-Distill-Qwen-1.5B-ONNX
    open-thoughts/OpenThinker-7B
    openchat/openchat-3.5-0106
    Orenguteng/Llama-3.1-8B-Lexi-Uncensored-V2
    OrionStarAI/Orion-14B-Chat
    pankajmathur/orca_mini_v6_8b
    PowerInfer/SmallThinker-3B-Preview
    PrimeIntellect/INTELLECT-1-Instruct
    princeton-nlp/Mistral-7B-Base-SFT-RDPO
    princeton-nlp/Mistral-7B-Instruct-DPO
    princeton-nlp/Mistral-7B-Instruct-RDPO
    prithivMLmods/Bellatrix-Tiny-1.5B-R1
    prithivMLmods/Bellatrix-Tiny-1B-R1
    prithivMLmods/Bellatrix-Tiny-1B-v3
    prithivMLmods/Bellatrix-Tiny-3B-R1
    prithivMLmods/Blaze-14B-xElite
    prithivMLmods/Calcium-Opus-14B-Elite2-R1
    prithivMLmods/Calme-Ties-78B
    prithivMLmods/Calme-Ties2-78B
    prithivMLmods/Calme-Ties3-78B
    prithivMLmods/ChemQwen2-vL
    prithivMLmods/GWQ2b
    prithivMLmods/LatexMind-2B-Codec
    prithivMLmods/Llama-3.2-6B-AlgoCode
    prithivMLmods/Megatron-Opus-14B-Exp
    prithivMLmods/Megatron-Opus-14B-Stock
    prithivMLmods/Megatron-Opus-7B-Exp
    prithivMLmods/Omni-Reasoner-Merged
    prithivMLmods/Omni-Reasoner4-Merged
    prithivMLmods/Primal-Opus-14B-Optimus-v1
    prithivMLmods/Qwen-7B-Distill-Reasoner
    prithivMLmods/Qwen2.5-1.5B-DeepSeek-R1-Instruct
    prithivMLmods/Qwen2.5-14B-DeepSeek-R1-1M
    prithivMLmods/Qwen2.5-32B-DeepSeek-R1-Instruct
    prithivMLmods/Qwen2.5-7B-DeepSeek-R1-1M
    prithivMLmods/QwQ-Math-IO-500M
    prithivMLmods/Triangulum-v2-10B
    qingy2024/Falcon3-2x10B-MoE-Instruct
    Qwen/QVQ-72B-Preview
    Qwen/Qwen1.5-7B-Chat
    Qwen/Qwen2-7B-Instruct
    Qwen/Qwen2-VL-72B-Instruct
    Qwen/Qwen2-VL-7B-Instruct
    Qwen/Qwen2.5-0.5B
    Qwen/Qwen2.5-1.5B-Instruct
    Qwen/Qwen2.5-14B
    Qwen/Qwen2.5-14B-Instruct-1M
    Qwen/Qwen2.5-32B
    Qwen/Qwen2.5-32B-Instruct
    Qwen/Qwen2.5-3B-Instruct
    Qwen/Qwen2.5-72B-Instruct
    Qwen/Qwen2.5-7B
    Qwen/Qwen2.5-7B-Instruct
    Qwen/Qwen2.5-7B-Instruct-1M
    Qwen/Qwen2.5-Coder-32B-Instruct
    Qwen/Qwen2.5-Coder-7B-Instruct
    Qwen/Qwen2.5-Math-1.5B
    Qwen/Qwen2.5-Math-7B-Instruct
    Qwen/Qwen2.5-VL-3B-Instruct
    Qwen/Qwen2.5-VL-72B-Instruct
    Qwen/Qwen2.5-VL-7B-Instruct
    Qwen/QwQ-32B
    Qwen/QwQ-32B-Preview
    rubenroy/Zurich-14B-GCv2-5m
    rubenroy/Zurich-7B-GCv2-5m
    RWKV-Red-Team/ARWKV-7B-Preview-0.1
    SakanaAI/TinySwallow-1.5B
    SakanaAI/TinySwallow-1.5B-Instruct
    Sao10K/70B-L3.3-Cirrus-x1
    SentientAGI/Dobby-Mini-Leashed-Llama-3.1-8B
    SentientAGI/Dobby-Mini-Unhinged-Llama-3.1-8B
    silma-ai/SILMA-Kashif-2B-Instruct-v1.0
    simplescaling/s1-32B
    sometimesanotion/Lamarck-14B-v0.7
    sonthenguyen/zephyr-sft-bnb-4bit-DPO-mtbr-180steps
    Steelskull/L3.3-Damascus-R1
    Steelskull/L3.3-MS-Nevoria-70b
    Steelskull/L3.3-Nevoria-R1-70b
    sthenno/tempesthenno-icy-0130
    sumink/qwft
    Tarek07/Progenitor-V1.1-LLaMa-70B
    teknium/OpenHermes-2.5-Mistral-7B
    TheBloke/FusionNet_34Bx2_MoE-AWQ
    thirdeyeai/elevate360m
    THUDM/glm-4-9b-chat
    THUDM/glm-edge-1.5b-chat
    tiiuae/Falcon3-10B-Instruct
    TinyLlama/TinyLlama-1.1B-Chat-v1.0
    UCLA-AGI/Mistral7B-PairRM-SPPO-Iter3
    unsloth/DeepSeek-R1-Distill-Llama-8B
    unsloth/DeepSeek-R1-Distill-Llama-8B-unsloth-bnb-4bit
    unsloth/Mistral-Small-24B-Instruct-2501-unsloth-bnb-4bit
    upstage/solar-pro-preview-instruct
    ValiantLabs/Llama3.1-8B-Enigma
    xwen-team/Xwen-72B-Chat
    xwen-team/Xwen-7B-Chat

    # Broken, TODO:
    # ai21labs/AI21-Jamba-1.5-Large        # https://github.com/google/minja/issues/8
    # Almawave/Velvet-14B
    # deepseek-ai/DeepSeek-R1
    # deepseek-ai/DeepSeek-R1-Zero
    # fireworks-ai/llama-3-firefunction-v2 # https://github.com/google/minja/issues/7
    # HuggingFaceTB/SmolVLM-256M-Instruct
    # HuggingFaceTB/SmolVLM-500M-Instruct
    # HuggingFaceTB/SmolVLM-Instruct
    # meta-llama/Llama-3.2-11B-Vision-Instruct
    # unsloth/DeepSeek-R1
)

if(WIN32)
    list(REMOVE_ITEM MODEL_IDS
        # Needs investigation (https://github.com/google/minja/issues/40)
        CohereForAI/c4ai-command-r7b-12-2024    
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
