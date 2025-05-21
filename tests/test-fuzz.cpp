/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include <fuzztest/fuzztest.h>
#include <fuzztest/grammars/json_grammar.h>
#include <gtest/gtest.h>
#include <minja/minja.hpp>
#include <minja/chat-template.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

// using json = nlohmann::ordered_json; // Replaced
using Document = rapidjson::Document; // Keep RValue for rapidjson::Value if minja::Value is distinct
using RValue = rapidjson::Value;


using namespace fuzztest;
using namespace minja;


// https://github.com/google/fuzztest/blob/main/doc/domains-reference.md

static auto AnyText() {
    return Arbitrary<std::string>().WithMaxSize(1000);
}

static auto AnyLocation() {
    return StructOf<Location>();
}

static auto AnyJsonObject() {
    // return Filter([](const std::string & s) {
    //     return json::parse(s).is_object();
    // }, InJsonGrammar());
    return internal::grammar::InGrammarImpl<internal::grammar::json::ObjectNode>();
}

static auto AnyTemplateNode() {
    return SharedPtrOf(
        ConstructorOf<TextNode>(
            AnyLocation(),
            AnyText()
        )
    );
}

static Domain<std::shared_ptr<Expression>> AnyExpression() {
    // Assumes minja::Value has constructors for these primitive types and
    // that LiteralExpr takes a minja::Value.
    // The minja::Value constructors for basic types (int, double, bool, string)
    // should internally use rvalue_.SetInt64(), rvalue_.SetDouble(), etc.
    // For objects/arrays, this is more complex.
    // minja::Value for an empty object could be Value(rapidjson::kObjectType) if such a ctor exists,
    // or more likely, Value::object() static method.
    // The overwritten minja.hpp should handle these.
    // The nlohmann::json bridge constructor in the overwritten minja::Value will be used here.
    return ElementOf({
        std::shared_ptr<Expression>(nullptr),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json()))), // null
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json(1)))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json(1.0)))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json(std::numeric_limits<double>::infinity())))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json(std::numeric_limits<double>::quiet_NaN())))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json(std::numeric_limits<double>::signaling_NaN())))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json(true)))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json("")))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json("x")))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json::object()))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json::object({{"x", 1}})))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json::array()))),
        std::shared_ptr<Expression>(new LiteralExpr({}, minja::Value(nlohmann::json::array({1, 2})))),
        std::shared_ptr<Expression>(new VariableExpr({}, "")),
        std::shared_ptr<Expression>(new VariableExpr({}, "x")),
    });
}

// static auto AnyArguments() { ... } // Remains commented out
// static auto AnyIdentifier() { ... } // Remains commented out

// parse_and_render now needs to handle rapidjson for bindings.
// The Context::make function is assumed to be updated to take a minja::Value
// which is internally rapidjson-based.
static std::string parse_and_render(const std::string & template_str, const minja::Value & bindings_val, const Options & options) {
    auto root = Parser::parse(template_str, options);
    // Context::make expects a minja::Value. If bindings_val is already a minja::Value,
    // it can be moved or copied. The existing minja.hpp uses move.
    auto context = Context::make(minja::Value(bindings_val)); // Ensure copy or proper move
    return root->render(context);
}

static void TestNodeRenderDoesNotCrash(const std::shared_ptr<TemplateNode> & root, const std::string & json_bindings_str) {
    if (!root) return;
    // Parse json_bindings_str into a rapidjson::Document, then to minja::Value
    Document doc;
    if (doc.Parse(json_bindings_str.c_str()).HasParseError()) {
        // Handle or log parse error, though fuzz tests often proceed
        return;
    }
    minja::Value bindings_value; // This should ideally construct from 'doc'
                                 // Using the nlohmann bridge for now as direct RValue->minja::Value is complex
    nlohmann::json temp_nl_json = nlohmann::json::parse(json_bindings_str, nullptr, false); // allow no-throw parse
    if (temp_nl_json.is_discarded()) {
        return; // Invalid JSON, skip
    }
    bindings_value = Value(temp_nl_json);


    auto context = Context::make(std::move(bindings_value));
    try {
        root->render(context);
    } catch (const std::exception& ) {
        // Do nothing
    }
}
static void TestExprEvalDoesNotCrash(const std::shared_ptr<Expression> & expr, const std::string & json_bindings_str) {
    if (!expr) return;
    // Parse json_bindings_str into a rapidjson::Document, then to minja::Value
    Document doc;
    if (doc.Parse(json_bindings_str.c_str()).HasParseError()) {
        return; // Or log
    }
    minja::Value bindings_value; // Bridge via nlohmann for now
    nlohmann::json temp_nl_json = nlohmann::json::parse(json_bindings_str, nullptr, false);
    if (temp_nl_json.is_discarded()) {
        return;
    }
    bindings_value = Value(temp_nl_json);

    auto context = Context::make(std::move(bindings_value));
    try {
        expr->evaluate(context);
    } catch (const std::exception& ) {
        // Do nothing
    }
}

