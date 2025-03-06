/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <fstream>
#include <iostream>
#include <string>

static std::string render_python(const std::string & template_str, const json & bindings, const minja::Options & options) {
    json data {
        {"template", template_str},
        {"bindings", bindings.is_null() ? json::object() : bindings},
        {"options", {
            {"trim_blocks", options.trim_blocks},
            {"lstrip_blocks", options.lstrip_blocks},
            {"keep_trailing_newline", options.keep_trailing_newline},
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

static std::string render(const std::string & template_str, const json & bindings, const minja::Options & options) {
    if (getenv("USE_JINJA2")) {
        return render_python(template_str, bindings, options);
    }
    auto root = minja::Parser::parse(template_str, options);
    auto context = minja::Context::make(bindings);
    return root->render(context);
}

const minja::Options lstrip_blocks {
    /* .trim_blocks = */ false,
    /* .lstrip_blocks = */ true,
    /* .keep_trailing_newline = */ false,
};
const minja::Options trim_blocks {
    /* .trim_blocks = */ true,
    /* .lstrip_blocks = */ false,
    /* .keep_trailing_newline = */ false,
};
const minja::Options lstrip_trim_blocks {
    /* .trim_blocks = */ true,
    /* .lstrip_blocks = */ true,
    /* .keep_trailing_newline = */ false,
};

TEST(SyntaxTest, SimpleCases) {
    auto ThrowsWithSubstr = [](const std::string & expected_substr) {
        return testing::Throws<std::runtime_error>(Property(&std::runtime_error::what, testing::HasSubstr(expected_substr)));
    };

    EXPECT_EQ("a", render("{{ ' a '.strip() }}", {}, {}));
    EXPECT_EQ("a ", render("{{ ' a '.lstrip() }}", {}, {}));
    EXPECT_EQ(" a", render("{{ ' a '.rstrip() }}", {}, {}));
    EXPECT_EQ("bcXYZab", render("{{ 'abcXYZabc'.strip('ac') }}", {}, {}));

    EXPECT_EQ(R"(["a", "b"])", render("{{ 'a b'.split(' ') | tojson }}", {}, {}));

    EXPECT_EQ(
        "Ok",
        render("{{ 'ok'.capitalize() }}", {}, {}));

    EXPECT_EQ(
        "ok",
        render("{# Hey\nHo #}{#- Multiline...\nComments! -#}{{ 'ok' }}{# yo #}", {}, {}));
    
    EXPECT_EQ(
        "    b",
        render(R"(  {% set _ = 1 %}    {% set _ = 2 %}b)", {}, lstrip_trim_blocks));
    EXPECT_EQ(
        "        1",
        render(R"({%- if True %}        {% set _ = x %}{%- endif %}{{ 1 }})", {}, lstrip_trim_blocks));

    EXPECT_EQ("\n",       render("    {% if True %}\n    {% endif %}", {}, lstrip_blocks));
    EXPECT_EQ("",         render("    {% if True %}\n    {% endif %}", {}, lstrip_trim_blocks));
    EXPECT_EQ("        ", render("    {% if True %}\n    {% endif %}", {}, trim_blocks));

    EXPECT_EQ("      ", render("  {% set _ = 1 %}    ", {}, {}));
    EXPECT_EQ("    ",   render("  {% set _ = 1 %}    ", {}, lstrip_blocks));
    EXPECT_EQ("      ", render("  {% set _ = 1 %}    ", {}, trim_blocks));
    EXPECT_EQ("    ",   render("  {% set _ = 1 %}    ", {}, lstrip_trim_blocks));

    EXPECT_EQ("  \n            \n                ", render("  \n    {% set _ = 1 %}        \n                ", {}, {}));
    EXPECT_EQ("  \n        \n                ",     render("  \n    {% set _ = 1 %}        \n                ", {}, lstrip_blocks));
    EXPECT_EQ("  \n            \n                ", render("  \n    {% set _ = 1 %}        \n                ", {}, trim_blocks));
    EXPECT_EQ("  \n        \n                ",     render("  \n    {% set _ = 1 %}        \n                ", {}, lstrip_trim_blocks));

    EXPECT_EQ("\n  ", render("{% set _ = 1 %}\n  ", {}, {}));
    EXPECT_EQ("\n  ", render("{% set _ = 1 %}\n  ", {}, lstrip_blocks));
    EXPECT_EQ("  ",   render("{% set _ = 1 %}\n  ", {}, trim_blocks));
    EXPECT_EQ("  ",   render("{% set _ = 1 %}\n  ", {}, lstrip_trim_blocks));

    EXPECT_EQ(
        "[2, 3]",
        render("{{ range(*[2,4]) | list }}", {}, {}));
    EXPECT_EQ(
        "1, 0, 10, -10, 10, -10, 0, 0, 2, 0, 0, ",
        render("{% for i in [true, false, 10, -10, 10.1, -10.1, None, 'a', '2', {}, [1]] %}{{ i | int }}, {% endfor %}", {}, {}));
    EXPECT_EQ(
        "abc",
        render("{% filter trim %} abc {% endfilter %}", {}, {}));
    EXPECT_EQ(
        "[1, 2, 3]",
        render("{{ [1] + [2, 3] }}", {}, {}));
    EXPECT_EQ(
        "abc",
        render("{{ 'AbC' | lower }}", {}, {}));
    EXPECT_EQ(
        "the default1",
        render("{{ foo | default('the default') }}{{ 1 | default('nope') }}", {}, {}));
    EXPECT_EQ(
        "the default1",
        render("{{ '' | default('the default', true) }}{{ 1 | default('nope', true) }}", {}, {}));
    EXPECT_EQ(
        "a\n  b\n|  a\n  b\n",
        render("{% set txt = 'a\\nb\\n' %}{{ txt | indent(2) }}|{{ txt | indent(2, first=true) }}", {}, {}));
    EXPECT_EQ(
        "        1",
        render(R"({%- if True %}        {% set _ = x %}{%- endif %}{{ 1 }})", {}, lstrip_trim_blocks));
    EXPECT_EQ(
        "a  b",
        render(R"(  {{- 'a' -}}{{ '  ' }}{{- 'b' -}}  )", {}, {}));
    EXPECT_EQ(
        "bc",
        render(R"({{ "abcd"[1:-1] }})", {}, {}));
    EXPECT_EQ(
        "[1, 2]",
        render(R"({{ [0, 1, 2, 3][1:-1] }})", {}, {}));
    EXPECT_EQ(
        "9",
        render(R"({{ "123456789" | length }})", {}, {}));
    EXPECT_EQ(
        "        end",
        render(R"(    {%- if True %}{%- endif %}{{ '        ' }}{%- for x in [] %}foo{% endfor %}end)", {}, {}));
    EXPECT_EQ(
        "False",
        render(R"({% set ns = namespace(is_first=false, nottool=false, and_or=true, delme='') %}{{ ns.is_first }})", {}, {}));
    EXPECT_EQ(
        "True,False",
        render(R"({{ {} is mapping }},{{ '' is mapping }})", {}, {}));
    EXPECT_EQ(
        "True,True",
        render(R"({{ {} is iterable }},{{ '' is iterable }})", {}, {}));
    EXPECT_EQ(
        "a,b,",
        render(R"({% for x in ["a", "b"] %}{{ x }},{% endfor %})", {}, {}));
    EXPECT_EQ(
        "a,b,",
        render(R"({% for x in {"a": 1, "b": 2} %}{{ x }},{% endfor %})", {}, {}));
    EXPECT_EQ(
        "a,b,",
        render(R"({% for x in "ab" %}{{ x }},{% endfor %})", {}, {}));
    EXPECT_EQ(
        "Foo Bar",
        render(R"({{ 'foo bar'.title() }})", {}, {}));
    EXPECT_EQ(
        "1",
        render(R"({{ 1 | safe }})", {}, {}));
    EXPECT_EQ(
        "True,False",
        render(R"({{ 'abc'.endswith('bc') }},{{ ''.endswith('a') }})", {}, {}));
    EXPECT_EQ(
        "[]",
        render(R"({{ none | selectattr("foo", "equalto", "bar") | list }})", {}, {}));
    EXPECT_EQ(
        "True,False",
        render(R"({{ 'a' in {"a": 1} }},{{ 'a' in {} }})", {}, {}));
    EXPECT_EQ(
        "True,False",
        render(R"({{ 'a' in ["a"] }},{{ 'a' in [] }})", {}, {}));
    EXPECT_EQ(
        R"([{'a': 1}])",
        render(R"({{ [{"a": 1}, {"a": 2}, {}] | selectattr("a", "equalto", 1) | list }})", {}, {}));
    EXPECT_EQ(
        R"([{'a': 2}, {}])",
        render(R"({{ [{"a": 1}, {"a": 2}, {}] | rejectattr("a", "equalto", 1) | list }})", {}, {}));
    EXPECT_EQ(
        "[1, 2]",
        render(R"({{ [{"a": 1}, {"a": 2}] | map(attribute="a") | list }})", {}, {}));
    EXPECT_EQ(
        "[0, 1]",
        render(R"({{ ["", "a"] | map("length") | list }})", {}, {}));
    EXPECT_EQ(
        "2",
        render(R"({{ range(3) | last }})", {}, {}));
    EXPECT_EQ(
        "True",
        render(R"({% set foo = true %}{{ foo is defined }})", {}, {}));
    EXPECT_EQ(
        "False",
        render(R"({% set foo = true %}{{ not foo is defined }})", {}, {}));
    EXPECT_EQ(
        R"({"a": "b"})",
        render(R"({{ {"a": "b"} | tojson }})", {}, {}));
    EXPECT_EQ(
        R"({'a': 'b'})",
        render(R"({{ {"a": "b"} }})", {}, {}));

    std::string trim_tmpl =
        "\n"
        "  {% if true %}Hello{% endif %}  \n"
        "...\n"
        "\n";
    EXPECT_EQ(
        "\n  Hello  \n...\n",
        render(trim_tmpl, {}, trim_blocks));
    EXPECT_EQ(
        "\n  Hello  \n...\n",
        render(trim_tmpl, {}, {}));
    EXPECT_EQ(
        "\nHello  \n...\n",
        render(trim_tmpl, {}, lstrip_blocks));
    EXPECT_EQ(
        "\nHello  \n...\n",
        render(trim_tmpl, {}, lstrip_trim_blocks));
    EXPECT_EQ(
        "a | b | c",
        render(R"({%- set separator = joiner(' | ') -%}
            {%- for item in ["a", "b", "c"] %}{{ separator() }}{{ item }}{% endfor -%})", {}, {}));
    EXPECT_EQ(
        "a\nb",
        render("a\nb\n", {}, {}));
    EXPECT_EQ(
        " a\n",
        render("  {{- ' a\n'}}", {}, trim_blocks));
    EXPECT_EQ(
        "but first, mojitos!1,2,3",
        render(R"(
            {%- for x in range(3) -%}
                {%- if loop.first -%}
                    but first, mojitos!
                {%- endif -%}
                {{ loop.index }}{{ "," if not loop.last -}}
            {%- endfor -%}
        )", {}, {}));
    EXPECT_EQ(
        "a0b",
        render("{{ 'a' + [] | length | string + 'b' }}", {}, {}));
    EXPECT_EQ(
        "1, 2, 3...",
        render("{{ [1, 2, 3] | join(', ') + '...' }}", {}, {}));
    EXPECT_EQ(
        "Tools: 1, 3...",
        render("{{ 'Tools: ' + [1, 2, 3] | reject('equalto', 2) | join(', ') + '...' }}", {}, {}));
    EXPECT_EQ(
        "Tools: 2...",
        render("{{ 'Tools: ' + [1, 2, 3] | select('equalto', 2) | join(', ') + '...' }}", {}, {}));
    EXPECT_EQ(
        "1, 2, 3",
        render("{{ [1, 2, 3] | join(', ') }}", {}, {}));
    EXPECT_EQ(
        "0,1,2,",
        render("{% for i in range(3) %}{{i}},{% endfor %}", {}, {}));
    EXPECT_EQ(
        "1Hello there2",
        render("{% set foo %}Hello {{ 'there' }}{% endset %}{{ 1 ~ foo ~ 2 }}", {}, {}));
    EXPECT_EQ(
        "[1, False, 2, '3']",
        render("{{ [1, False, 2, '3', 1, '3', False] | unique | list }}", {}, {}));
    EXPECT_EQ(
        "1",
        render("{{ range(5) | length % 2 }}", {}, {}));
    EXPECT_EQ(
        "True,False",
        render("{{ range(5) | length % 2 == 1 }},{{ [] | length > 0 }}", {}, {}));
    EXPECT_EQ(
        "False",
        render(
        "{{ messages[0]['role'] != 'system' }}",
        {{"messages", json::array({json({{"role", "system"}})})}},
        {}
    ));
    EXPECT_EQ(
        "a,b;c,d;",
        render(R"(
            {%- for x, y in [("a", "b"), ("c", "d")] -%}
                {{- x }},{{ y -}};
            {%- endfor -%}
        )", {}, {}));
    EXPECT_EQ(
        "True",
        render("{{ 1 is not string }}", {}, {}));
    EXPECT_EQ(
        "ababab",
        render("{{ 'ab' * 3 }}", {}, {}));
    EXPECT_EQ(
        "3",
        render("{{ [1, 2, 3][-1] }}", {}, {}));
    EXPECT_EQ(
        "OK",
        render("{%- for i in range(0) -%}NAH{% else %}OK{% endfor %}", {}, {}));
    EXPECT_EQ(
        "(0, odd),(1, even),(2, odd),(3, even),(4, odd),",
        render(R"(
            {%- for i in range(5) -%}
                ({{ i }}, {{ loop.cycle('odd', 'even') }}),
            {%- endfor -%}
        )", {}, {}));
    if (!getenv("USE_JINJA2")) {
        EXPECT_EQ(
            "0, first=True, last=False, index=1, index0=0, revindex=3, revindex0=2, prev=, next=2,\n"
            "2, first=False, last=False, index=2, index0=1, revindex=2, revindex0=1, prev=0, next=4,\n"
            "4, first=False, last=True, index=3, index0=2, revindex=1, revindex0=0, prev=2, next=,\n",
            render(
                "{%- for i in range(5) if i % 2 == 0 -%}\n"
                "{{ i }}, first={{ loop.first }}, last={{ loop.last }}, index={{ loop.index }}, index0={{ loop.index0 }}, revindex={{ loop.revindex }}, revindex0={{ loop.revindex0 }}, prev={{ loop.previtem }}, next={{ loop.nextitem }},\n"
                "{% endfor -%}",
                {}, {}
            )
        );
    }
    EXPECT_EQ(
        "[0, 1, 2][0, 2]",
        render(R"(
            {%- set o = [0, 1, 2, 3] -%}
            {%- set _ = o.pop() -%}
            {{- o | tojson -}}
            {%- set _ = o.pop(1) -%}
            {{- o | tojson -}}
        )", {}, {}));
    EXPECT_EQ(
        R"({"y": 2})",
        render(R"(
            {%- set o = {"x": 1, "y": 2} -%}
            {%- set _ = o.pop("x") -%}
            {{- o | tojson -}}
        )", {}, {}));
    EXPECT_EQ(
        R"(&lt;, &gt;, &amp;, &#34;)",
        render(R"(
            {%- set res = [] -%}
            {%- for c in ["<", ">", "&", '"'] -%}
                {%- set _ = res.append(c | e) -%}
            {%- endfor -%}
            {{- res | join(", ") -}}
        )", {}, {}));
    EXPECT_EQ(
        R"(x=100, y=2, z=3, w=10)",
        render(R"(
            {%- set x = 1 -%}
            {%- set y = 2 -%}
            {%- macro foo(x, z, w=10) -%}
                x={{ x }}, y={{ y }}, z={{ z }}, w={{ w -}}
            {%- endmacro -%}
            {{- foo(100, 3) -}}
        )", {}, {}));
    EXPECT_EQ(R"(
            <p><input type="text" name="username" value="" size="20"></p>
            <p><input type="password" name="password" value="" size="20"></p>)",
            render(R"(
            {% macro input(name, value='', type='text', size=20) -%}
                <input type="{{ type }}" name="{{ name }}" value="{{ value|e }}" size="{{ size }}">
            {%- endmacro -%}

            <p>{{ input('username') }}</p>
            <p>{{ input('password', type='password') }}</p>)", {}, {}));
    EXPECT_EQ(
        R"([1] [1])",
        render(R"(
            {#- The values' default array should be created afresh at each call, unlike the equivalent Python function -#}
            {%- macro foo(values=[]) -%}
                {%- set _ = values.append(1) -%}
                {{- values -}}
            {%- endmacro -%}
            {{- foo() }} {{ foo() -}})", {}, {}));

    if (!getenv("USE_JINJA2")) {
        EXPECT_EQ(
            "[]",
            render(R"({{ None | items | list | tojson }})", {}, {}));
        EXPECT_EQ(
            "Foo",
            render(R"({% generation %}Foo{% endgeneration %})", {}, {}));
    }
    EXPECT_EQ(
        "[[1, 2]]",
        render(R"({{ {1: 2} | items | list | tojson }})", {}, {}));
    EXPECT_EQ(
        "[[1, 2], [3, 4], [5, 7]]",
        render(R"({{ {1: 2, 3: 4, 5: 7} | dictsort | tojson }})", {}, {}));
    EXPECT_EQ(
        "[[1, 2]]",
        render(R"({{ {1: 2}.items() | map("list") | list }})", {}, {}));
    EXPECT_EQ(
        "2; ; 10",
        render(R"({{ {1: 2}.get(1) }}; {{ {}.get(1) or '' }}; {{ {}.get(1, 10) }})", {}, {}));
    EXPECT_EQ(
        R"(1,1.2,"a",true,true,false,false,null,[],[1],[1, 2],{},{"a": 1},{"1": "b"},)",
        render(R"(
            {%- for x in [1, 1.2, "a", true, True, false, False, None, [], [1], [1, 2], {}, {"a": 1}, {1: "b"}] -%}
                {{- x | tojson -}},
            {%- endfor -%}
        )", {}, {}));
    EXPECT_EQ(
        R"(1 "",2 "Hello")",
        render(R"(
            {%- set n = namespace(value=1, title='') -%}
            {{- n.value }} "{{ n.title }}",
            {%- set n.value = 2 -%}
            {%- set n.title = 'Hello' -%}
            {{- n.value }} "{{ n.title }}")", {}, {}));
    EXPECT_EQ(
        "[1, 2, 3]",
        render(
        "{% set _ = a.b.append(c.d.e) %}{{ a.b }}",
        json::parse(R"({
            "a": {"b": [1, 2]},
            "c": {"d": {"e": 3}}
        })"),
        {}
    ));
    EXPECT_EQ(
        "1,10;2,20;",
        render(R"(
        {%- for x, y in z -%}
            {{- x }},{{ y -}};
        {%- endfor -%}
    )", {{"z", json({json({1, 10}), json({2, 20})})}}, {}));
    EXPECT_EQ(
        " a bc ",
        render(" a {{  'b' -}} c ", {}, {}));
    EXPECT_EQ(
        " ab c ",
        render(" a {{- 'b'  }} c ", {}, {}));
    EXPECT_EQ(
        "ab\nc",
        render("a\n{{- 'b'  }}\nc", {}, {}));
    EXPECT_EQ(
        "a\nbc",
        render("a\n{{  'b' -}}\nc", {}, {}));
    EXPECT_EQ(
        "True",
        render("{{ [] is iterable }}", {}, {}));
    EXPECT_EQ(
        "True",
        render("{{ [] is not number }}", {}, {}));
    EXPECT_EQ(
        "[1, 2, 3][0, 1][1, 2]",
        render("{% set x = [0, 1, 2, 3] %}{{ x[1:] }}{{ x[:2] }}{{ x[1:3] }}", {}, {}));
    EXPECT_EQ(
        "a",
        render("{{ ' a  ' | trim }}", {}, {}));
    if (!getenv("USE_JINJA2")) {
        EXPECT_EQ(
            "",
            render(R"({{ None | trim }})", {}, {}));
    }
    EXPECT_EQ(
        "[0, 1, 2][4, 5, 6][0, 2, 4, 6, 8]",
        render("{{ range(3) | list }}{{ range(4, 7) | list }}{{ range(0, 10, 2) | list }}", {}, {}));
    EXPECT_EQ(
        " abc ",
        render(R"( {{ "a" -}} b {{- "c" }} )", {}, {}));
    EXPECT_EQ(
        "[\n  1\n]",
        render("{% set x = [] %}{% set _ = x.append(1) %}{{ x | tojson(indent=2) }}", {}, {}));
    EXPECT_EQ(
        "True",
        render("{{ not [] }}", {}, {}));
    EXPECT_EQ(
        "True",
        render("{{ tool.function.name == 'ipython' }}",
        json({{"tool", json({
            {"function", {{"name", "ipython"}}}
        })}}),
        {}
    ));
    EXPECT_EQ(
        "Hello Olivier",
        render(R"(
        {%- set user = "Olivier" -%}
        {%- set greeting = "Hello " ~ user -%}
        {{- greeting -}}
    )", {}, {}));

    EXPECT_EQ(
        "",
        render("{% if 1 %}{% elif 1 %}{% else %}{% endif %}", {}, {}));

    EXPECT_EQ(
        "0,1,2,",
        render("{% for i in range(10) %}{{ i }},{% if i == 2 %}{% break %}{% endif %}{% endfor %}", {}, {}));
    EXPECT_EQ(
        "0,2,4,6,8,",
        render("{% for i in range(10) %}{% if i % 2 %}{% continue %}{% endif %}{{ i }},{% endfor %}", {}, {}));

    if (!getenv("USE_JINJA2")) {
        // TODO: capture stderr from jinja2 and test these.

        EXPECT_THAT([]() { render("{% break %}", {}, {}); }, ThrowsWithSubstr("break outside of a loop"));
        EXPECT_THAT([]() { render("{% continue %}", {}, {}); }, ThrowsWithSubstr("continue outside of a loop"));

        EXPECT_THAT([]() { render("{%- set _ = [].pop() -%}", {}, {}); }, ThrowsWithSubstr("pop from empty list"));
        EXPECT_THAT([]() { render("{%- set _ = {}.pop() -%}", {}, {}); }, ThrowsWithSubstr("pop"));
        EXPECT_THAT([]() { render("{%- set _ = {}.pop('foooo') -%}", {}, {}); }, ThrowsWithSubstr("foooo"));

        EXPECT_THAT([]() { render("{% else %}", {}, {}); }, ThrowsWithSubstr("Unexpected else"));

        EXPECT_THAT([]() { render("{% else %}", {}, {}); }, ThrowsWithSubstr("Unexpected else"));
        EXPECT_THAT([]() { render("{% endif %}", {}, {}); }, ThrowsWithSubstr("Unexpected endif"));
        EXPECT_THAT([]() { render("{% elif 1 %}", {}, {}); }, ThrowsWithSubstr("Unexpected elif"));
        EXPECT_THAT([]() { render("{% endfor %}", {}, {}); }, ThrowsWithSubstr("Unexpected endfor"));
        EXPECT_THAT([]() { render("{% endfilter %}", {}, {}); }, ThrowsWithSubstr("Unexpected endfilter"));

        EXPECT_THAT([]() { render("{% if 1 %}", {}, {}); }, ThrowsWithSubstr("Unterminated if"));
        EXPECT_THAT([]() { render("{% for x in 1 %}", {}, {}); }, ThrowsWithSubstr("Unterminated for"));
        EXPECT_THAT([]() { render("{% generation %}", {}, {}); }, ThrowsWithSubstr("Unterminated generation"));
        EXPECT_THAT([]() { render("{% if 1 %}{% else %}", {}, {}); }, ThrowsWithSubstr("Unterminated if"));
        EXPECT_THAT([]() { render("{% if 1 %}{% else %}{% elif 1 %}{% endif %}", {}, {}); }, ThrowsWithSubstr("Unterminated if"));
        EXPECT_THAT([]() { render("{% filter trim %}", {}, {}); }, ThrowsWithSubstr("Unterminated filter"));
        EXPECT_THAT([]() { render("{# ", {}, {}); }, ThrowsWithSubstr("Missing end of comment tag"));
    }

    EXPECT_EQ(
        "3",
        render("{{ (a.b.c) }}", {{"a", json({{"b", {{"c", 3}}}})}}, {}));

    // expect_throws_with_message_substr([]() { render("{{ a.b }}", {}, {}); }, "'a' is not defined");
    // expect_throws_with_message_substr([]() { render("{{ raise_exception('hey') }}", {}, {}); }, "hey");
}
