# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
foreach(example
    chat-template
    raw
)
    add_executable(${example} ${example}.cpp)
    target_compile_features(${example} PUBLIC cxx_std_17)
    target_link_libraries(${example} PRIVATE minja)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        target_compile_definitions(${example} PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()

endforeach()
