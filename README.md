# minja.hpp - A minimalistic C++ Jinja templating engine for LLM chat templates

_**Used to be at https://github.com/google/minja, but I've left Google and I'll only maintain my fork from now on**_

Minja is a minimalistic reimplementation of the [Jinja](https://github.com/pallets/jinja/) templating engine to integrate in/with C++ LLM projects (it's used in [llama.cpp](https://github.com/ggerganov/llama.cpp/pull/11016), [Jan](https://jan.ai/) (through [cortex.cpp](https://github.com/menloresearch/cortex.cpp/pull/1814)), [GPT4All](https://github.com/nomic-ai/gpt4all/pull/3433) and [Docker Model Runner](https://github.com/docker/model-runner)).

It is **not general purpose**: it includes just what’s needed for actual chat templates (very limited set of filters, tests and language features). Users with different needs should look at third-party alternatives such as [Jinja2Cpp](https://github.com/jinja2cpp/Jinja2Cpp), [Jinja2CppLight](https://github.com/hughperkins/Jinja2CppLight), or [inja](https://github.com/pantor/inja) (none of which we endorse).

> [!WARNING]  
> TL;DR: use of Minja is *at your own risk*, and the risks are plenty! See [Security & Privacy](#security--privacy) section below.

> [!IMPORTANT]  
> [@ochafik](https://github.com/ochafik) has left Google, watch out for https://github.com/ochafik/minja

[![CI](https://github.com/google/minja/actions/workflows/build.yml/badge.svg)](https://github.com/google/minja/actions/workflows/build.yml)

## Design goals:

- Support each and every major LLM found on HuggingFace
  - See `MODEL_IDS` in [tests/CMakeLists.txt](./tests/CMakeLists.txt) for the list of models currently supported
- Easy to integrate to/with projects such as [llama.cpp](https://github.com/ggerganov/llama.cpp) or [gemma.cpp](https://github.com/google/gemma.cpp):
  - Header-only
  - C++17
  - Only depend on [nlohmann::json](https://github.com/nlohmann/json) (no Boost)
  - Keep codebase small (currently 2.5k LoC) and easy to understand
- *Decent* performance compared to Python.

## Non-goals:

- Address glaring Prompt injection risks in current Jinja chat templating practices. See [Security & Privacy](#security--privacy) below
- Additional features from Jinja that aren't used by the template(s) of any major LLM (no feature creep!)
  - Please don't submit PRs with such features, they will unfortunately be rejected.
- Full Jinja compliance (neither syntax-wise, nor filters / tests / globals)

## Usage:

This library is header-only: just copy the header(s) you need, make sure to use a compiler that handles C++17 and you're done. Oh, and get [nlohmann::json](https://github.com/nlohmann/json) in your include path.

If your project is based on [cmake](https://cmake.org/), can simply import by using `FetchContent`.
```
FetchContent_Declare(minja GIT_REPOSITORY "https://github.com/google/minja")
FetchContent_MakeAvailable(minja)

target_link_libraries(<YOUR_TARGET> PRIVATE minja)
```

See API in [minja/minja.hpp](./include/minja/minja.hpp) and [minja/chat-template.hpp](./include/minja/chat-template.hpp) (experimental).

For raw Jinja templating (see [examples/raw.cpp](./examples/raw.cpp)):

```c++
#include <minja.hpp>
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
```

To apply a template to a JSON array of `messages` and `tools` in the HuggingFace standard (see [examples/chat-template.cpp](./examples/chat-template.cpp)):

```c++
#include <chat-template.hpp>
#include <iostream>

using json = nlohmann::ordered_json;

int main() {
    minja::chat_template tmpl(
        "{% for message in messages %}"
        "{{ '<|' + message['role'] + '|>\\n' + message['content'] + '<|end|>' + '\\n' }}"
        "{% endfor %}",
        /* bos_token= */ "<|start|>",
        /* eos_token= */ "<|end|>"
    );
    std::cout << tmpl.apply(
        json::parse(R"([
            {"role": "user", "content": "Hello"},
            {"role": "assistant", "content": "Hi there"}
        ])"),
        json::parse(R"([
            {"type": "function", "function": {"name": "google_search", "arguments": {"query": "2+2"}}}
        ])"),
        /* add_generation_prompt= */ true,
        /* extra_context= */ {}) << std::endl;
}
```

(Note that some template quirks are worked around by [minja/chat-template.hpp](./include/minja/chat-template.hpp) so that all templates can be used the same way)

## Supported features

Models have increasingly complex templates (see [some examples](https://gist.github.com/ochafik/15881018fa0aeff5b7ddaa8ff14540b0)), so a fair bit of Jinja's language constructs is required to execute their templates properly.

Minja supports the following subset of the [Jinja2/3 template syntax](https://jinja.palletsprojects.com/en/3.1.x/templates/):

- Full expression syntax
- Statements `{{% … %}}`, variable sections `{{ … }}`, and comments `{# … #}` with pre/post space elision `{%- … -%}` / `{{- … -}}` / `{#- … -#}`
- `if` / `elif` / `else` / `endif`
- `for` (`recursive`) (`if`) / `else` / `endfor` w/ `loop.*` (including `loop.cycle`) and destructuring)
- `break`, `continue` (aka [loop controls extensions](https://github.com/google/minja/pull/39))
- `set` w/ namespaces & destructuring
- `macro` / `endmacro`
- `call` / `endcall` - for calling macro (w/ macro arguments and `caller()` syntax) and passing a macro to another macro (w/o passing arguments back to the call block)
- `filter` / `endfilter`
- Extensible filters collection: `count`, `dictsort`, `equalto`, `e` / `escape`, `items`, `join`, `joiner`, `namespace`, `raise_exception`, `range`, `reject` / `rejectattr` / `select` / `selectattr`, `tojson`, `trim`

Main limitations (non-exhaustive list):

- Not supporting [most filters](https://jinja.palletsprojects.com/en/3.0.x/templates/#builtin-filters). Only the ones actually used in templates of major (or trendy) models are/will be implemented.
- No difference between `none` and `undefined`
- Single namespace with all filters / tests / functions / macros / variables
- No tuples (templates seem to rely on lists only)
- No `if` expressions w/o `else` (but `if` statements are fine)
- No `{% raw %}`, `{% block … %}`, `{% include … %}`, `{% extends … %},

## Roadmap / TODOs

- [ ] Fix known line difference issues on Windows
- [ ] Document the various capabilities detectors + backfill strategies used
- [ ] Propose integration w/ https://github.com/google/gemma.cpp
- [x] Integrate to llama.cpp: https://github.com/ggerganov/llama.cpp/pull/11016 + https://github.com/ggerganov/llama.cpp/pull/9639
- Improve fuzzing coverage:
    - use thirdparty jinja grammar to guide exploration of inputs (or implement prettification of internal ASTs and use them to generate arbitrary values)
    - fuzz each filter / test
- Measure / track test coverage
- Setup performance tests
- Simplify two-pass parsing
    - Pass tokens to IfNode and such
- Macro nested set scope = global?
- Get listed in https://jbmoelker.github.io/jinja-compat-tests/, https://en.cppreference.com/w/cpp/links/libs

## Developer corner

### Design overview

- `minja::Parser` does two-phased parsing:
  - its `tokenize()` method creates coarse template "tokens" (plain text section, or expression blocks or opening / closing blocks). Tokens may have nested expressions ASTs, parsed with `parseExpression()`
  - its `parseTemplate()` method iterates on tokens to build the final `TemplateNode` AST.
- `minja::Value` represents a Python-like value
  - It relies on `nlohmann/json` for primitive values, but does its own JSON dump to be exactly compatible w/ the Jinja / Python implementation of `dict` string representation
- `minja::chat_template` wraps a template and provides an interface similar to HuggingFace's chat template formatting. It also normalizes the message history to accommodate different expectations from some templates (e.g. `message.tool_calls.function.arguments` is typically expected to be a JSON string representation of the tool call arguments, but some templates expect the arguments object instead)
- Testing involves a myriad of simple syntax tests and full e2e chat template rendering tests. For each model in `MODEL_IDS` (see [tests/CMakeLists.txt](./tests/CMakeLists.txt)), we fetch the `chat_template` field of the repo's `tokenizer_config.json`, use the official jinja2 Python library to render them on each of the (relevant) test contexts (in [tests/contexts](./tests/contexts)) into a golden file, and run a C++ test that renders w/ Minja and checks we get exactly the same output.

### Adding new Templates / Building

- Install Prerequisites:

    - cmake
    - GCC / clang
    - python 3.8+ (for tests)
    - flake8
    - editorconfig-checker

- Optional: test additional templates:

    - Add their HuggingFace model identifier to `MODEL_IDS` in [tests/CMakeLists.txt](./tests/CMakeLists.txt) (e.g. `meta-llama/Llama-3.2-3B-Instruct`)
    - For [gated models](https://huggingface.co/docs/transformers.js/en/guides/private) you have access to, first authenticate w/ HuggingFace:

        ```bash
        pip install huggingface_hub
        huggingface-cli login
        ```

- Build & run tests (shorthand: `./scripts/tests.sh`):

    ```bash
    rm -fR build && \
        cmake -B build && \
        cmake --build build -j && \
        ctest --test-dir build -j --output-on-failure
    ```

- Bonus: install `clang-tidy` before building (on MacOS: `brew install llvm ; sudo ln -s "$(brew --prefix llvm)/bin/clang-tidy" "/usr/local/bin/clang-tidy"`)

- Fuzzing tests

    - Note: `fuzztest` **[doesn't work](https://github.com/google/fuzztest/issues/179)** natively on Windows or MacOS.

        <details>
        <summary>Show instructions to run it inside a Docker container</summary>

        Beware of Docker Desktop's licensing: you might want to check out alternatives such as [colima](https://github.com/abiosoft/colima) (we'll still use the docker *client* in the example below).

        ```bash
        docker run --rm -it -v $PWD:/src:rw $( echo "
            FROM python:3.12-slim-bookworm
            COPY requirements.txt /tmp
            RUN apt update && \
                apt install -y cmake clang ccache git python3 python-is-python3 python3-pip && \
                apt-get clean && \
                rm -rf /var/lib/apt/lists/*
            RUN pip install setuptools pip --upgrade --force-reinstall
            RUN pip install -r /tmp/requirements.txt
            CMD /usr/bin/bash
            WORKDIR /src
        " | docker build . -f - -q )
        ```

        </details>

    - Build in [fuzzing mode](https://github.com/google/fuzztest/blob/main/doc/quickstart-cmake.md#fuzzing-mode) & run all fuzzing tests (optionally, set a higher `TIMEOUT` as env var):

        ```bash
        ./scripts/fuzzing_tests.sh
        ```

- Sanitizer tests:

    ```bash
    for sanitizer in ADDRESS THREAD UNDEFINED ; do
        docker run --rm \
            -v "$PWD":/src:ro \
            -v "$PWD/build-sanitizer-${sanitizer}":/src/build \
            -w /src \
            "$(echo "
                FROM ghcr.io/astral-sh/uv:debian-slim
                RUN apt-get update && apt-get install -y build-essential libcurl4-openssl-dev cmake clang-tidy
            " | docker build . -q -f - )" \
            bash -c "
                cmake -B build -DCMAKE_BUILD_TYPE=Debug -DMINJA_SANITIZER=${sanitizer} && \
                cmake --build build -j --config Debug && \
                ctest --test-dir build -j -C Debug --output-on-failure
            "
    done
    ```

- If your model's template doesn't run fine, please consider the following before [opening a bug](https://github.com/googlestaging/minja/issues/new):

    - Is the template using any unsupported filter / test / method / global function, and which one(s)?
    - Is the template publicly available? Non-gated models are more likely to become supported.
    - Which version of GCC / clang did you compile the tests with? On which OS version?
    - If you intend to contribute a fix:
        - Please read [CONTRIBUTING](./CONTRIBUTING.md) first. You'd have to sign a CLA, which your employer may need to accept.
        - Please test as many gated models as possible (use `cmake -B build -DMINJA_TEST_GATED_MODELS=1 ...` and edit MODEL_LIST appropriately)

- For bonus points, check the style of your edits with:

    ```bash
    flake8
    editorconfig-checker
    ```

## Security & Privacy

### Data protection

This library doesn't store any data by itself, it doesn't access files or the web, it only transforms a template (string) and context (JSON w/ fields `"messages"`, `"tools"`...) into a formatted string.

You should still be careful about untrusted third-party chat templates, as these could try and trigger bugs in Minja to exfiltrate user chat data (we only have limited fuzzing tests in place).

Risks are even higher with any user-defined functions.

### Do NOT produce HTML or JavaScript with this!

HTML processing with this library is UNSAFE: no escaping of is performed (and the `safe` filter is a passthrough), leaving users vulnerable to XSS. Minja is not intended to produce HTML.

### Beware of Prompt injection risks!

Prompt injection is NOT protected against by this library.

There are many types of prompt injection, some quite exotic (cf. [data exfiltration exploits leveraging markdown image previews](https://promptarmor.substack.com/p/data-exfiltration-from-writercom)).

For the simpler cases, it is perfectly possible for a user to craft a message that will look like a system prompt, like an assistant response or like the results of tool calls. While some models might be fine-tuned to ignore system calls not at the very start of the prompt or out of order messages / tool call results, it is expected that most models will be very confused & successfully manipulated by such prompt injections.

Note that injection of tool calls should typically not result in their execution as LLM inference engines should not try to parse the template output (just generated tokens), but this is something to watch out for when auditing such inference engines.

As there isn't any standard mechanism to escape special tokens to prevent those attacks, it is advised users of this library take their own message sanitization measures before applying chat templates. We do not recommend any specific such measure as each model reacts differently (some even understand l33tcode as instructions).
