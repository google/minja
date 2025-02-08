# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
'''
  Fetches the Jinja2 templates of specified models and generates prompt goldens for given chat contexts.
  Outputs lines of arguments for a C++ test binary.
  All files are written to the specified output folder.

  Usage:
    python scripts/fetch_templates_and_goldens.py output_folder context_file1.json context_file2.json ... model_id1 model_id2 ...

  Example:
    pip install -r requirements.txt
    python scripts/fetch_templates_and_goldens.py ./test_files tests/contexts/*.json CohereForAI/c4ai-command-r-plus mistralai/Mistral-Large-Instruct-2407 meetkai/functionary-medium-v3.1.jinja microsoft/Phi-3-medium-4k-instruct Qwen/Qwen2-7B-Instruct
'''

from dataclasses import dataclass
import logging
import datetime
import os
import sys
import asyncio
import aiofiles
from huggingface_hub import AsyncInferenceClient
from huggingface_hub.utils import build_hf_headers
import json
import jinja2
import jinja2.ext
import re
import argparse
import aiohttp
import shutil

logging.basicConfig(level=logging.INFO, format='%(message)s')
logger = logging.getLogger(__name__)


def raise_exception(message: str):
    raise ValueError(message)


def tojson(eval_ctx, value, indent=None):
    return json.dumps(value, indent=indent)

TEST_DATE = os.environ.get('TEST_DATE', '2024-07-26')


def strftime_now(format):
    now = datetime.datetime.strptime(TEST_DATE, "%Y-%m-%d")
    return now.strftime(format)


def join_cmake_path(parent, child):
    '''
        On Windows, CMake will interpret any backslashes as escapes so we return / for path separators
    '''
    return '/'.join(x.replace(r'\\', '/') for x in (parent, child))


def add_system(messages, system_prompt):
    if len(messages) > 0 and messages[0]["role"] == "system":
        existing_system = messages[0]["content"]
        messages[0] = {
            "role": "system",
            "content": existing_system + "\n\n" + system_prompt,
        }
    else:
        messages.insert(0, {
            "role": "system",
            "content": system_prompt,
        })

# data class
@dataclass
class TemplateCaps:
    supports_tools: bool = False
    supports_tool_calls: bool = False
    supports_tool_responses: bool = False
    supports_system_role: bool = False
    supports_parallel_tool_calls: bool = False
    supports_tool_call_id: bool = False
    requires_object_arguments: bool = False
    requires_non_null_content: bool = False
    requires_typed_content: bool = False

    def to_json(self):
        return json.dumps({
            "supports_tools": self.supports_tools,
            "supports_tool_calls": self.supports_tool_calls,
            "supports_tool_responses": self.supports_tool_responses,
            "supports_system_role": self.supports_system_role,
            "supports_parallel_tool_calls": self.supports_parallel_tool_calls,
            "supports_tool_call_id": self.supports_tool_call_id,
            "requires_object_arguments": self.requires_object_arguments,
            # "requires_non_null_content": self.requires_non_null_content,
            "requires_typed_content": self.requires_typed_content,
        }, indent=2)


