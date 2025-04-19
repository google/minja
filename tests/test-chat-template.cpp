
/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja/chat-template.hpp"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <fstream>
#include <iostream>
#include <string>

using namespace minja;
using namespace testing;

static std::string render_python(const std::string & template_str, const chat_template_inputs & inputs) {
    json bindings = inputs.extra_context;
    bindings["messages"] = inputs.messages;
    bindings["tools"] = inputs.tools;
    bindings["add_generation_prompt"] = inputs.add_generation_prompt;
    json data {
        {"template", template_str},
        {"bindings", bindings},
        {"options", {
            {"trim_blocks", true},
            {"lstrip_blocks", true},
            {"keep_trailing_newline", false},
        }},
    };
    {
        std::ofstream of("data.json");
        of << data.dump(2);
        of.close();
    }

    auto pyExeEnv = getenv("PYTHON_EXECUTABLE");
    std::string pyExe = pyExeEnv ? pyExeEnv : "python3";

    std::remove("out.txt");
    auto res = std::system((pyExe + " -m scripts.render data.json out.txt").c_str());
    if (res != 0) {
        throw std::runtime_error("Failed to run python script with data: " + data.dump(2));
    }

    std::ifstream f("out.txt");
    std::string out((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return out;
}

static std::string render(const std::string & template_str, const chat_template_inputs & inputs, const chat_template_options & opts) {
  if (getenv("USE_JINJA2")) {
      return render_python(template_str, inputs);
  }
  chat_template tmpl(
      template_str,
      "",
      "");
  return tmpl.apply(inputs, opts);
}

TEST(ChatTemplateTest, SimpleCases) {
    EXPECT_THAT(render("{{ strftime_now('%Y-%m-%d %H:%M:%S') }}", {}, {}), MatchesRegex(R"([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2})"));
}
