/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include <minja/minja.hpp>
#include <iostream>

using json = nlohmann::ordered_json;

int main() {
    auto tmpl = minja::Parser::parse("Hello, {{ location }}!", /* options= */ {});
    auto context = minja::Context::make(minja::Value(json {
        {"location", "World"},
    }));
    auto result = tmpl->render(context);
    std::cout << result << std::endl;
}