// dump function now takes a minja::Value, assuming it's what we want to test for tojson filter.
static std::string dump_minja_value_to_json_string(const minja::Value & val) {
  return val.dump(-1, /* to_json= */ true);
}

void TestParseAndRenderDoesNotCrash(const std::string& template_str, const std::string& json_bindings_str) {
    try {
        Document doc;
        if (doc.Parse(json_bindings_str.c_str()).HasParseError()) {
            return; // Invalid JSON input from fuzzer
        }
        minja::Value bindings_value; // Bridge via nlohmann
        nlohmann::json temp_nl_json = nlohmann::json::parse(json_bindings_str, nullptr, false);
        if (temp_nl_json.is_discarded()) {
            return;
        }
        bindings_value = Value(temp_nl_json);
        
        auto unused = parse_and_render(template_str, bindings_value, {});
    } catch (const std::exception& e) {
        // std::cerr << "Exception caught in TestParseAndRenderDoesNotCrash: " << e.what() << std::endl;
    }
}

void TestParseAndRenderJsonDoesNotCrash(const std::string & json_input_str) {
    // This test checks if "{{ x | tojson }}" correctly serializes a JSON structure.
    // The input 'x' is a JSON string. We parse it, put it in context, render, and compare.
    Document doc_x;
    if (doc_x.Parse(json_input_str.c_str()).HasParseError()) {
        return; // Invalid JSON from fuzzer
    }
    
    // Create minja::Value for 'x' using the nlohmann bridge from the parsed rapidjson string
    // This is convoluted: json_input_str -> rapidjson::Document -> (string via dump) -> nlohmann::json -> minja::Value
    // This is necessary because minja::Value(RValue) is not fully implemented/safe for complex types.
    rapidjson::StringBuffer buffer_x_str;
    rapidjson::Writer<rapidjson::StringBuffer> writer_x_str(buffer_x_str);
    doc_x.Accept(writer_x_str);
    nlohmann::json nl_x = nlohmann::json::parse(buffer_x_str.GetString());
    minja::Value minja_x_val(nl_x);

    // The expected output is the JSON string representation of minja_x_val.
    // minja::Value::dump(to_json=true) should produce this.
    std::string expected_dump = minja_x_val.dump(-1, true);

    // Create context: { "x": minja_x_val }
    // Again, using nlohmann bridge for context creation for simplicity here.
    nlohmann::json context_bindings_nl;
    context_bindings_nl["x"] = nl_x; // nl_x used here as it's what minja_x_val was created from
    minja::Value context_bindings_minja_val(context_bindings_nl);

    std::string rendered_output = parse_and_render("{{ x | tojson }}", context_bindings_minja_val, {});
    
    // The rendered output should be equivalent to dumping the original parsed rapidjson document (doc_x) as a string,
    // or more directly, the minja_x_val dumped as JSON.
    EXPECT_EQ(expected_dump, rendered_output);
}

void TestChatTemplate(const std::string& template_str, const std::string& messages_json_str, const std::string& tools_json_str) {
    try {
        chat_template tmpl(template_str, "<|start|>", "<|end|>");
        
        rapidjson::Document input_owner_doc; // Owns all data for inputs
        minja::chat_template_inputs inputs;
        inputs.allocator_for_inputs = &input_owner_doc.GetAllocator();

        rapidjson::Document messages_doc;
        if (!messages_doc.Parse(messages_json_str.c_str()).HasParseError()) {
            inputs.messages.CopyFrom(messages_doc, *inputs.allocator_for_inputs);
        } else {
            inputs.messages.SetArray(); // Default to empty array on parse error
        }

        rapidjson::Document tools_doc;
        if (!tools_json_str.empty() && !tools_doc.Parse(tools_json_str.c_str()).HasParseError()) {
            inputs.tools.CopyFrom(tools_doc, *inputs.allocator_for_inputs);
        } else {
            inputs.tools.SetNull(); // Default to null or empty array as appropriate
        }
        // extra_context defaults to kNullType in chat_template_inputs

        auto unused = tmpl.apply(inputs); // Apply with default options
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

FUZZ_TEST(FuzzTextNode, TestNodeRenderDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<TextNode>(AnyLocation(), AnyText())),
        AnyJsonObject());
FUZZ_TEST(FuzzExpressionNode, TestNodeRenderDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<ExpressionNode>(AnyLocation(), AnyExpression())),
        AnyJsonObject());
// FUZZ_TEST(FuzzSequenceNode, TestNodeRenderDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<SequenceNode>(AnyLocation(), VectorOf(AnyTemplateNode()))),
//         AnyJsonObject());
// FUZZ_TEST(FuzzIfNode, TestNodeRenderDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<IfNode>(
//             AnyLocation(),
//             VectorOf(PairOf(AnyExpression(), AnyTemplateNode())))),
//         AnyJsonObject());
FUZZ_TEST(FuzzForNode, TestNodeRenderDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<ForNode>(
            AnyLocation(),
            VectorOf(AnyText()),
            AnyExpression(),
            AnyExpression(),
            AnyTemplateNode(),
            Arbitrary<bool>(),
            AnyTemplateNode())),
        AnyJsonObject());