class chat_template:

    def try_raw_render(self, messages, *, tools=[], add_generation_prompt=False, extra_context={}, expect_strings=[]):
        basic_extra_context = {
            "bos_token": "<|startoftext|>",
            "eos_token": "<|endoftext|>",
        }

        try:
            out = self.template.render(messages=messages, tools=tools, add_generation_prompt=add_generation_prompt, **basic_extra_context, **extra_context)
            # print(out, file=sys.stderr)
            return out
        except BaseException as e:
            # print(f"{template_file}: Error rendering template with messages {messages}: {e}", file=sys.stderr, flush=True)
            return ""

    def __init__(self, template, known_eos_tokens, env=None):
        if not env:
            env = jinja2.Environment(
                trim_blocks=True,
                lstrip_blocks=True,
                extensions=[jinja2.ext.loopcontrols]
            )
        self.env = env
        self.template = env.from_string(template)

        caps = TemplateCaps()

        user_needle = "<User Needle>"
        sys_needle = "<System Needle>"
        dummy_str_user_msg = {"role": "user", "content": user_needle }
        dummy_typed_user_msg = {"role": "user", "content": [{"type": "text", "text": user_needle}]}

        caps.requires_typed_content = \
            (user_needle not in self.try_raw_render([dummy_str_user_msg])) \
            and (user_needle in self.try_raw_render([dummy_typed_user_msg]))
        dummy_user_msg = dummy_typed_user_msg if caps.requires_typed_content else dummy_str_user_msg

        needle_system_msg = {"role": "system", "content": [{"type": "text", "text": sys_needle}] if caps.requires_typed_content else sys_needle}

        caps.supports_system_role = sys_needle in self.try_raw_render([needle_system_msg, dummy_user_msg])

        out = self.try_raw_render([dummy_user_msg], tools=[{
            "name": "some_tool",
            "type": "function",
            "function": {
                "name": "some_tool",
                "description": "Some tool",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "arg": {
                            "type": "string",
                            "description": "Some arg",
                        },
                    },
                    "required": ["arg"],
                },
            },
        }])
        caps.supports_tools = "some_tool" in out

        def make_tool_calls_msg(tool_calls, content=None):
            return {
                "role": "assistant",
                "content": content,
                "tool_calls": tool_calls,
            }
        def make_tool_call(tool_name, arguments):
            return {
                "id": "call_1___",
                "type": "function",
                "function": {
                    "arguments": arguments,
                    "name": tool_name,
                }
            }

        dummy_args_obj = {"argument_needle": "print('Hello, World!')"}

        out = self.try_raw_render([
            dummy_user_msg,
            make_tool_calls_msg([make_tool_call("ipython", json.dumps(dummy_args_obj))]),
        ])
        tool_call_renders_str_arguments = '"argument_needle":' in out or "'argument_needle':" in out
        out = self.try_raw_render([
            dummy_user_msg,
            make_tool_calls_msg([make_tool_call("ipython", dummy_args_obj)]),
        ])
        tool_call_renders_obj_arguments = '"argument_needle":' in out or "'argument_needle':" in out

        caps.supports_tool_calls = tool_call_renders_str_arguments or tool_call_renders_obj_arguments
        caps.requires_object_arguments = not tool_call_renders_str_arguments and tool_call_renders_obj_arguments

        caps.requires_non_null_content = \
            (user_needle in self.try_raw_render([dummy_user_msg, {"role": "assistant", "content": ''}])) \
            and (user_needle not in self.try_raw_render([dummy_user_msg, {"role": "assistant", "content": None}]))

        if caps.supports_tool_calls:
            dummy_args = dummy_args_obj if caps.requires_object_arguments else json.dumps(dummy_args_obj)
            tc1 = make_tool_call("test_tool1", dummy_args)
            tc2 = make_tool_call("test_tool2", dummy_args)
            out = self.try_raw_render([
                dummy_user_msg,
                make_tool_calls_msg([tc1, tc2]),
            ])
            caps.supports_parallel_tool_calls = "test_tool1" in out and "test_tool2" in out

            out = self.try_raw_render([
                dummy_user_msg,
                make_tool_calls_msg([tc1]),
                {
                    "role": "tool",
                    "name": "test_tool1",
                    "content": "Some response!",
                    "tool_call_id": "call_911_",
                }
            ])
            caps.supports_tool_responses = "Some response!" in out
            caps.supports_tool_call_id = "call_911_" in out

        self.tool_call_example = None
        try:
            if not caps.supports_tools:
                user_msg = {"role": "user", "content": "Hey"}
                args = {"arg1": "some_value"}
                tool_call_msg = {
                    "role": "assistant",
                    "content": None,
                    "tool_calls": [
                        {
                            "id": "call_1___",
                            "type": "function",
                            "function": {
                                "name": "tool_name",
                                "arguments": args if caps.requires_object_arguments else json.dumps(args),
                            },
                        },
                    ],
                }
                prefix = self.try_raw_render([user_msg], add_generation_prompt=True)
                full = self.try_raw_render([user_msg, tool_call_msg], add_generation_prompt=False)
                if not full.startswith(prefix):
                    for known_eos_token in known_eos_tokens:
                        prefix = prefix.rstrip()
                        if prefix.endswith(known_eos_token):
                            prefix = prefix[:-len(known_eos_token)]
                            break
                if not full.startswith(prefix):
                    print("Failed to infer a tool call example (possible template bug)", file=sys.stderr)
                self.tool_call_example = full[len(prefix):]
        except Exception as e:
            print(f"Failed to generate tool call example: {e}", file=sys.stderr)

        self.original_caps = caps

    def needs_polyfills(self, context):
        has_tools = context.get('tools') is not None
        caps = self.original_caps
        return not caps.supports_system_role \
            or (has_tools is not None and (False \
                or not caps.supports_tools \
                or not caps.supports_tool_responses \
                or not caps.supports_tool_calls \
                or caps.requires_object_arguments \
            )) \
            or caps.requires_typed_content

    def apply(self, context: dict):
        assert isinstance(context, dict)

        caps = self.original_caps
        has_tools = 'tools' in context

        if self.needs_polyfills(context):
            if has_tools and not caps.supports_tools:
                add_system(context['messages'],
                    f"You can call any of the following tools to satisfy the user's requests: {json.dumps(context['tools'], indent=2)}" +
                    ("\n\nExample tool call syntax:\n\n" + self.tool_call_example + "\n\n" if self.tool_call_example is not None else ""))

            for message in context['messages']:
                if 'tool_calls' in message:
                    for tool_call in message['tool_calls']:
                        if caps.requires_object_arguments:
                            if tool_call.get('type') == 'function':
                                arguments = tool_call['function']['arguments']
                                try:
                                    arguments = json.loads(arguments)
                                except:
                                    pass
                                tool_call['function']['arguments'] = arguments
                    if not caps.supports_tool_calls:
                        message['content'] = json.dumps({
                            "tool_calls": [
                                {
                                    "name": tc['function']['name'],
                                    "arguments": json.loads(tc['function']['arguments']),
                                    "id": tc.get('id'),
                                }
                                for tc in message['tool_calls']
                            ],
                            "content": None if message.get('content', '') == '' else message['content'],
                        }, indent=2)
                        del message['tool_calls']
                if message.get('role') == 'tool' and not caps.supports_tool_responses:
                    message['role'] = 'user'
                    message['content'] = json.dumps({
                        "tool_response": {
                            "tool": message['name'],
                            "content": message['content'],
                            "tool_call_id": message.get('tool_call_id'),
                        }
                    }, indent=2)
                    del message['name']

            if caps.requires_typed_content:
                for message in context['messages']:
                    if 'content' in message and isinstance(message['content'], str):
                        message['content'] = [{"type": "text", "text": message['content']}]

        try:
            return self.template.render(**context)
        except Exception as e1:
            for message in context['messages']:
                if message.get("content") is None:
                    message["content"] = ""

            try:
                return self.template.render(**context)
            except Exception as e2:
                logger.info(f"  ERROR: {e2} (after first error: {e1})")
                return f"ERROR: {e2}"

