/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include <minja/minja.hpp>
#include <iostream>

// rapidjson includes
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

// using json = nlohmann::ordered_json; // Replaced
// No top-level using Document = rapidjson::Document; needed if interaction is via minja types

int main() {
    auto tmpl = minja::Parser::parse("Hello, {{ location }}!", /* options= */ {});

    // Create data for the context using rapidjson
    // The minja::Value constructor that takes nlohmann::json is temporary.
    // Ideally, minja::Value would be constructed directly with rapidjson values or
    // have a more direct way to build its internal structure.
    // For this example, we'll bridge via the nlohmann-accepting constructor,
    // assuming it's been updated internally as per the minja.hpp refactoring.
    // This implies minja::Value(nlohmann::json) converts to its new rapidjson backend.

    // If minja::Value is to be constructed directly with rapidjson:
    // 1. Create a rapidjson::Document to own the memory
    // 2. Create the object within that document
    // 3. Pass the rapidjson::Value (referencing the object in the doc) to a
    //    minja::Value constructor designed for this (e.g., Value(const rapidjson::Value&, rapidjson::Document::AllocatorType*))
    //    or, if minja::Value itself manages an owned_document_ for such cases.

    // Given the current state of minja.hpp (with the nlohmann bridge constructor):
    nlohmann::json context_data_nl = {
        {"location", "World"}
    };
    minja::Value context_value(context_data_nl); // This uses the bridge constructor

    // If the bridge was removed, it would look something like this:
    // rapidjson::Document context_doc_owner;
    // rapidjson::Value context_rvalue(rapidjson::kObjectType);
    // rapidjson::Value location_key("location", context_doc_owner.GetAllocator());
    // rapidjson::Value location_val("World", context_doc_owner.GetAllocator());
    // context_rvalue.AddMember(location_key, location_val, context_doc_owner.GetAllocator());
    // minja::Value context_value; // Needs a way to be initialized with context_rvalue
                                // and potentially take ownership or reference context_doc_owner.
                                // This part is complex and depends on the final design of minja::Value's rapidjson integration.
                                // The current minja.hpp overwrite created an owned_document_ in string/primitive constructors
                                // and in the nlohmann::json constructor.
                                // A minja::Value representing an object directly built with rapidjson would need careful handling.

    auto context = minja::Context::make(std::move(context_value));
    auto result = tmpl->render(context);
    std::cout << result << std::endl;
}