// FUZZ_TEST(FuzzMacroNode, TestNodeRenderDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<MacroNode>(
//             AnyLocation(),
//             SharedPtrOf(ConstructorOf<VariableExpr>(AnyLocation(), AnyText())),
//             VectorOf(PairOf(AnyText(), AnyExpression())),
//             AnyTemplateNode())),
//         AnyJsonObject());
FUZZ_TEST(FuzzSetNode, TestNodeRenderDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<SetNode>(
            AnyLocation(),
            AnyText(),
            VectorOf(AnyText()),
            AnyExpression())),
        AnyJsonObject());
FUZZ_TEST(FuzzSetTemplateNode, TestNodeRenderDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<SetTemplateNode>(
            AnyLocation(),
            AnyText(),
            AnyTemplateNode())),
        AnyJsonObject());

FUZZ_TEST(FuzzIfExpr, TestExprEvalDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<IfExpr>(AnyLocation(), AnyExpression(), AnyExpression(), AnyExpression())),
        AnyJsonObject());
FUZZ_TEST(FuzzLiteralExpr, TestExprEvalDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<LiteralExpr>(AnyLocation(), AnyText())),
        AnyJsonObject());
// FUZZ_TEST(FuzzArrayExpr, TestExprEvalDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<ArrayExpr>(AnyLocation(), VectorOf<Expression>(AnyExpression()))),
//         AnyJsonObject());
// FUZZ_TEST(FuzzDictExpr, TestExprEvalDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<DictExpr>(
//             AnyLocation(),
//             VectorOf(PairOf(AnyExpression(), AnyExpression())))),
//         AnyJsonObject());
FUZZ_TEST(FuzzSliceExpr, TestExprEvalDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<SliceExpr>(AnyLocation(), AnyExpression(), AnyExpression())),
        AnyJsonObject());
FUZZ_TEST(FuzzVariableExpr, TestExprEvalDoesNotCrash)
    .WithDomains(
        SharedPtrOf(ConstructorOf<VariableExpr>(AnyLocation(), AnyText())),
        AnyJsonObject());
// FUZZ_TEST(FuzzUnaryOpExpr, TestExprEvalDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<UnaryOpExpr>(
//             AnyLocation(),
//             AnyExpression(),
//             Arbitrary<UnaryOpExpr::Op>())),
//         AnyJsonObject());
// FUZZ_TEST(FuzzBinaryOpExpr, TestExprEvalDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<BinaryOpExpr>(
//             AnyLocation(),
//             AnyExpression(),
//             AnyExpression(),
//             Arbitrary<BinaryOpExpr::Op>())),
//         AnyJsonObject());
// FUZZ_TEST(FuzzMethodCallExpr, TestExprEvalDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<MethodCallExpr>(
//             AnyLocation(),
//             AnyExpression(),
//             SharedPtrOf(ConstructorOf<VariableExpr>(AnyLocation(), AnyText())),
//             Expression::Arguments())),
//         AnyJsonObject());
// FUZZ_TEST(FuzzCallExpr, TestExprEvalDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<CallExpr>(
//             AnyLocation(),
//             AnyExpression(),
//             Expression::Arguments())),
//         AnyJsonObject());
// FUZZ_TEST(FuzzFilterExpr, TestExprEvalDoesNotCrash)
//     .WithDomains(
//         SharedPtrOf(ConstructorOf<FilterExpr>(
//             AnyLocation(),
//             VectorOf(AnyExpression()))),
//         AnyJsonObject());

FUZZ_TEST(Fuzz, TestParseAndRenderDoesNotCrash)
    // .WithSeeds({
    //     {"{% for x in range(10) | odd %}{% if x % 3 == 0 %}{{ x * 100 }}{% endif %}{% endfor %}", {"x", nullptr}},
    //     {"{{ x.y[z]() - 1 }}", {}},
    //     {"{% if 1 %}{# booh #}{% endif %}", {}},
    //     {"{{ }}", {}},
    //     {"{% %}", {}},
    // })
    .WithDomains(
        AnyText(),
        AnyJsonObject()
    );
FUZZ_TEST(Fuzz, TestParseAndRenderJsonDoesNotCrash)
    // .WithSeeds({
    //     {"null"},
    //     {"[]"},
    //     {"[null]"},
    //     {"[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]"},
    //     {"{\"a\": [null]}"},
    // })
    .WithDomains(AnyJsonObject());

FUZZ_TEST(Fuzz, TestChatTemplate)
    .WithDomains(
        AnyText(),
        AnyJsonObject(),
        AnyJsonObject()
    );
