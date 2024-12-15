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
    target_link_libraries(${example} PRIVATE nlohmann_json::nlohmann_json)

endforeach()