@dataclass
class Context:
    name: str
    file: str
    bindings: dict


async def handle_chat_template(output_folder, model_id, variant, template_src, contexts: list[Context]):
    if '{% generation %}' in template_src:
        print('Removing {% generation %} blocks from template', file=sys.stderr)
        template_src = template_src.replace('{% generation %}', '').replace('{% endgeneration %}', '')

    model_name = model_id.replace("/", "-")
    base_name = f'{model_name}-{variant}' if variant else model_name
    template_file = join_cmake_path(output_folder, f'{base_name}.jinja')

    caps_file = join_cmake_path(output_folder, f'{base_name}.caps.json')

    async with aiofiles.open(template_file, 'w') as f:
        await f.write(template_src)

    known_eos_tokens = [
        "<|END_OF_TURN_TOKEN|>",
        "<end_of_turn>",
        "</s>",
        "<|im_end|>",
        "<|eom_id|>",
        "<|eot_id|>",
        "<｜end▁of▁sentence｜>",
    ]

    template = chat_template(template_src, known_eos_tokens)
    template.env.filters['safe'] = lambda x: x
    template.env.filters['tojson'] = tojson
    template.env.globals['raise_exception'] = raise_exception
    template.env.globals['strftime_now'] = strftime_now
    caps = template.original_caps

    if not contexts:
        print(f"{template_file} {caps_file} n/a {template_file}")
        return

    async with aiofiles.open(caps_file, 'w') as f:
        await f.write(caps.to_json())

    assert isinstance(contexts, list)
    for context in contexts:
        assert isinstance(context, Context)
        assert isinstance(context.bindings, dict)
        if not caps.supports_tool_calls and context.bindings.get('tools') is not None:
            print(f'Skipping {context.name} test as tools seem unsupported by template {template_file}', file=sys.stderr)
            continue

        needs_tools_in_system = len(context.bindings.get('tools', [])) > 0 and not caps.supports_tools
        if not caps.supports_system_role and (any(m['role'] == 'system' for m in context.bindings['messages']) or needs_tools_in_system):
            continue

        output_file = join_cmake_path(output_folder, f'{base_name}-{context.name}.txt')

        output = template.apply(context.bindings)
        async with aiofiles.open(output_file, 'w') as f:
            await f.write(output)

        print(f"{template_file} {caps_file} {context.file} {output_file}")

