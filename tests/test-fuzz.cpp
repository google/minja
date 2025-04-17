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

using json = nlohmann::ordered_json;
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
    return ElementOf({
        std::shared_ptr<Expression>(nullptr),
        std::shared_ptr<Expression>(new LiteralExpr({}, json())),
        std::shared_ptr<Expression>(new LiteralExpr({}, json(1))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json(1.0))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json(std::numeric_limits<double>::infinity()))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json(std::numeric_limits<double>::quiet_NaN()))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json(std::numeric_limits<double>::signaling_NaN()))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json(true))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json(""))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json("x"))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json::object())),
        std::shared_ptr<Expression>(new LiteralExpr({}, json::object({{"x", 1}}))),
        std::shared_ptr<Expression>(new LiteralExpr({}, json::array())),
        std::shared_ptr<Expression>(new LiteralExpr({}, json::array({1, 2}))),
        std::shared_ptr<Expression>(new VariableExpr({}, "")),
        std::shared_ptr<Expression>(new VariableExpr({}, "x")),
    });
    // return SharedPtrOf(
    //         ConstructorOf<VariableExpr>(
    //             AnyLocation(),
    //             Arbitrary<std::string>().WithMaxSize(1000)
    //         ));
}

// static auto AnyArguments() {
//     return FlatMap([](
//         const std::vector<std::shared_ptr<Expression>> & args,
//         const std::vector<std::pair<std::string, std::shared_ptr<Expression>>> & kwargs
//     ) -> Expression::Arguments {
//         return {
//             args,
//             kwargs
//         };
//     }, VectorOf(AnyExpression()), VectorOf(PairOf(AnyText(), AnyExpression())));
// }

// static auto AnyIdentifier() {
//     return InRegexp("[^\\s\\n]+");
// }

static std::string parse_and_render(const std::string & template_str, const json & bindings, const Options & options) {
    auto root = Parser::parse(template_str, options);
    auto context = Context::make(bindings);
    return root->render(context);
}

static void TestNodeRenderDoesNotCrash(const std::shared_ptr<TemplateNode> & root, const std::string & bindings) {
    if (!root) return;
    auto context = Context::make(json::parse(bindings));
    try {
        root->render(context);
    } catch (const std::exception& e) {
        // Do nothing
    }
}
static void TestExprEvalDoesNotCrash(const std::shared_ptr<Expression> & expr, const std::string & bindings) {
    if (!expr) return;
    auto context = Context::make(json::parse(bindings));
    try {
        expr->evaluate(context);
    } catch (const std::exception& e) {
        // Do nothing
    }
}

// Dumps `{"a": 1}` as `"{\"a\": 1}"`, unlike nlohmann::json::dump which would dump it as `"{\"a\":1}"`.
static std::string dump(const json & j) {
  return Value(j).dump(-1, /* to_json= */ true);
}

void TestParseAndRenderDoesNotCrash(const std::string& template_str, const std::string& json_str) {
    try {
        auto unused = parse_and_render(template_str, json::parse(json_str), {});
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

void TestParseAndRenderJsonDoesNotCrash(const std::string & x)   {
    EXPECT_EQ(dump(json::parse(x)), parse_and_render("{{ x | tojson }}", {{"x", json::parse(x)}}, {}));
}

void TestChatTemplate(const std::string& template_str, const std::string& messages_json, const std::string& tools_json) {
    try {
        chat_template tmpl(template_str, "<|start|>", "<|end|>");
        auto messages = json::parse(messages_json);
        auto tools = json::parse(tools_json);
        auto unused = tmpl.apply(messages, tools, true, {});
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