async def async_hf_download(repo_id: str, filename: str) -> str:
    headers = build_hf_headers()
    url = f"https://huggingface.co/{repo_id}/raw/main/{filename}"
    async with aiohttp.ClientSession() as session:
        async with session.get(url, headers=headers) as response:
            response.raise_for_status()
            return await response.text()

async def process_model(output_folder: str, model_id: str, contexts: list[Context]):
    try:
        print(f"Processing model {model_id}...", file=sys.stderr)
        config_str = await async_hf_download(model_id, "tokenizer_config.json")

        try:
            config = json.loads(config_str)
        except json.JSONDecodeError:
            config = json.loads(re.sub(r'\}([\n\s]*\}[\n\s]*\],[\n\s]*"clean_up_tokenization_spaces")', r'\1', config_str))

        assert 'chat_template' in config, 'No "chat_template" entry in tokenizer_config.json!'
        chat_template = config['chat_template']
        if isinstance(chat_template, str):
            await handle_chat_template(output_folder, model_id, None, chat_template, contexts)
        else:
            await asyncio.gather(*[
                handle_chat_template(output_folder, model_id, ct['name'], ct['template'], contexts)
                for ct in chat_template
            ])
    except Exception as e:
        logger.error(f"Error processing model {model_id}: {e}")
        # import traceback
        # traceback.print_exc()
        await handle_chat_template(output_folder, model_id, None, str(e), [])

async def async_copy_file(src: str, dst: str):
    async with aiofiles.open(src, 'rb') as fsrc:
        async with aiofiles.open(dst, 'wb') as fdst:
            await fdst.write(await fsrc.read())

async def main():
    parser = argparse.ArgumentParser(description="Generate chat templates and output test arguments.")
    parser.add_argument("output_folder", help="Folder to store all output files")
    parser.add_argument("json_context_files_or_model_ids", nargs="+", help="List of context JSON files or HuggingFace model IDs")
    args = parser.parse_args()

    contexts: list[Context] = []
    model_ids = []
    for file in args.json_context_files_or_model_ids:
        if file.endswith('.json'):
            async with aiofiles.open(file, 'r') as f:
                contexts.append(Context(
                    name=os.path.basename(file).replace(".json", ""),
                    file=file,
                    bindings=json.loads(await f.read())))
        else:
            model_ids.append(file)

    output_folder = args.output_folder
    if not os.path.isdir(output_folder):
        os.makedirs(output_folder)

    # for model_id in model_ids:
    #     await process_model(output_folder, model_id, contexts)
    await asyncio.gather(*[
        process_model(output_folder, model_id, contexts)
        for model_id in model_ids
    ])

if __name__ == '__main__':
    asyncio.run(main())
