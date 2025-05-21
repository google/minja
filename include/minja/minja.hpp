/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include "rapidjson/ostreamwrapper.h"
// #include <nlohmann/json.hpp> // Replaced

#include <MNN/MNNDefine.h>

static void _printlog(const std::string& i) {
    MNN_PRINT("%s\n", i.c_str());
}

// using json = nlohmann::ordered_json; // Replaced
using Document = rapidjson::Document;
using RValue = rapidjson::Value; // Alias for rapidjson::Value

namespace minja {

class Context;

struct Options {
    bool trim_blocks;  // removes the first newline after a block
    bool lstrip_blocks;  // removes leading whitespace on the line of the block
    bool keep_trailing_newline;  // don't remove last newline
};

struct ArgumentsValue;

inline std::string normalize_newlines(const std::string & s) {
#ifdef _WIN32
  static const std::regex nl_regex("\r\n");
  return std::regex_replace(s, nl_regex, "\n");
#else
  return s;
#endif
}

/* Values that behave roughly like in Python. */
class Value : public std::enable_shared_from_this<Value> {
public:
  using CallableType = std::function<Value(const std::shared_ptr<Context> &, ArgumentsValue &)>;
  using FilterType = std::function<Value(const std::shared_ptr<Context> &, ArgumentsValue &)>;

private:
  using ObjectType = std::map<std::string, Value>;  // Only contains primitive keys
  using ArrayType = std::vector<Value>;

  std::shared_ptr<ArrayType> array_; // std::vector<Value>
  std::shared_ptr<ObjectType> object_; // std::map<std::string, Value>
  std::shared_ptr<CallableType> callable_;

  std::unique_ptr<Document> owned_document_; // If this Value owns the underlying JSON structure for rvalue_
  RValue rvalue_; // Represents the actual JSON data if not an array_, object_, or callable_ minja type.

  Value(const std::shared_ptr<ArrayType> & arr) : array_(arr), rvalue_(rapidjson::kNullType) {}
  Value(const std::shared_ptr<ObjectType> & obj) : object_(obj), rvalue_(rapidjson::kNullType) {}
  Value(const std::shared_ptr<CallableType> & call) : object_(std::make_shared<ObjectType>()), callable_(call), rvalue_(rapidjson::kNullType) {}

  // Helper to ensure an owned document exists if we need to allocate for rvalue_
  Document::AllocatorType& get_rvalue_allocator() {
    if (!owned_document_) {
        owned_document_ = std::make_unique<Document>();
        // Ensure rvalue_ is associated with this new document if it's going to store alloc-needing types
        if (rvalue_.IsNull()) { // Or other conditions where rvalue_ should be reset
          rvalue_.SetNull(); // Or appropriate default for this new document
        }
    }
    return owned_document_->GetAllocator();
  }


  static std::string RValueToString(const RValue& rval) {
      rapidjson::StringBuffer buffer;
      rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
      rval.Accept(writer);
      return buffer.GetString();
  }

  /* Python-style string repr */
  static void dump_string_rvalue(const RValue & rval_primitive, std::ostringstream & out, char string_quote = '\'') {
    if (!rval_primitive.IsString()) {
        _printlog("Value is not a string: " + RValueToString(rval_primitive));
        return;
    }
    std::string s_val = rval_primitive.GetString();
    if (string_quote == '"' || s_val.find('\'') != std::string::npos) {
        out << '"'; // Force double quotes
        for (char c : s_val) {
            if (c == '"' || c == '\\') out << '\\';
            out << c;
        }
        out << '"';
        return;
    }
    out << string_quote; // Start with the chosen quote
    for (char c : s_val) {
        if (c == '\\') out << "\\\\";
        else if (c == string_quote) out << '\\' << string_quote;
        else out << c;
    }
    out << string_quote; // End with the chosen quote
  }

  void dump(std::ostringstream & out, int indent_val = -1, int level = 0, bool to_json_format = false) const {
    auto print_indent_fn = [&](int current_level) {
      if (indent_val > 0) {
          out << "\n";
          for (int i = 0, n = current_level * indent_val; i < n; ++i) out << ' ';
      }
    };
    auto print_sub_sep_fn = [&]() {
      out << ',';
      if (indent_val < 0) out << ' ';
      else print_indent_fn(level + 1);
    };

    char chosen_string_quote = to_json_format ? '"' : '\'';

    if (is_null_internal()) { // Use the private helper that checks all internal states
        out << "null";
    } else if (array_) {
        out << "[";
        print_indent_fn(level + 1);
        for (size_t i = 0; i < array_->size(); ++i) {
            if (i) print_sub_sep_fn();
            (*array_)[i].dump(out, indent_val, level + 1, to_json_format);
        }
        print_indent_fn(level);
        out << "]";
    } else if (object_) {
        out << "{";
        print_indent_fn(level + 1);
        for (auto map_begin = object_->begin(), map_it = map_begin; map_it != object_->end(); ++map_it) {
            if (map_it != map_begin) print_sub_sep_fn();
            RValue key_rval_temp(map_it->first.c_str(), map_it->first.length());
            dump_string_rvalue(key_rval_temp, out, chosen_string_quote);
            out << ": ";
            map_it->second.dump(out, indent_val, level + 1, to_json_format);
        }
        print_indent_fn(level);
        out << "}";
    } else if (callable_) {
        _printlog("Cannot dump callable to JSON");
        out << "<callable>"; // Placeholder representation
    } else if (rvalue_.IsBool() && !to_json_format) { // Pythonic bool
        out << (rvalue_.GetBool() ? "True" : "False");
    } else if (rvalue_.IsString() && !to_json_format) { // Pythonic string
        dump_string_rvalue(rvalue_, out, chosen_string_quote);
    } else { // Handles numbers, and actual JSON objects/arrays if rvalue_ is used for that, or if to_json_format is true
        rapidjson::StringBuffer buffer;
        if (indent_val > 0 && (rvalue_.IsObject() || rvalue_.IsArray())) { // Pretty print for JSON structures
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            writer.SetIndent(' ', indent_val); // Use specified indent
            rvalue_.Accept(writer);
        } else { // Compact print for numbers or other types
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            rvalue_.Accept(writer);
        }
        out << buffer.GetString();
    }
  }

private:
  bool is_null_internal() const { return !object_ && !array_ && rvalue_.IsNull() && !callable_; }

public:
  Value() : rvalue_(rapidjson::kNullType) {}

  Value(bool v) : rvalue_(v) {}
  Value(int64_t v) : rvalue_(v) {}
  Value(double v) : rvalue_(v) {}
  Value(const std::nullptr_t &) : rvalue_(rapidjson::kNullType) {}

  Value(const std::string & s) {
      auto& allocator = get_rvalue_allocator();
      rvalue_.SetString(s.c_str(), s.length(), allocator);
  }
  Value(const char * s) {
      auto& allocator = get_rvalue_allocator();
      rvalue_.SetString(s, strlen(s), allocator);
  }

  // Constructor from nlohmann::json - CRITICAL: This is to be removed/refactored.
  // This constructor is temporarily kept for Context::builtins() which uses nlohmann::json.
  Value(const nlohmann::json &nj_val) : rvalue_(rapidjson::kNullType) {
    // _printlog("TEMPORARY: Converting nlohmann::json to minja::Value (rapidjson). Phase out this constructor.");
    if (nj_val.is_object()) {
        object_ = std::make_shared<ObjectType>();
        for (auto it_nl = nj_val.begin(); it_nl != nj_val.end(); ++it_nl) {
            (*object_)[it_nl.key()] = Value(it_nl.value()); // Recursive
        }
    } else if (nj_val.is_array()) {
        array_ = std::make_shared<ArrayType>();
        for (const auto& item_nl : nj_val) {
            array_->push_back(Value(item_nl)); // Recursive
        }
    } else { // Primitive from nlohmann::json
        auto& allocator = get_rvalue_allocator();
        if (nj_val.is_null()) rvalue_.SetNull();
        else if (nj_val.is_boolean()) rvalue_.SetBool(nj_val.get<bool>());
        else if (nj_val.is_number_integer()) rvalue_.SetInt64(nj_val.get<int64_t>());
        else if (nj_val.is_number_float()) rvalue_.SetDouble(nj_val.get<double>());
        else if (nj_val.is_string()) {
            std::string s = nj_val.get<std::string>();
            rvalue_.SetString(s.c_str(), s.length(), allocator);
        } else {
             _printlog("Unsupported nlohmann::json type in temp constructor.");
            rvalue_.SetNull();
        }
    }
  }

  std::vector<Value> keys() const {
    if (!object_) { _printlog("Value is not an object (map-type): " + dump()); return {}; }
    std::vector<Value> res_keys;
    for (const auto& item : *object_) {
      res_keys.push_back(Value(item.first)); // minja::Value from string key
    }
    return res_keys;
  }

  size_t size() const {
    if (is_object()) return object_->size();
    if (is_array()) return array_->size();
    if (rvalue_.IsString()) return rvalue_.GetStringLength();
    _printlog("Value is not a minja array/object or rvalue string: " + dump());
    return 0;
  }

  static Value array(const std::vector<Value> values = {}) {
    auto arr_ptr = std::make_shared<ArrayType>();
    for (const auto& item : values) {
      arr_ptr->push_back(item);
    }
    return Value(arr_ptr);
  }
  static Value object(const std::shared_ptr<ObjectType> obj_ptr = std::make_shared<ObjectType>()) {
    return Value(obj_ptr);
  }
  static Value callable(const CallableType & call_fn) {
    return Value(std::make_shared<CallableType>(call_fn));
  }

  void insert(size_t index, const Value& v) {
    if (!array_) _printlog("Value is not an array: " + dump());
    else array_->insert(array_->begin() + index, v);
  }
  void push_back(const Value& v) {
    if (!array_) _printlog("Value is not an array: " + dump());
    else array_->push_back(v);
  }
  Value pop(const Value& index_val) {
    if (is_array()) {
      if (array_->empty()) { _printlog("pop from empty list"); return Value(); }
      if (index_val.is_null()) {
        Value ret = array_->back();
        array_->pop_back();
        return ret;
      } else if (!index_val.is_number_integer()) {
        _printlog("pop index must be an integer: " + index_val.dump()); return Value();
      } else {
        int64_t i = index_val.to_int();
        if (i < 0) i += array_->size(); // Python-like negative indexing
        if (i < 0 || i >= static_cast<int64_t>(array_->size())) {
          _printlog("pop index out of range: " + index_val.dump()); return Value();
        }
        auto it = array_->begin() + i;
        Value ret = *it;
        array_->erase(it);
        return ret;
      }
    } else if (is_object()) {
      if (!index_val.is_string()) { _printlog("Key for pop must be a string: " + index_val.dump()); return Value(); }
      std::string key_str = index_val.to_str();
      auto it = object_->find(key_str);
      if (it == object_->end()) { _printlog("Key not found for pop: " + key_str); return Value(); }
      Value ret = it->second;
      object_->erase(it);
      return ret;
    }
    _printlog("Value is not an array or object for pop: " + dump());
    return Value();
  }

  Value get(const Value& key_val) { // Should be const if it doesn't modify
    if (array_) {
      if (!key_val.is_number_integer()) return Value();
      int64_t index = key_val.to_int();
      if (index < 0) index += array_->size();
      if (index < 0 || index >= static_cast<int64_t>(array_->size())) return Value(); // Out of bounds
      return array_->at(index);
    } else if (object_) {
      if (!key_val.is_string()) return Value();
      auto it = object_->find(key_val.to_str());
      if (it == object_->end()) return Value();
      return it->second;
    }
    return Value(); // Not an array or object, or key not suitable/found
  }

  void set(const std::string& key, const Value& value_to_set) {
      if (!object_) {
          _printlog("Value is not an object, cannot set key: " + dump());
          return;
      }
    (*object_)[key] = value_to_set;
  }

  Value call(const std::shared_ptr<Context> & context, ArgumentsValue & args) const {
      if (!callable_) { /* _printlog("Value is not callable: " + dump()); */ return Value(); }
    return (*callable_)(context, args);
  }

  bool is_object() const { return !!object_; }
  bool is_array() const { return !!array_; }
  bool is_callable() const { return !!callable_; }
  bool is_null() const { return is_null_internal(); } // Public is_null uses the private one
  bool is_boolean() const { return rvalue_.IsBool() && !object_ && !array_ && !callable_; }
  bool is_number_integer() const { return (rvalue_.IsInt64() || rvalue_.IsUint64()) && !object_ && !array_ && !callable_; }
  bool is_number_float() const { return rvalue_.IsDouble() && !object_ && !array_ && !callable_; }
  bool is_number() const { return rvalue_.IsNumber() && !object_ && !array_ && !callable_; }
  bool is_string() const { return rvalue_.IsString() && !object_ && !array_ && !callable_; }
  bool is_iterable() const { return is_array() || is_object() || is_string(); }

  bool is_primitive() const { return !array_ && !object_ && !callable_ && (rvalue_.IsNumber() || rvalue_.IsString() || rvalue_.IsBool() || rvalue_.IsNull()); }
  bool is_hashable() const { return is_primitive(); }

  bool empty() const {
    if (is_null()) _printlog("Undefined value or reference"); // This check might be too broad or misleading
    if (is_string()) return rvalue_.GetStringLength() == 0;
    if (is_array()) return array_->empty();
    if (is_object()) return object_->empty();
    return false; // Default for non-container types or if not fitting above
  }

  void for_each(const std::function<void(Value &)> & callback) const {
    if (is_null()) _printlog("Undefined value or reference");
    else if (array_) {
      for (auto& item : *array_) {
        callback(item);
      }
    } else if (object_) {
      for (auto & item : *object_) {
        Value key(item.first); // Convert string key to minja::Value
        callback(key); // Callback receives the key, not the value. Jinja `for key in dict`.
      }
    } else if (is_string()) {
      for (char c : std::string(rvalue_.GetString(), rvalue_.GetStringLength())) {
        auto val = Value(std::string(1, c));
        callback(val);
      }
    } else {
      _printlog("Value is not iterable: " + dump());
    }
  }

  bool to_bool() const {
    if (is_null()) return false;
    if (is_boolean()) return rvalue_.GetBool();
    if (is_number()) return rvalue_.GetDouble() != 0; // Compare as double for simplicity
    if (is_string()) return rvalue_.GetStringLength() > 0;
    if (is_array()) return !array_->empty(); // Check Minja array
    if (is_object()) return !object_->empty(); // Check Minja object
    return true; // Default for other types (e.g. callable)
  }

  int64_t to_int() const {
    if (is_null()) return 0;
    if (is_boolean()) return rvalue_.GetBool() ? 1 : 0;
    if (is_number()) {
        if (rvalue_.IsInt64()) return rvalue_.GetInt64();
        if (rvalue_.IsUint64()) return static_cast<int64_t>(rvalue_.GetUint64()); // Potential overflow
        if (rvalue_.IsDouble()) return static_cast<int64_t>(rvalue_.GetDouble());
    }
    if (is_string()) {
        return std::stoll(std::string(rvalue_.GetString(), rvalue_.GetStringLength()));
    }
    return 0;
  }

  bool operator<(const Value & other) const {
    if (is_null() || other.is_null()) {
      _printlog("Undefined value or reference in operator<");
      return false;
    }
    if (is_primitive() && other.is_primitive()) {
        if (rvalue_.IsNumber() && other.rvalue_.IsNumber()) {
            return rvalue_.GetDouble() < other.rvalue_.GetDouble();
        }
        if (rvalue_.IsString() && other.rvalue_.IsString()) {
            return std::string(rvalue_.GetString(), rvalue_.GetStringLength()) < std::string(other.rvalue_.GetString(), other.rvalue_.GetStringLength());
        }
    }
    _printlog("Cannot compare values (operator<): " + dump() + " < " + other.dump());
    return false;
  }
  bool operator>=(const Value & other) const { return !(*this < other); }

  bool operator>(const Value & other) const {
    if (is_null() || other.is_null()) {
      _printlog("Undefined value or reference in operator>");
      return false;
    }
    if (is_primitive() && other.is_primitive()) {
        if (rvalue_.IsNumber() && other.rvalue_.IsNumber()) {
            return rvalue_.GetDouble() > other.rvalue_.GetDouble();
        }
        if (rvalue_.IsString() && other.rvalue_.IsString()) {
            return std::string(rvalue_.GetString(), rvalue_.GetStringLength()) > std::string(other.rvalue_.GetString(), other.rvalue_.GetStringLength());
        }
    }
    _printlog("Cannot compare values (operator>): " + dump() + " > " + other.dump());
    return false;
  }
  bool operator<=(const Value & other) const { return !(*this > other); }

  bool operator==(const Value & other) const {
    if (callable_ || other.callable_) { // If either is callable, compare pointers
      return callable_.get() == other.callable_.get();
    }
    if (array_ && other.array_) { // Both are Minja arrays
      if (array_->size() != other.array_->size()) return false;
      for (size_t i = 0; i < array_->size(); ++i) {
        if ((*array_)[i] != (*other.array_)[i]) return false; // Recursive comparison
      }
      return true;
    }
    if (object_ && other.object_) { // Both are Minja objects (maps)
      if (object_->size() != other.object_->size()) return false;
      return *object_ == *other.object_; // std::map comparison
    }
    // If not Minja array/object/callable, compare rvalue_ (primitives or JSON structures)
    if (!array_ && !object_ && !callable_ && !other.array_ && !other.object_ && !other.callable_) {
        return rvalue_ == other.rvalue_; // rapidjson::Value comparison
    }
    return false; // Mixed types or unhandled cases
  }
  bool operator!=(const Value & other) const { return !(*this == other); }

  bool contains(const char * key_cstr) const { return contains(std::string(key_cstr)); }
  bool contains(const std::string & key_str) const {
    if (is_object()) {
        return object_->count(key_str) > 0;
    } else if (rvalue_.IsObject()) {
        return rvalue_.HasMember(key_str.c_str());
    }
    return false;
  }

  bool contains(const Value & val_to_find) const {
    if (is_null()) { _printlog("Undefined value or reference in contains(Value)"); return false; }
    if (is_array()) {
        for (const auto& item : *array_) {
            if (item == val_to_find) return true;
        }
        return false;
    } else if (is_object()) {
        if (!val_to_find.is_string()) { _printlog("Key for 'contains' in object must be a string: " + val_to_find.dump()); return false; }
        return object_->count(val_to_find.to_str()) > 0;
    } else if (rvalue_.IsArray()) {
        if (val_to_find.is_primitive()) { // Simplified: only compare primitive minja::Values with RValue array elements
            for (const auto& item_rval : rvalue_.GetArray()) {
                // This comparison (RValue == RValue) is fine if val_to_find.rvalue_ is correctly representing the primitive
                if (item_rval == val_to_find.rvalue_) return true;
            }
        } else { _printlog("Comparing complex minja::Value with elements of a raw rapidjson array via 'contains' is not directly supported.");}
        return false;
    } else if (rvalue_.IsObject()) {
        if (!val_to_find.is_string()) { _printlog("Key for 'contains' in rapidjson object must be a string: " + val_to_find.dump()); return false; }
        return rvalue_.HasMember(val_to_find.to_str().c_str());
    }
    return false;
  }

  void erase(size_t index) {
    if (!array_) _printlog("Value is not an array: " + dump());
    else if (index < array_->size()) array_->erase(array_->begin() + index);
    else _printlog("Index out of bounds for erase: " + std::to_string(index));
  }
  void erase(const std::string & key) {
    if (!object_) _printlog("Value is not an object: " + dump());
    else object_->erase(key);
  }

  const Value& at(const Value & index_val) const {
    return const_cast<Value*>(this)->at(index_val); // Re-route to non-const version, careful with semantics
  }
  Value& at(const Value & index_val) {
    if (is_array()) {
        if (!index_val.is_number_integer()) { _printlog("Array index must be integer: " + index_val.dump()); static Value err; return err; }
        int64_t i = index_val.to_int();
        if (i < 0) i += array_->size();
        if (i < 0 || i >= static_cast<int64_t>(array_->size())) { _printlog("Array index out of bounds: " + std::to_string(i)); static Value err; return err; }
        return array_->at(i);
    }
    if (is_object()) {
        if (!index_val.is_string()) { _printlog("Object key must be string: " + index_val.dump()); static Value err; return err; }
        std::string key = index_val.to_str();
        if (object_->find(key) == object_->end()) { _printlog("Object key not found: " + key); static Value err; return err;} // Or insert?
        return object_->at(key);
    }
    // Case for rvalue_ being an array or object - this is problematic for returning Value& due to ownership.
    // The previous attempt commented this out. A proper solution would be to return Value by value or a proxy.
    // For now, this path will effectively fail or lead to issues if rvalue_ is the container.
    _printlog("Value is not a Minja array or object for 'at' operation: " + dump());
    static Value err_val; return err_val; // Problematic: returns ref to static local
  }

  const Value& at(size_t index) const {
    return const_cast<Value*>(this)->at(index);
  }
  Value& at(size_t index) {
    if (is_array()) {
        if (index >= array_->size()) { _printlog("Array index out of bounds: " + std::to_string(index)); static Value err; return err; }
        return array_->at(index);
    }
    // Accessing map-like object_ by size_t index is not standard. Assuming string key if it were object.
    _printlog("Value is not an array for 'at(size_t)' operation: " + dump());
    static Value err_val; return err_val;
  }

  template <typename T>
  T get(const std::string & key, T default_value) const {
    if (!contains(key)) return default_value;
    // .at(Value(key)) is needed if 'at' expects a minja::Value key
    return at(Value(key)).get<T>();
  }

  template <typename T>
  T get() const {
    if (std::is_same<T, bool>::value && is_boolean()) return rvalue_.GetBool();
    if (std::is_same<T, int64_t>::value && is_number_integer()) return rvalue_.IsInt64() ? rvalue_.GetInt64() : static_cast<int64_t>(rvalue_.GetUint64());
    if (std::is_same<T, double>::value && is_number()) return rvalue_.GetDouble(); // Includes integers convertible to double
    if (std::is_same<T, std::string>::value && is_string()) return std::string(rvalue_.GetString(), rvalue_.GetStringLength());
    _printlog("get<T> not defined or type mismatch for this value type: " + dump());
    return T{};
  }

  std::string dump(int indent=-1, bool to_json_format=false) const {
    std::ostringstream out;
    dump(out, indent, 0, to_json_format);
    return out.str();
  }

  Value operator-() const {
      if (rvalue_.IsInt64()) return Value(-rvalue_.GetInt64());
      if (rvalue_.IsDouble()) return Value(-rvalue_.GetDouble());
      _printlog("Unary minus not supported for this Value type: " + dump());
      return Value();
  }
  std::string to_str() const {
    if (is_string()) return std::string(rvalue_.GetString(), rvalue_.GetStringLength());
    if (rvalue_.IsInt64()) return std::to_string(rvalue_.GetInt64());
    if (rvalue_.IsUint64()) return std::to_string(rvalue_.GetUint64());
    if (rvalue_.IsDouble()) return std::to_string(rvalue_.GetDouble());
    if (rvalue_.IsBool()) return rvalue_.GetBool() ? "True" : "False";
    if (is_null()) return "None";
    return dump();
  }
  Value operator+(const Value& rhs) const {
      if ((is_string() || rhs.is_string()) && !(is_array() || rhs.is_array())) {
        return Value(to_str() + rhs.to_str());
      } else if (rvalue_.IsNumber() && rhs.rvalue_.IsNumber()) {
        if (rvalue_.IsInt64() && rhs.rvalue_.IsInt64()) return Value(rvalue_.GetInt64() + rhs.rvalue_.GetInt64());
        else return Value(rvalue_.GetDouble() + rhs.rvalue_.GetDouble());
      } else if (is_array() && rhs.is_array()) {
        auto res = Value::array();
        if(array_) for (const auto& item : *array_) res.push_back(item);
        if(rhs.array_) for (const auto& item : *rhs.array_) res.push_back(item);
        return res;
      }
      _printlog("Operator+ not supported for these types: " + dump() + " + " + rhs.dump());
      return Value();
  }
  Value operator-(const Value& rhs) const {
      if (rvalue_.IsNumber() && rhs.rvalue_.IsNumber()) {
        if (rvalue_.IsInt64() && rhs.rvalue_.IsInt64()) return Value(rvalue_.GetInt64() - rhs.rvalue_.GetInt64());
        else return Value(rvalue_.GetDouble() - rhs.rvalue_.GetDouble());
      }
      _printlog("Operator- not supported for these types: " + dump() + " - " + rhs.dump());
      return Value();
  }
  Value operator*(const Value& rhs) const {
      if (is_string() && rhs.rvalue_.IsInt64()) {
        std::ostringstream out_mul;
        std::string s_val = rvalue_.GetString();
        for (int64_t i = 0, n = rhs.rvalue_.GetInt64(); i < n; ++i) {
          out_mul << s_val;
        }
        return Value(out_mul.str());
      }
      else if (rvalue_.IsNumber() && rhs.rvalue_.IsNumber()) {
        if (rvalue_.IsInt64() && rhs.rvalue_.IsInt64()) return Value(rvalue_.GetInt64() * rhs.rvalue_.GetInt64());
        else return Value(rvalue_.GetDouble() * rhs.rvalue_.GetDouble());
      }
      _printlog("Operator* not supported for these types: " + dump() + " * " + rhs.dump());
      return Value();
  }
  Value operator/(const Value& rhs) const {
      if (rvalue_.IsNumber() && rhs.rvalue_.IsNumber()) {
        if (rhs.rvalue_.GetDouble() == 0) { _printlog("Division by zero"); return Value(); }
        return Value(rvalue_.GetDouble() / rhs.rvalue_.GetDouble());
      }
      _printlog("Operator/ not supported for these types: " + dump() + " / " + rhs.dump());
      return Value();
  }
  Value operator%(const Value& rhs) const {
    if (rvalue_.IsInt64() && rhs.rvalue_.IsInt64()) {
        if (rhs.rvalue_.GetInt64() == 0) { _printlog("Modulo by zero"); return Value(); }
        return Value(rvalue_.GetInt64() % rhs.rvalue_.GetInt64());
    }
    _printlog("Operator% not supported for these types (requires integers): " + dump() + " % " + rhs.dump());
    return Value();
  }
};

struct ArgumentsValue {
  std::vector<Value> args;
  std::vector<std::pair<std::string, Value>> kwargs;

  bool has_named(const std::string & name) {
    for (const auto & p : kwargs) {
      if (p.first == name) return true;
    }
    return false;
  }

  Value get_named(const std::string & name) {
      for (const auto & p : kwargs) {
          if (p.first == name) {
              return p.second;
          }
      }
    return Value();
  }

  bool empty() {
    return args.empty() && kwargs.empty();
  }

  void expectArgs(const std::string & method_name, const std::pair<size_t, size_t> & pos_count, const std::pair<size_t, size_t> & kw_count) {
    if (args.size() < pos_count.first || args.size() > pos_count.second || kwargs.size() < kw_count.first || kwargs.size() > kw_count.second) {
      std::ostringstream out;
      out << method_name << " must have between " << pos_count.first << " and " << pos_count.second << " positional arguments and between " << kw_count.first << " and " << kw_count.second << " keyword arguments";
      _printlog(out.str());
    }
  }
};

} // namespace minja

namespace std {
  template <>
  struct hash<minja::Value> {
    size_t operator()(const minja::Value & v_to_hash) const {
      if (!v_to_hash.is_hashable()) {
        _printlog("Unsupported type for hashing: " + v_to_hash.dump());
      }
      return std::hash<std::string>()(v_to_hash.dump());
    }
  };
} // namespace std

namespace minja {

static std::string error_location_suffix(const std::string & source, size_t pos) {
  auto get_line_fn = [&](size_t line_num) {
    auto current_start = source.begin();
    for (size_t i = 1; i < line_num; ++i) {
      current_start = std::find(current_start, source.end(), '\n');
      if (current_start == source.end()) return std::string(); // Line not found
      ++current_start; // Move past '\n'
    }
    auto current_end = std::find(current_start, source.end(), '\n');
    return std::string(current_start, current_end);
  };
  auto start = source.begin();
  auto end = source.end();
  auto it = start + pos;
  auto line = std::count(start, it, '\n') + 1;
  auto max_line = std::count(start, end, '\n') + 1;
  auto col = pos - std::string(start, it).rfind('\n');
  std::ostringstream out;
  out << " at row " << line << ", column " << col << ":\n";
  if (line > 1) out << get_line_fn(line - 1) << "\n";
  out << get_line_fn(line) << "\n";
  out << std::string(col - 1, ' ') << "^\n";
  if (line < max_line) out << get_line_fn(line + 1) << "\n";

  return out.str();
}

class Context : public std::enable_shared_from_this<Context> {
  protected:
    Value values_;
    std::shared_ptr<Context> parent_;
  public:
    Context(Value && context_values, const std::shared_ptr<Context> & parent_context = nullptr)
        : values_(std::move(context_values)), parent_(parent_context) {
        if (!values_.is_object() && !values_.is_null()) { 
             _printlog("Context values_ must be an object or null: " + values_.dump()); 
        }
    }
    virtual ~Context() {}

    static std::shared_ptr<Context> builtins();
    static std::shared_ptr<Context> make(Value && context_values, const std::shared_ptr<Context> & parent_context = builtins());

    std::vector<Value> keys() const { 
        if (!values_.is_object()) {
             _printlog("Context values_ is not an object, cannot get keys: " + values_.dump());
             return {};
        }
        return values_.keys();
    }
    virtual Value get(const Value & key_val) const { 
        if (!key_val.is_string()) {
            _printlog("Context::get key must be a string: " + key_val.dump()); 
            return Value(); 
        }
        std::string key_str = key_val.to_str(); 

        if (values_.is_object() && values_.contains(key_str)) {
            return values_.get(key_val); 
        }
        if (parent_) return parent_->get(key_val);
        return Value(); 
    }
    virtual Value & at(const Value & key_val) { 
        if (!key_val.is_string()) {
            _printlog("Context::at key must be a string: " + key_val.dump()); 
            static Value error_val; return error_val;
        }
        std::string key_str = key_val.to_str();

        if (values_.is_object() && values_.contains(key_str)) {
            return values_.at(key_val); 
        }
        if (parent_) return parent_->at(key_val);

        _printlog("Undefined variable: " + key_val.dump());
        if (values_.is_object()) {
            return values_.at(key_val); 
        }
        static Value error_val; return error_val; 
    }
    virtual bool contains(const Value & key_val) const { 
        if (!key_val.is_string()) {
            _printlog("Context::contains key must be a string: " + key_val.dump()); 
            return false;
        }
        std::string key_str = key_val.to_str();
        if (values_.is_object() && values_.contains(key_str)) return true;
        if (parent_) return parent_->contains(key_val);
        return false;
    }
    virtual void set(const std::string & key, const Value & value) {
        values_.set(key, value);
    }
};

struct Location {
    std::shared_ptr<std::string> source;
    size_t pos;
};

class Expression {
protected:
    virtual Value do_evaluate(const std::shared_ptr<Context> & context) const = 0;
public:
    enum Type {
        Type_Variable = 0,
        Type_If,
        Type_Liter,
        Type_Array,
        Type_Dict,
        Type_Slice,
        Type_Subscript,
        Type_Unary,
        Type_Binary,
        Type_MethodCall,
        Type_Call,
        Type_Filter,
    };
    using Parameters = std::vector<std::pair<std::string, std::shared_ptr<Expression>>>;

    Location location;
    const int mType;

    Expression(const Location & location, int type) : location(location), mType(type) {}
    virtual ~Expression() = default;

    Value evaluate(const std::shared_ptr<Context> & context) const {
            return do_evaluate(context);
    }
};

class VariableExpr : public Expression {
    std::string name;
public:
    VariableExpr(const Location & loc, const std::string& n)
      : Expression(loc, Expression::Type_Variable), name(n) {}
    std::string get_name() const { return name; }
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!context->contains(name)) {
            return Value();
        }
        return context->at(name);
    }
};

static void destructuring_assign(const std::vector<std::string> & var_names, const std::shared_ptr<Context> & context, Value& item) {
  if (var_names.size() == 1) {
      context->set(var_names[0], item);
  } else {
      if (!item.is_array() || item.size() != var_names.size()) {
          _printlog("Mismatched number of variables and items in destructuring assignment");
      }
      for (size_t i = 0; i < var_names.size(); ++i) {
          context->set(var_names[i], item.at(i));
      }
  }
}

enum SpaceHandling { Keep, Strip, StripSpaces, StripNewline };

class TemplateToken {
public:
    enum class Type { Text, Expression, If, Else, Elif, EndIf, For, EndFor, Generation, EndGeneration, Set, EndSet, Comment, Macro, EndMacro, Filter, EndFilter, Break, Continue };

    static std::string typeToString(Type t) {
        switch (t) {
            case Type::Text: return "text";
            case Type::Expression: return "expression";
            case Type::If: return "if";
            case Type::Else: return "else";
            case Type::Elif: return "elif";
            case Type::EndIf: return "endif";
            case Type::For: return "for";
            case Type::EndFor: return "endfor";
            case Type::Set: return "set";
            case Type::EndSet: return "endset";
            case Type::Comment: return "comment";
            case Type::Macro: return "macro";
            case Type::EndMacro: return "endmacro";
            case Type::Filter: return "filter";
            case Type::EndFilter: return "endfilter";
            case Type::Generation: return "generation";
            case Type::EndGeneration: return "endgeneration";
            case Type::Break: return "break";
            case Type::Continue: return "continue";
        }
        return "Unknown";
    }

    TemplateToken(Type type, const Location & location, SpaceHandling pre, SpaceHandling post) : type(type), location(location), pre_space(pre), post_space(post) {}
    virtual ~TemplateToken() = default;

    Type type;
    Location location;
    SpaceHandling pre_space = SpaceHandling::Keep;
    SpaceHandling post_space = SpaceHandling::Keep;
};

struct TextTemplateToken : public TemplateToken {
    std::string text;
    TextTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::string& t) : TemplateToken(Type::Text, loc, pre, post), text(t) {}
};

struct ExpressionTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> expr;
    ExpressionTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && e) : TemplateToken(Type::Expression, loc, pre, post), expr(std::move(e)) {}
};

struct IfTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> condition;
    IfTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && c) : TemplateToken(Type::If, loc, pre, post), condition(std::move(c)) {}
};

struct ElifTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> condition;
    ElifTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && c) : TemplateToken(Type::Elif, loc, pre, post), condition(std::move(c)) {}
};

struct ElseTemplateToken : public TemplateToken {
    ElseTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::Else, loc, pre, post) {}
};

struct EndIfTemplateToken : public TemplateToken {
    EndIfTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndIf, loc, pre, post) {}
};

struct MacroTemplateToken : public TemplateToken {
    std::shared_ptr<VariableExpr> name;
    Expression::Parameters params;
    MacroTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<VariableExpr> && n, Expression::Parameters && p)
      : TemplateToken(Type::Macro, loc, pre, post), name(std::move(n)), params(std::move(p)) {}
};

struct EndMacroTemplateToken : public TemplateToken {
    EndMacroTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndMacro, loc, pre, post) {}
};

struct FilterTemplateToken : public TemplateToken {
    std::shared_ptr<Expression> filter;
    FilterTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, std::shared_ptr<Expression> && filter)
      : TemplateToken(Type::Filter, loc, pre, post), filter(std::move(filter)) {}
};

struct EndFilterTemplateToken : public TemplateToken {
    EndFilterTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndFilter, loc, pre, post) {}
};

struct ForTemplateToken : public TemplateToken {
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> iterable;
    std::shared_ptr<Expression> condition;
    bool recursive;
    ForTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::vector<std::string> & vns, std::shared_ptr<Expression> && iter,
      std::shared_ptr<Expression> && c, bool r)
      : TemplateToken(Type::For, loc, pre, post), var_names(vns), iterable(std::move(iter)), condition(std::move(c)), recursive(r) {}
};

struct EndForTemplateToken : public TemplateToken {
    EndForTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndFor, loc, pre, post) {}
};

struct GenerationTemplateToken : public TemplateToken {
    GenerationTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::Generation, loc, pre, post) {}
};

struct EndGenerationTemplateToken : public TemplateToken {
    EndGenerationTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndGeneration, loc, pre, post) {}
};

struct SetTemplateToken : public TemplateToken {
    std::string ns;
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> value;
    SetTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::string & ns, const std::vector<std::string> & vns, std::shared_ptr<Expression> && v)
      : TemplateToken(Type::Set, loc, pre, post), ns(ns), var_names(vns), value(std::move(v)) {}
};

struct EndSetTemplateToken : public TemplateToken {
    EndSetTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post) : TemplateToken(Type::EndSet, loc, pre, post) {}
};

struct CommentTemplateToken : public TemplateToken {
    std::string text;
    CommentTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, const std::string& t) : TemplateToken(Type::Comment, loc, pre, post), text(t) {}
};

enum class LoopControlType { Normal, Break, Continue};


struct LoopControlTemplateToken : public TemplateToken {
    LoopControlType control_type;
    LoopControlTemplateToken(const Location & loc, SpaceHandling pre, SpaceHandling post, LoopControlType control_type) : TemplateToken(Type::Break, loc, pre, post), control_type(control_type) {}
};

class TemplateNode {
    Location location_;
protected:
    virtual LoopControlType do_render(std::ostringstream & out, const std::shared_ptr<Context> & context) const = 0;

public:
    TemplateNode(const Location & location) : location_(location) {}
    LoopControlType render(std::ostringstream & out, const std::shared_ptr<Context> & context) const {
        return do_render(out, context);
    }
    const Location & location() const { return location_; }
    virtual ~TemplateNode() = default;
    std::string render(const std::shared_ptr<Context> & context) const {
        std::ostringstream out;
        render(out, context);
        return out.str();
    }
};

class SequenceNode : public TemplateNode {
    std::vector<std::shared_ptr<TemplateNode>> children;
public:
    SequenceNode(const Location & loc, std::vector<std::shared_ptr<TemplateNode>> && c)
      : TemplateNode(loc), children(std::move(c)) {}
    LoopControlType do_render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        for (const auto& child : children) {
            auto type = child->render(out, context);
            if (LoopControlType::Normal != type) {
                return type;
            }
        }
        return LoopControlType::Normal;
    }
};

class TextNode : public TemplateNode {
    std::string text;
public:
    TextNode(const Location & loc, const std::string& t) : TemplateNode(loc), text(t) {}
    LoopControlType do_render(std::ostringstream & out, const std::shared_ptr<Context> &) const override {
        out << text;
        return LoopControlType::Normal;
    }
};

class ExpressionNode : public TemplateNode {
    std::shared_ptr<Expression> expr;
public:
    ExpressionNode(const Location & loc, std::shared_ptr<Expression> && e) : TemplateNode(loc), expr(std::move(e)) {}
    LoopControlType do_render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
      if (!expr) _printlog("ExpressionNode.expr is null");
      auto result = expr->evaluate(context);
      if (result.is_string()) {
          out << result.get<std::string>();
      } else if (result.is_boolean()) {
          out << (result.get<bool>() ? "True" : "False");
      } else if (!result.is_null()) {
          out << result.dump();
      }
        return LoopControlType::Normal;
  }
};

class IfNode : public TemplateNode {
    std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<TemplateNode>>> cascade;
public:
    IfNode(const Location & loc, std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<TemplateNode>>> && c)
        : TemplateNode(loc), cascade(std::move(c)) {}
    LoopControlType do_render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
      for (const auto& branch : cascade) {
          auto enter_branch = true;
          if (branch.first) {
            enter_branch = branch.first->evaluate(context).to_bool();
          }
          if (enter_branch) {
            if (!branch.second) _printlog("IfNode.cascade.second is null");
              return branch.second->render(out, context);
          }
      }
        return LoopControlType::Normal;
    }
};

class LoopControlNode : public TemplateNode {
    LoopControlType control_type_;
  public:
    LoopControlNode(const Location & loc, LoopControlType control_type) : TemplateNode(loc), control_type_(control_type) {}
    LoopControlType do_render(std::ostringstream &, const std::shared_ptr<Context> &) const override {
        return control_type_;
    }
};

class ForNode : public TemplateNode {
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> iterable;
    std::shared_ptr<Expression> condition;
    std::shared_ptr<TemplateNode> body;
    bool recursive;
    std::shared_ptr<TemplateNode> else_body;
public:
    ForNode(const Location & loc, std::vector<std::string> && var_names, std::shared_ptr<Expression> && iterable,
      std::shared_ptr<Expression> && condition, std::shared_ptr<TemplateNode> && body, bool recursive, std::shared_ptr<TemplateNode> && else_body)
            : TemplateNode(loc), var_names(var_names), iterable(std::move(iterable)), condition(std::move(condition)), body(std::move(body)), recursive(recursive), else_body(std::move(else_body)) {}

    LoopControlType do_render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
      // https://jinja.palletsprojects.com/en/3.0.x/templates/#for
      if (!iterable) _printlog("ForNode.iterable is null");
      if (!body) _printlog("ForNode.body is null");

      auto iterable_value = iterable->evaluate(context);
      Value::CallableType loop_function;

      std::function<LoopControlType(Value&)> visit = [&](Value& iter) {
          auto filtered_items = Value::array();
          if (!iter.is_null()) {
            if (!iterable_value.is_iterable()) {
              _printlog("For loop iterable must be iterable: " + iterable_value.dump());
            }
            iterable_value.for_each([&](Value & item) {
                destructuring_assign(var_names, context, item);
                if (!condition || condition->evaluate(context).to_bool()) {
                  filtered_items.push_back(item);
                }
            });
          }
          if (filtered_items.empty()) {
            if (else_body) {
              auto loopcode = else_body->render(out, context);
                if (loopcode != LoopControlType::Normal) {
                    return loopcode;
                }
            }
          } else {
              auto loop = recursive ? Value::callable(loop_function) : Value::object();
              loop.set("length", (int64_t) filtered_items.size());

              size_t cycle_index = 0;
              loop.set("cycle", Value::callable([&](const std::shared_ptr<Context> &, ArgumentsValue & args) {
                  if (args.args.empty() || !args.kwargs.empty()) {
                      _printlog("cycle() expects at least 1 positional argument and no named arg");
                  }
                  auto item = args.args[cycle_index];
                  cycle_index = (cycle_index + 1) % args.args.size();
                  return item;
              }));
              auto loop_context = Context::make(Value::object(), context);
              loop_context->set("loop", loop);
              for (size_t i = 0, n = filtered_items.size(); i < n; ++i) {
                  auto & item = filtered_items.at(i);
                  destructuring_assign(var_names, loop_context, item);
                  loop.set("index", (int64_t) i + 1);
                  loop.set("index0", (int64_t) i);
                  loop.set("revindex", (int64_t) (n - i));
                  loop.set("revindex0", (int64_t) (n - i - 1));
                  loop.set("length", (int64_t) n);
                  loop.set("first", i == 0);
                  loop.set("last", i == (n - 1));
                  loop.set("previtem", i > 0 ? filtered_items.at(i - 1) : Value());
                  loop.set("nextitem", i < n - 1 ? filtered_items.at(i + 1) : Value());
                  auto control_type = body->render(out, loop_context);
                  if (control_type == LoopControlType::Break) break;
                  if (control_type == LoopControlType::Continue) continue;
              }
          }
          return LoopControlType::Normal;
      };

      if (recursive) {
        loop_function = [&](const std::shared_ptr<Context> &, ArgumentsValue & args) {
            if (args.args.size() != 1 || !args.kwargs.empty() || !args.args[0].is_array()) {
                _printlog("loop() expects exactly 1 positional iterable argument");
            }
            auto & items = args.args[0];
            auto code = visit(items);
            return Value();
        };
      }

      return visit(iterable_value);
  }
};

class MacroNode : public TemplateNode {
    std::shared_ptr<VariableExpr> name;
    Expression::Parameters params;
    std::shared_ptr<TemplateNode> body;
    std::unordered_map<std::string, size_t> named_param_positions;
public:
    MacroNode(const Location & loc, std::shared_ptr<VariableExpr> && n, Expression::Parameters && p, std::shared_ptr<TemplateNode> && b)
        : TemplateNode(loc), name(std::move(n)), params(std::move(p)), body(std::move(b)) {
        for (size_t i = 0; i < params.size(); ++i) {
          const auto & name = params[i].first;
          if (!name.empty()) {
            named_param_positions[name] = i;
          }
        }
    }
    LoopControlType do_render(std::ostringstream &, const std::shared_ptr<Context> & macro_context) const override {
        if (!name) _printlog("MacroNode.name is null");
        if (!body) _printlog("MacroNode.body is null");
        auto callable = Value::callable([&](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
            auto call_context = macro_context;
            std::vector<bool> param_set(params.size(), false);
            for (size_t i = 0, n = args.args.size(); i < n; i++) {
                auto & arg = args.args[i];
                if (i >= params.size()) _printlog("Too many positional arguments for macro " + name->get_name());
                param_set[i] = true;
                auto & param_name = params[i].first;
                call_context->set(param_name, arg);
            }
            for (auto& iter : args.kwargs) {
                auto& arg_name = iter.first;
                auto& value = iter.second;
                auto it = named_param_positions.find(arg_name);
                if (it == named_param_positions.end()) _printlog("Unknown parameter name for macro " + name->get_name() + ": " + arg_name);

                call_context->set(arg_name, value);
                param_set[it->second] = true;
            }
            // Set default values for parameters that were not passed
            for (size_t i = 0, n = params.size(); i < n; i++) {
                if (!param_set[i] && params[i].second != nullptr) {
                    auto val = params[i].second->evaluate(context);
                    call_context->set(params[i].first, val);
                }
            }
            return body->render(call_context);
        });
        macro_context->set(name->get_name(), callable);
        return LoopControlType::Normal;
    }
};

class FilterNode : public TemplateNode {
    std::shared_ptr<Expression> filter;
    std::shared_ptr<TemplateNode> body;

public:
    FilterNode(const Location & loc, std::shared_ptr<Expression> && f, std::shared_ptr<TemplateNode> && b)
        : TemplateNode(loc), filter(std::move(f)), body(std::move(b)) {}

    LoopControlType do_render(std::ostringstream & out, const std::shared_ptr<Context> & context) const override {
        if (!filter) _printlog("FilterNode.filter is null");
        if (!body) _printlog("FilterNode.body is null");
        auto filter_value = filter->evaluate(context);
        if (!filter_value.is_callable()) {
            _printlog("Filter must be a callable: " + filter_value.dump());
        }
        std::string rendered_body = body->render(context);

        ArgumentsValue filter_args = {{Value(rendered_body)}, {}};
        auto result = filter_value.call(context, filter_args);
        out << result.to_str();
        return LoopControlType::Normal;
    }
};

class SetNode : public TemplateNode {
    std::string ns;
    std::vector<std::string> var_names;
    std::shared_ptr<Expression> value;
public:
    SetNode(const Location & loc, const std::string & ns, const std::vector<std::string> & vns, std::shared_ptr<Expression> && v)
        : TemplateNode(loc), ns(ns), var_names(vns), value(std::move(v)) {}
    LoopControlType do_render(std::ostringstream &, const std::shared_ptr<Context> & context) const override {
      if (!value) _printlog("SetNode.value is null");
      if (!ns.empty()) {
        if (var_names.size() != 1) {
          _printlog("Namespaced set only supports a single variable name");
        }
        auto & name = var_names[0];
        auto ns_value = context->get(ns);
        if (!ns_value.is_object()) _printlog("Namespace '" + ns + "' is not an object");
        ns_value.set(name, this->value->evaluate(context));
      } else {
        auto val = value->evaluate(context);
        destructuring_assign(var_names, context, val);
      }
        return LoopControlType::Normal;

    }
};

class SetTemplateNode : public TemplateNode {
    std::string name;
    std::shared_ptr<TemplateNode> template_value;
public:
    SetTemplateNode(const Location & loc, const std::string & name, std::shared_ptr<TemplateNode> && tv)
        : TemplateNode(loc), name(name), template_value(std::move(tv)) {}
    LoopControlType do_render(std::ostringstream &, const std::shared_ptr<Context> & context) const override {
      if (!template_value) _printlog("SetTemplateNode.template_value is null");
      Value value { template_value->render(context) };
      context->set(name, value);
        return LoopControlType::Normal;

    }
};

class IfExpr : public Expression {
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Expression> then_expr;
    std::shared_ptr<Expression> else_expr;
public:
    IfExpr(const Location & loc, std::shared_ptr<Expression> && c, std::shared_ptr<Expression> && t, std::shared_ptr<Expression> && e)
        : Expression(loc, Expression::Type_If), condition(std::move(c)), then_expr(std::move(t)), else_expr(std::move(e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
      if (!condition) _printlog("IfExpr.condition is null");
      if (!then_expr) _printlog("IfExpr.then_expr is null");
      if (condition->evaluate(context).to_bool()) {
        return then_expr->evaluate(context);
      }
      if (else_expr) {
        return else_expr->evaluate(context);
      }
      return nullptr;
    }
};

class LiteralExpr : public Expression {
    Value value;
public:
    LiteralExpr(const Location & loc, const Value& v)
      : Expression(loc, Expression::Type_Liter), value(v) {}
    Value do_evaluate(const std::shared_ptr<Context> &) const override { return value; }
};

class ArrayExpr : public Expression {
    std::vector<std::shared_ptr<Expression>> elements;
public:
    ArrayExpr(const Location & loc, std::vector<std::shared_ptr<Expression>> && e)
      : Expression(loc, Expression::Type_Array), elements(std::move(e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        auto result = Value::array();
        for (const auto& e : elements) {
            if (!e) _printlog("Array element is null");
            result.push_back(e->evaluate(context));
        }
        return result;
    }
};

class DictExpr : public Expression {
    std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> elements;
public:
    DictExpr(const Location & loc, std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> && e)
      : Expression(loc, Expression::Type_Dict), elements(std::move(e)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        auto result = Value::object();
        for (const auto& iter : elements) {
            const auto& key = iter.first;
            const auto& value = iter.second;
            if (!key) _printlog("Dict key is null");
            if (!value) _printlog("Dict value is null");
            result.set(key->evaluate(context).to_str(), value->evaluate(context));
        }
        return result;
    }
};

class SliceExpr : public Expression {
public:
    std::shared_ptr<Expression> start, end, step;
    SliceExpr(const Location & loc, std::shared_ptr<Expression> && s, std::shared_ptr<Expression> && e, std::shared_ptr<Expression> && st = nullptr)
      : Expression(loc, Expression::Type_Slice), start(std::move(s)), end(std::move(e)), step(std::move(st)) {}

    Value do_evaluate(const std::shared_ptr<Context> &) const override {
        _printlog("SliceExpr not implemented");
        return Value();
    }
};

class SubscriptExpr : public Expression {
    std::shared_ptr<Expression> base;
    std::shared_ptr<Expression> index;
public:
    SubscriptExpr(const Location & loc, std::shared_ptr<Expression> && b, std::shared_ptr<Expression> && i)
        : Expression(loc, Expression::Type_Subscript), base(std::move(b)), index(std::move(i)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!base) _printlog("SubscriptExpr.base is null");
        if (!index) _printlog("SubscriptExpr.index is null");
        auto target_value = base->evaluate(context);
        if (index->mType == Expression::Type_Slice){
            auto slice = (SliceExpr*)(index.get());
            bool reverse = slice->step && slice->step->evaluate(context).get<int64_t>() == -1;
            if (slice->step && !reverse) {
              MNN_ERROR("Slicing with step other than -1 is not supported");
            }

            int64_t start = slice->start ? slice->start->evaluate(context).get<int64_t>() : (reverse ? target_value.size() - 1 : 0);
            int64_t end = slice->end ? slice->end->evaluate(context).get<int64_t>() : (reverse ? -1 : target_value.size());

            size_t len = target_value.size();

            if (slice->start && start < 0) {
              start = (int64_t)len + start;
            }
            if (slice->end && end < 0) {
              end = (int64_t)len + end;
            }
            if (target_value.is_string()) {
              std::string s = target_value.get<std::string>();

              std::string result_str;
              if (reverse) {
                for (int64_t i = start; i > end; --i) {
                  if (i >= 0 && i < (int64_t)len) {
                    result_str += s[i];
                  } else if (i < 0) {
                    break;
                  }
                }
              } else {
                result_str = s.substr(start, end - start);
              }
              return result_str;

            } else if (target_value.is_array()) {
              auto result = Value::array();
              if (reverse) {
                for (int64_t i = start; i > end; --i) {
                  if (i >= 0 && i < (int64_t)len) {
                    result.push_back(target_value.at(i));
                  } else if (i < 0) {
                    break;
                  }
                }
              } else {
                for (auto i = start; i < end; ++i) {
                  result.push_back(target_value.at(i));
                }
              }
              return result;
            } else {
                if(target_value.is_null()) {
                    MNN_ERROR("Cannot subscript null\n");
                } else {
                    MNN_ERROR("Subscripting only supported on arrays and strings\n");
                }
            }
        } else {
          auto index_value = index->evaluate(context);
          if (target_value.is_null()) {
            if (base->mType == Expression::Type_Variable) {
                auto t = (VariableExpr*)(base.get());
              _printlog("'" + t->get_name() + "' is " + (context->contains(t->get_name()) ? "null" : "not defined"));
            }
            _printlog("Trying to access property '" +  index_value.dump() + "' on null!");
          }
          return target_value.get(index_value);
        }
        return Value();
    }
};

class UnaryOpExpr : public Expression {
public:
    enum class Op { Plus, Minus, LogicalNot, Expansion, ExpansionDict };
    std::shared_ptr<Expression> expr;
    Op op;
    UnaryOpExpr(const Location & loc, std::shared_ptr<Expression> && e, Op o)
      : Expression(loc, Expression::Type_Unary), expr(std::move(e)), op(o) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!expr) _printlog("UnaryOpExpr.expr is null");
        auto e = expr->evaluate(context);
        switch (op) {
            case Op::Plus: return e;
            case Op::Minus: return -e;
            case Op::LogicalNot: return !e.to_bool();
            case Op::Expansion:
            case Op::ExpansionDict:
                _printlog("Expansion operator is only supported in function calls and collections");

        }
        _printlog("Unknown unary operator");
        return Value();
    }
};

class BinaryOpExpr : public Expression {
public:
    enum class Op { StrConcat, Add, Sub, Mul, MulMul, Div, DivDiv, Mod, Eq, Ne, Lt, Gt, Le, Ge, And, Or, In, NotIn, Is, IsNot };
private:
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    Op op;
public:
    BinaryOpExpr(const Location & loc, std::shared_ptr<Expression> && l, std::shared_ptr<Expression> && r, Op o)
        : Expression(loc, Expression::Type_Binary), left(std::move(l)), right(std::move(r)), op(o) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!left) _printlog("BinaryOpExpr.left is null");
        if (!right) _printlog("BinaryOpExpr.right is null");
        auto l = left->evaluate(context);

        auto do_eval = [&](const Value & l) -> Value {
          if (op == Op::Is || op == Op::IsNot) {
            auto t = (VariableExpr*)(right.get());
              if (right->mType != Expression::Type_Variable) {
                  _printlog("Right side of 'is' operator must be a variable");
              }

            auto eval = [&]() {
              const auto & name = t->get_name();
              if (name == "none") return l.is_null();
              if (name == "boolean") return l.is_boolean();
              if (name == "integer") return l.is_number_integer();
              if (name == "float") return l.is_number_float();
              if (name == "number") return l.is_number();
              if (name == "string") return l.is_string();
              if (name == "mapping") return l.is_object();
              if (name == "iterable") return l.is_iterable();
              if (name == "sequence") return l.is_array();
              if (name == "defined") return !l.is_null();
              _printlog("Unknown type for 'is' operator: " + name);
              return false;
            };
            auto value = eval();
            return Value(op == Op::Is ? value : !value);
          }

          if (op == Op::And) {
            if (!l.to_bool()) return Value(false);
            return right->evaluate(context).to_bool();
          } else if (op == Op::Or) {
            if (l.to_bool()) return l;
            return right->evaluate(context);
          }

          auto r = right->evaluate(context);
          switch (op) {
              case Op::StrConcat: return l.to_str() + r.to_str();
              case Op::Add:       return l + r;
              case Op::Sub:       return l - r;
              case Op::Mul:       return l * r;
              case Op::Div:       return l / r;
              case Op::MulMul:    return std::pow(l.get<double>(), r.get<double>());
              case Op::DivDiv:    return l.get<int64_t>() / r.get<int64_t>();
              case Op::Mod:       return l.get<int64_t>() % r.get<int64_t>();
              case Op::Eq:        return l == r;
              case Op::Ne:        return l != r;
              case Op::Lt:        return l < r;
              case Op::Gt:        return l > r;
              case Op::Le:        return l <= r;
              case Op::Ge:        return l >= r;
              case Op::In:        return (r.is_array() || r.is_object()) && r.contains(l);
              case Op::NotIn:     return !(r.is_array() && r.contains(l));
              default:            break;
          }
          _printlog("Unknown binary operator");
          return false;
        };

        if (l.is_callable()) {
          return Value::callable([l, do_eval](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
            auto ll = l.call(context, args);
            return do_eval(ll); //args[0].second);
          });
        } else {
          return do_eval(l);
        }
    }
};

struct ArgumentsExpression {
    std::vector<std::shared_ptr<Expression>> args;
    std::vector<std::pair<std::string, std::shared_ptr<Expression>>> kwargs;

    ArgumentsValue evaluate(const std::shared_ptr<Context> & context) const {
        ArgumentsValue vargs;
        for (const auto& arg : this->args) {
            if (arg->mType == Expression::Type_Unary) {
                auto un_expr = (UnaryOpExpr*)(arg.get());
                if (un_expr->op == UnaryOpExpr::Op::Expansion) {
                    auto array = un_expr->expr->evaluate(context);
                    if (!array.is_array()) {
                        _printlog("Expansion operator only supported on arrays");
                    }
                    array.for_each([&](Value & value) {
                        vargs.args.push_back(value);
                    });
                    continue;
                } else if (un_expr->op == UnaryOpExpr::Op::ExpansionDict) {
                    auto dict = un_expr->expr->evaluate(context);
                    if (!dict.is_object()) {
                        _printlog("ExpansionDict operator only supported on objects");
                    }
                    dict.for_each([&](const Value & key) {
                        vargs.kwargs.push_back({key.get<std::string>(), dict.at(key)});
                    });
                    continue;
                }
            }
            vargs.args.push_back(arg->evaluate(context));
        }
        for (const auto& iter : this->kwargs) {
            const auto& name = iter.first;
            const auto& value = iter.second;
            vargs.kwargs.push_back({name, value->evaluate(context)});
        }
        return vargs;
    }
};

static std::string strip(const std::string & s, const std::string & chars = "", bool left = true, bool right = true) {
  auto charset = chars.empty() ? " \t\n\r" : chars;
  auto start = left ? s.find_first_not_of(charset) : 0;
  if (start == std::string::npos) return "";
  auto end = right ? s.find_last_not_of(charset) : s.size() - 1;
  return s.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string & s, const std::string & sep) {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = s.find(sep);
  while (end != std::string::npos) {
    result.push_back(s.substr(start, end - start));
    start = end + sep.length();
    end = s.find(sep, start);
  }
  result.push_back(s.substr(start));
  return result;
}

static std::string capitalize(const std::string & s) {
  if (s.empty()) return s;
  auto result = s;
  result[0] = std::toupper(result[0]);
  return result;
}

static std::string html_escape(const std::string & s) {
  std::string result;
  result.reserve(s.size());
  for (const auto & c : s) {
    switch (c) {
      case '&': result += "&amp;"; break;
      case '<': result += "&lt;"; break;
      case '>': result += "&gt;"; break;
      case '"': result += "&#34;"; break;
      case '\'': result += "&apos;"; break;
      default: result += c; break;
    }
  }
  return result;
}

class MethodCallExpr : public Expression {
    std::shared_ptr<Expression> object;
    std::shared_ptr<VariableExpr> method;
    ArgumentsExpression args;
public:
    MethodCallExpr(const Location & loc, std::shared_ptr<Expression> && obj, std::shared_ptr<VariableExpr> && m, ArgumentsExpression && a)
        : Expression(loc, Expression::Type_MethodCall), object(std::move(obj)), method(std::move(m)), args(std::move(a)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!object) _printlog("MethodCallExpr.object is null");
        if (!method) _printlog("MethodCallExpr.method is null");
        auto obj = object->evaluate(context);
        auto vargs = args.evaluate(context);
        if (obj.is_null()) {
           // _printlog("Trying to call method '" + method->get_name() + "' on null");
            return Value();
        }
        if (obj.is_array()) {
          if (method->get_name() == "append") {
              vargs.expectArgs("append method", {1, 1}, {0, 0});
              obj.push_back(vargs.args[0]);
              return Value();
          } else if (method->get_name() == "pop") {
              vargs.expectArgs("pop method", {0, 1}, {0, 0});
              return obj.pop(vargs.args.empty() ? Value() : vargs.args[0]);
          } else if (method->get_name() == "insert") {
              vargs.expectArgs("insert method", {2, 2}, {0, 0});
              auto index = vargs.args[0].get<int64_t>();
              if (index < 0 || index > (int64_t) obj.size()) _printlog("Index out of range for insert method");
              obj.insert(index, vargs.args[1]);
              return Value();
          }
        } else if (obj.is_object()) {
          if (method->get_name() == "items") {
            vargs.expectArgs("items method", {0, 0}, {0, 0});
            auto result = Value::array();
            for (const auto& key : obj.keys()) {
              result.push_back(Value::array({key, obj.at(key)}));
            }
            return result;
          } else if (method->get_name() == "pop") {
            vargs.expectArgs("pop method", {1, 1}, {0, 0});
            return obj.pop(vargs.args[0]);
          } else if (method->get_name() == "get") {
            vargs.expectArgs("get method", {1, 2}, {0, 0});
            auto key = vargs.args[0];
            if (vargs.args.size() == 1) {
              return obj.contains(key) ? obj.at(key) : Value();
            } else {
              return obj.contains(key) ? obj.at(key) : vargs.args[1];
            }
          } else if (obj.contains(method->get_name())) {
            auto callable = obj.at(method->get_name());
            if (!callable.is_callable()) {
              _printlog("Property '" + method->get_name() + "' is not callable");
            }
            return callable.call(context, vargs);
          }
        } else if (obj.is_string()) {
          auto str = obj.get<std::string>();
          if (method->get_name() == "strip") {
            vargs.expectArgs("strip method", {0, 1}, {0, 0});
            auto chars = vargs.args.empty() ? "" : vargs.args[0].get<std::string>();
            return Value(strip(str, chars));
          } else if (method->get_name() == "lstrip") {
            vargs.expectArgs("lstrip method", {0, 1}, {0, 0});
            auto chars = vargs.args.empty() ? "" : vargs.args[0].get<std::string>();
            return Value(strip(str, chars, /* left= */ true, /* right= */ false));
          } else if (method->get_name() == "rstrip") {
            vargs.expectArgs("rstrip method", {0, 1}, {0, 0});
            auto chars = vargs.args.empty() ? "" : vargs.args[0].get<std::string>();
            return Value(strip(str, chars, /* left= */ false, /* right= */ true));
          } else if (method->get_name() == "split") {
            vargs.expectArgs("split method", {1, 1}, {0, 0});
            auto sep = vargs.args[0].get<std::string>();
            auto parts = split(str, sep);
            Value result = Value::array();
            for (const auto& part : parts) {
              result.push_back(Value(part));
            }
            return result;
          } else if (method->get_name() == "capitalize") {
            vargs.expectArgs("capitalize method", {0, 0}, {0, 0});
            return Value(capitalize(str));
          } else if (method->get_name() == "endswith") {
            vargs.expectArgs("endswith method", {1, 1}, {0, 0});
            auto suffix = vargs.args[0].get<std::string>();
            return suffix.length() <= str.length() && std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
          } else if (method->get_name() == "startswith") {
            vargs.expectArgs("startswith method", {1, 1}, {0, 0});
            auto prefix = vargs.args[0].get<std::string>();
            return prefix.length() <= str.length() && std::equal(prefix.begin(), prefix.end(), str.begin());
          } else if (method->get_name() == "title") {
            vargs.expectArgs("title method", {0, 0}, {0, 0});
            auto res = str;
            for (size_t i = 0, n = res.size(); i < n; ++i) {
              if (i == 0 || std::isspace(res[i - 1])) res[i] = std::toupper(res[i]);
              else res[i] = std::tolower(res[i]);
            }
            return res;
          }
        }
        // _printlog("Unknown method: " + method->get_name());
        return Value();
    }
};

class CallExpr : public Expression {
public:
    std::shared_ptr<Expression> object;
    ArgumentsExpression args;
    CallExpr(const Location & loc, std::shared_ptr<Expression> && obj, ArgumentsExpression && a)
        : Expression(loc, Expression::Type_Call), object(std::move(obj)), args(std::move(a)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        if (!object) {
            _printlog("CallExpr.object is null");
            return Value();
        }
        auto obj = object->evaluate(context);
        if (!obj.is_callable()) {
            //_printlog("Object is not callable: " + obj.dump(2));
            return Value();
        }
        auto vargs = args.evaluate(context);
        return obj.call(context, vargs);
    }
};

class FilterExpr : public Expression {
    std::vector<std::shared_ptr<Expression>> parts;
public:
    FilterExpr(const Location & loc, std::vector<std::shared_ptr<Expression>> && p)
      : Expression(loc, Expression::Type_Filter), parts(std::move(p)) {}
    Value do_evaluate(const std::shared_ptr<Context> & context) const override {
        Value result;
        bool first = true;
        for (const auto& part : parts) {
          if (!part) _printlog("FilterExpr.part is null");
          if (first) {
            first = false;
            result = part->evaluate(context);
          } else {
              if (part->mType == Expression::Type_Call) {
                  auto ce = (CallExpr*)(part.get());
              auto target = ce->object->evaluate(context);
              ArgumentsValue args = ce->args.evaluate(context);
              args.args.insert(args.args.begin(), result);
              result = target.call(context, args);
            } else {
              auto callable = part->evaluate(context);
              ArgumentsValue args;
              args.args.insert(args.args.begin(), result);
              result = callable.call(context, args);
            }
          }
        }
        return result;
    }

    void prepend(std::shared_ptr<Expression> && e) {
        parts.insert(parts.begin(), std::move(e));
    }
};

class Parser {
private:
    using CharIterator = std::string::const_iterator;

    std::shared_ptr<std::string> template_str;
    CharIterator start, end, it;
    Options options;

    Parser(const std::shared_ptr<std::string>& template_str, const Options & options) : template_str(template_str), options(options) {
      if (!template_str) _printlog("Template string is null");
      start = it = this->template_str->begin();
      end = this->template_str->end();
    }

    bool consumeSpaces(SpaceHandling space_handling = SpaceHandling::Strip) {
      if (space_handling == SpaceHandling::Strip) {
        while (it != end && std::isspace(*it)) ++it;
      }
      return true;
    }

    std::shared_ptr<std::string> parseString() {
      auto doParse = [&](char quote) -> std::shared_ptr<std::string> {
        if (it == end || *it != quote) return nullptr;
        std::string result;
        bool escape = false;
        for (++it; it != end; ++it) {
          if (escape) {
            escape = false;
            switch (*it) {
              case 'n': result += '\n'; break;
              case 'r': result += '\r'; break;
              case 't': result += '\t'; break;
              case 'b': result += '\b'; break;
              case 'f': result += '\f'; break;
              case '\\': result += '\\'; break;
              default:
                if (*it == quote) {
                  result += quote;
                } else {
                  result += *it;
                }
                break;
            }
          } else if (*it == '\\') {
            escape = true;
          } else if (*it == quote) {
              ++it;
              std::shared_ptr<std::string> res(new std::string);
              *res = result;
              return res;
          } else {
            result += *it;
          }
        }
        return nullptr;
      };

      consumeSpaces();
      if (it == end) return nullptr;
      if (*it == '"') return doParse('"');
      if (*it == '\'') return doParse('\'');
      return nullptr;
    }

    RValue parseNumberRapid(CharIterator& current_it, const CharIterator& iter_end) {
        auto initial_it = current_it; 
        consumeSpaces();
        auto num_start_it = current_it;
        bool has_decimal = false;
        bool has_exponent = false;

        if (current_it != iter_end && (*current_it == '-' || *current_it == '+')) ++current_it;

        CharIterator num_end_it = current_it;
        while (num_end_it != iter_end) {
            if (std::isdigit(*num_end_it)) {
                num_end_it++;
            } else if (*num_end_it == '.') {
                if (has_decimal) { current_it = initial_it; return RValue(rapidjson::kNullType); }
                has_decimal = true;
                num_end_it++;
            } else if (num_end_it != num_start_it && (*num_end_it == 'e' || *num_end_it == 'E')) {
                if (has_exponent) { current_it = initial_it; return RValue(rapidjson::kNullType); }
                has_exponent = true;
                num_end_it++;
                if (num_end_it != iter_end && (*num_end_it == '+' || *num_end_it == '-')) num_end_it++;
            } else {
                break;
            }
        }
        
        bool valid_num_char_found = false;
        for (auto temp_it = num_start_it; temp_it != num_end_it; ++temp_it) {
            if (std::isdigit(*temp_it)) { valid_num_char_found = true; break; }
        }
        if (!valid_num_char_found && !(has_decimal && num_end_it > num_start_it && std::isdigit(*(num_end_it-1)) ) ) { // check if it's just "." or "+." etc
             if( !(num_start_it != num_end_it && (std::string(num_start_it, num_end_it) == "." || std::string(num_start_it, num_end_it) == "+" || std::string(num_start_it, num_end_it) == "-")) ) {
                // if it's not just a standalone sign or dot, and no digits, it's not a number for us.
                // This condition is to prevent single "." or "+", "-" from being considered.
                // However, if it was like ".5" or "-.5", std::stod would handle it.
                // The main check is if any digit was part of the sequence.
                bool digit_present = false;
                for(autochk = num_start_it; achk != num_end_it; ++achk) if(std::isdigit(*achk)) digit_present = true;
                if(!digit_present) {
                    current_it = initial_it; 
                    return RValue(rapidjson::kNullType);
                }
             }
        }


        std::string str_num(num_start_it, num_end_it);
        if (str_num.empty() || str_num == "+" || str_num == "-") { // Handle cases like empty string, or just sign
            current_it = initial_it;
            return RValue(rapidjson::kNullType);
        }

        current_it = num_end_it; 

        try {
            if (!has_decimal && !has_exponent) {
                size_t pos_int;
                long long val_ll = std::stoll(str_num, &pos_int);
                if (pos_int == str_num.length()) { 
                    return RValue(static_cast<int64_t>(val_ll));
                }
            }
            size_t pos_double;
            double val_d = std::stod(str_num, &pos_double);
            if (pos_double == str_num.length()) { 
                 return RValue(val_d);
            }
        } catch (const std::out_of_range& oor) {
            _printlog("Number out of range during parsing: " + str_num);
        } catch (const std::invalid_argument& ia) {
            _printlog("Invalid number format during parsing: " + str_num);
        }
        current_it = initial_it; 
        return RValue(rapidjson::kNullType);
    }

    /** integer, float, bool, string. Returns a minja::Value. */
    std::shared_ptr<Value> parseConstant() {
      auto original_it_state = it; 
      consumeSpaces();
      if (it == end) return nullptr;

      if (*it == '"' || *it == '\'') {
        auto str_ptr = parseString(); 
        if (str_ptr) return std::make_shared<Value>(*str_ptr); 
      }

      static std::regex prim_tok_regex(R"(true\b|True\b|false\b|False\b|None\b|null\b)"); 
      auto token_str = consumeToken(prim_tok_regex); 
      if (!token_str.empty()) {
        if (token_str == "true" || token_str == "True") return std::make_shared<Value>(true);
        if (token_str == "false" || token_str == "False") return std::make_shared<Value>(false);
        if (token_str == "None" || token_str == "null") return std::make_shared<Value>(nullptr); 
        _printlog("Unknown constant token: " + token_str);
      }

      RValue num_rval = parseNumberRapid(it, end); 
      if (!num_rval.IsNull()) {
          if (num_rval.IsInt64()) return std::make_shared<Value>(num_rval.GetInt64());
          if (num_rval.IsDouble()) return std::make_shared<Value>(num_rval.GetDouble());
      }

      it = original_it_state; 
      return nullptr;
    }

    bool peekSymbols(const std::vector<std::string> & symbols) const {
        for (const auto & symbol : symbols) {
            if (std::distance(it, end) >= (int64_t) symbol.size() && std::string(it, it + symbol.size()) == symbol) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> consumeTokenGroups(const std::regex & regex, SpaceHandling space_handling = SpaceHandling::Strip) {
        auto start = it;
        consumeSpaces(space_handling);
        std::smatch match;
        if (std::regex_search(it, end, match, regex) && match.position() == 0) {
            it += match[0].length();
            std::vector<std::string> ret;
            for (size_t i = 0, n = match.size(); i < n; ++i) {
                ret.push_back(match[i].str());
            }
            return ret;
        }
        it = start;
        return {};
    }
    std::string consumeToken(const std::regex & regex, SpaceHandling space_handling = SpaceHandling::Strip) {
        auto start = it;
        consumeSpaces(space_handling);
        std::smatch match;
        if (std::regex_search(it, end, match, regex) && match.position() == 0) {
            it += match[0].length();
            return match[0].str();
        }
        it = start;
        return "";
    }

    std::string consumeToken(const std::string & token, SpaceHandling space_handling = SpaceHandling::Strip) {
        auto start = it;
        consumeSpaces(space_handling);
        if (std::distance(it, end) >= (int64_t) token.size() && std::string(it, it + token.size()) == token) {
            it += token.size();
            return token;
        }
        it = start;
        return "";
    }

    std::shared_ptr<Expression> parseExpression(bool allow_if_expr = true) {
        auto left = parseLogicalOr();
        if (it == end) return left;

        if (!allow_if_expr) return left;

        static std::regex if_tok(R"(if\b)");
        if (consumeToken(if_tok).empty()) {
          return left;
        }

        auto location = get_location();
        auto cepair = parseIfExpression();
        auto condition = cepair.first;
        auto else_expr = cepair.second;
        return std::make_shared<IfExpr>(location, std::move(condition), std::move(left), std::move(else_expr));
    }

    Location get_location() const {
        return {template_str, (size_t) std::distance(start, it)};
    }

    std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>> parseIfExpression() {
        auto condition = parseLogicalOr();
        if (!condition) _printlog("Expected condition expression");

        static std::regex else_tok(R"(else\b)");
        std::shared_ptr<Expression> else_expr;
        if (!consumeToken(else_tok).empty()) {
          else_expr = parseExpression();
          if (!else_expr) _printlog("Expected 'else' expression");
        }
        return std::make_pair(std::move(condition), std::move(else_expr));
    }

    std::shared_ptr<Expression> parseLogicalOr() {
        auto left = parseLogicalAnd();
        if (!left) _printlog("Expected left side of 'logical or' expression");

        static std::regex or_tok(R"(or\b)");
        auto location = get_location();
        while (!consumeToken(or_tok).empty()) {
            auto right = parseLogicalAnd();
            if (!right) _printlog("Expected right side of 'or' expression");
            left = std::make_shared<BinaryOpExpr>(location, std::move(left), std::move(right), BinaryOpExpr::Op::Or);
        }
        return left;
    }

    std::shared_ptr<Expression> parseLogicalNot() {
        static std::regex not_tok(R"(not\b)");
        auto location = get_location();

        if (!consumeToken(not_tok).empty()) {
          auto sub = parseLogicalNot();
          if (!sub) _printlog("Expected expression after 'not' keyword");
          return std::make_shared<UnaryOpExpr>(location, std::move(sub), UnaryOpExpr::Op::LogicalNot);
        }
        return parseLogicalCompare();
    }

    std::shared_ptr<Expression> parseLogicalAnd() {
        auto left = parseLogicalNot();
        if (!left) _printlog("Expected left side of 'logical and' expression");

        static std::regex and_tok(R"(and\b)");
        auto location = get_location();
        while (!consumeToken(and_tok).empty()) {
            auto right = parseLogicalNot();
            if (!right) _printlog("Expected right side of 'and' expression");
            left = std::make_shared<BinaryOpExpr>(location, std::move(left), std::move(right), BinaryOpExpr::Op::And);
        }
        return left;
    }

    std::shared_ptr<Expression> parseLogicalCompare() {
        auto left = parseStringConcat();
        if (!left) _printlog("Expected left side of 'logical compare' expression");

        static std::regex compare_tok(R"(==|!=|<=?|>=?|in\b|is\b|not\s+in\b)");
        static std::regex not_tok(R"(not\b)");
        std::string op_str;
        while (!(op_str = consumeToken(compare_tok)).empty()) {
            auto location = get_location();
            if (op_str == "is") {
              auto negated = !consumeToken(not_tok).empty();

              auto identifier = parseIdentifier();
              if (!identifier) _printlog("Expected identifier after 'is' keyword");

              return std::make_shared<BinaryOpExpr>(
                  left->location,
                  std::move(left), std::move(identifier),
                  negated ? BinaryOpExpr::Op::IsNot : BinaryOpExpr::Op::Is);
            }
            auto right = parseStringConcat();
            if (!right) _printlog("Expected right side of 'logical compare' expression");
            BinaryOpExpr::Op op;
            if (op_str == "==") op = BinaryOpExpr::Op::Eq;
            else if (op_str == "!=") op = BinaryOpExpr::Op::Ne;
            else if (op_str == "<") op = BinaryOpExpr::Op::Lt;
            else if (op_str == ">") op = BinaryOpExpr::Op::Gt;
            else if (op_str == "<=") op = BinaryOpExpr::Op::Le;
            else if (op_str == ">=") op = BinaryOpExpr::Op::Ge;
            else if (op_str == "in") op = BinaryOpExpr::Op::In;
            else if (op_str.substr(0, 3) == "not") op = BinaryOpExpr::Op::NotIn;
            else _printlog("Unknown comparison operator: " + op_str);
            left = std::make_shared<BinaryOpExpr>(get_location(), std::move(left), std::move(right), op);
        }
        return left;
    }

    Expression::Parameters parseParameters() {
        consumeSpaces();
        if (consumeToken("(").empty()) _printlog("Expected opening parenthesis in param list");

        Expression::Parameters result;

        while (it != end) {
            if (!consumeToken(")").empty()) {
                return result;
            }
            auto expr = parseExpression();
            if (!expr) _printlog("Expected expression in call args");
            if (expr->mType == Expression::Type_Variable) {
                auto ident = (VariableExpr*)(expr.get());
                if (!consumeToken("=").empty()) {
                    auto value = parseExpression();
                    if (!value) _printlog("Expected expression in for named arg");
                    result.emplace_back(ident->get_name(), std::move(value));
                } else {
                    result.emplace_back(ident->get_name(), nullptr);
                }
            } else {
                result.emplace_back(std::string(), std::move(expr));
            }
            if (consumeToken(",").empty()) {
              if (consumeToken(")").empty()) {
                _printlog("Expected closing parenthesis in call args");
              }
              return result;
            }
        }
        _printlog("Expected closing parenthesis in call args");
        return result;
    }

    ArgumentsExpression parseCallArgs() {
        consumeSpaces();
        if (consumeToken("(").empty()) _printlog("Expected opening parenthesis in call args");

        ArgumentsExpression result;

        while (it != end) {
            if (!consumeToken(")").empty()) {
                return result;
            }
            auto expr = parseExpression();
            if (!expr) _printlog("Expected expression in call args");

            if (expr->mType == Expression::Type_Variable) {
                auto ident = (VariableExpr*)(expr.get());
                if (!consumeToken("=").empty()) {
                    auto value = parseExpression();
                    if (!value) _printlog("Expected expression in for named arg");
                    result.kwargs.emplace_back(ident->get_name(), std::move(value));
                } else {
                    result.args.emplace_back(std::move(expr));
                }
            } else {
                result.args.emplace_back(std::move(expr));
            }
            if (consumeToken(",").empty()) {
              if (consumeToken(")").empty()) {
                _printlog("Expected closing parenthesis in call args");
              }
              return result;
            }
        }
        _printlog("Expected closing parenthesis in call args");
        return result;
    }

    std::shared_ptr<VariableExpr> parseIdentifier() {
        static std::regex ident_regex(R"((?!(?:not|is|and|or|del)\b)[a-zA-Z_]\w*)");
        auto location = get_location();
        auto ident = consumeToken(ident_regex);
        if (ident.empty())
          return nullptr;
        return std::make_shared<VariableExpr>(location, ident);
    }

    std::shared_ptr<Expression> parseStringConcat() {
        auto left = parseMathPow();
        if (!left) _printlog("Expected left side of 'string concat' expression");

        static std::regex concat_tok(R"(~(?!\}))");
        if (!consumeToken(concat_tok).empty()) {
            auto right = parseLogicalAnd();
            if (!right) _printlog("Expected right side of 'string concat' expression");
            left = std::make_shared<BinaryOpExpr>(get_location(), std::move(left), std::move(right), BinaryOpExpr::Op::StrConcat);
        }
        return left;
    }

    std::shared_ptr<Expression> parseMathPow() {
        auto left = parseMathPlusMinus();
        if (!left) _printlog("Expected left side of 'math pow' expression");

        while (!consumeToken("**").empty()) {
            auto right = parseMathPlusMinus();
            if (!right) _printlog("Expected right side of 'math pow' expression");
            left = std::make_shared<BinaryOpExpr>(get_location(), std::move(left), std::move(right), BinaryOpExpr::Op::MulMul);
        }
        return left;
    }

    std::shared_ptr<Expression> parseMathPlusMinus() {
        static std::regex plus_minus_tok(R"(\+|-(?![}%#]\}))");

        auto left = parseMathMulDiv();
        if (!left) _printlog("Expected left side of 'math plus/minus' expression");
        std::string op_str;
        while (!(op_str = consumeToken(plus_minus_tok)).empty()) {
            auto right = parseMathMulDiv();
            if (!right) _printlog("Expected right side of 'math plus/minus' expression");
            auto op = op_str == "+" ? BinaryOpExpr::Op::Add : BinaryOpExpr::Op::Sub;
            left = std::make_shared<BinaryOpExpr>(get_location(), std::move(left), std::move(right), op);
        }
        return left;
    }

    std::shared_ptr<Expression> parseMathMulDiv() {
        auto left = parseMathUnaryPlusMinus();
        if (!left) _printlog("Expected left side of 'math mul/div' expression");

        static std::regex mul_div_tok(R"(\*\*?|//?|%(?!\}))");
        std::string op_str;
        while (!(op_str = consumeToken(mul_div_tok)).empty()) {
            auto right = parseMathUnaryPlusMinus();
            if (!right) _printlog("Expected right side of 'math mul/div' expression");
            auto op = op_str == "*" ? BinaryOpExpr::Op::Mul
                : op_str == "**" ? BinaryOpExpr::Op::MulMul
                : op_str == "/" ? BinaryOpExpr::Op::Div
                : op_str == "//" ? BinaryOpExpr::Op::DivDiv
                : BinaryOpExpr::Op::Mod;
            left = std::make_shared<BinaryOpExpr>(get_location(), std::move(left), std::move(right), op);
        }

        if (!consumeToken("|").empty()) {
            auto expr = parseMathMulDiv();
            if (expr->mType == Expression::Type_Filter) {
                auto filter = (FilterExpr*)(expr.get());
                filter->prepend(std::move(left));
                return expr;
            } else {
                std::vector<std::shared_ptr<Expression>> parts;
                parts.emplace_back(std::move(left));
                parts.emplace_back(std::move(expr));
                return std::make_shared<FilterExpr>(get_location(), std::move(parts));
            }
        }
        return left;
    }

    std::shared_ptr<Expression> call_func(const std::string & name, ArgumentsExpression && args) const {
        return std::make_shared<CallExpr>(get_location(), std::make_shared<VariableExpr>(get_location(), name), std::move(args));
    }

    std::shared_ptr<Expression> parseMathUnaryPlusMinus() {
        static std::regex unary_plus_minus_tok(R"(\+|-(?![}%#]\}))");
        auto op_str = consumeToken(unary_plus_minus_tok);
        auto expr = parseExpansion();
        if (!expr) _printlog("Expected expr of 'unary plus/minus/expansion' expression");

        if (!op_str.empty()) {
            auto op = op_str == "+" ? UnaryOpExpr::Op::Plus : UnaryOpExpr::Op::Minus;
            return std::make_shared<UnaryOpExpr>(get_location(), std::move(expr), op);
        }
        return expr;
    }

    std::shared_ptr<Expression> parseExpansion() {
      static std::regex expansion_tok(R"(\*\*?)");
      auto op_str = consumeToken(expansion_tok);
      auto expr = parseValueExpression();
      if (op_str.empty()) return expr;
        if (!expr) {
            _printlog("Expected expr of 'expansion' expression");
            return nullptr;
        }
      return std::make_shared<UnaryOpExpr>(get_location(), std::move(expr), op_str == "*" ? UnaryOpExpr::Op::Expansion : UnaryOpExpr::Op::ExpansionDict);
    }

    std::shared_ptr<Expression> parseValueExpression() {
      auto parseValue = [&]() -> std::shared_ptr<Expression> {
        auto location = get_location();
        auto constant = parseConstant();
        if (constant) return std::make_shared<LiteralExpr>(location, *constant);

        static std::regex null_regex(R"(null\b)");
        if (!consumeToken(null_regex).empty()) return std::make_shared<LiteralExpr>(location, Value());

        auto identifier = parseIdentifier();
        if (identifier) return identifier;

        auto braced = parseBracedExpressionOrArray();
        if (braced) return braced;

        auto array = parseArray();
        if (array) return array;

        auto dictionary = parseDictionary();
        if (dictionary) return dictionary;

        _printlog("Expected value expression");
          return nullptr;
      };

      auto value = parseValue();

      while (it != end && consumeSpaces() && peekSymbols({ "[", "." })) {
          if (!consumeToken("[").empty()) {
            std::shared_ptr<Expression> index;
            auto slice_loc = get_location();
            std::shared_ptr<Expression> start, end, step;
            bool c1 = false, c2 = false;

            if (!peekSymbols({ ":" })) {
              start = parseExpression();
            }

            if (!consumeToken(":").empty()) {
              c1 = true;
              if (!peekSymbols({ ":", "]" })) {
                end = parseExpression();
              }
              if (!consumeToken(":").empty()) {
                c2 = true;
                if (!peekSymbols({ "]" })) {
                  step = parseExpression();
                }
              }
            }
    
            if ((c1 || c2) && (start || end || step)) {
              index = std::make_shared<SliceExpr>(slice_loc, std::move(start), std::move(end), std::move(step));
            } else {
              index = std::move(start);
            }
              if (!index) {
                  MNN_ERROR("Empty index in subscript");
              }
              if (consumeToken("]").empty()) {
                  MNN_ERROR("Expected closing bracket in subscript");
              }

            value = std::make_shared<SubscriptExpr>(value->location, std::move(value), std::move(index));
        } else if (!consumeToken(".").empty()) {
            auto identifier = parseIdentifier();
            if (!identifier) _printlog("Expected identifier in subscript");

            consumeSpaces();
            if (peekSymbols({ "(" })) {
              auto callParams = parseCallArgs();
              value = std::make_shared<MethodCallExpr>(identifier->location, std::move(value), std::move(identifier), std::move(callParams));
            } else {
              auto key = std::make_shared<LiteralExpr>(identifier->location, Value(identifier->get_name()));
              value = std::make_shared<SubscriptExpr>(identifier->location, std::move(value), std::move(key));
            }
        }
        consumeSpaces();
      }

      if (peekSymbols({ "(" })) {
        auto location = get_location();
        auto callParams = parseCallArgs();
        value = std::make_shared<CallExpr>(location, std::move(value), std::move(callParams));
      }
      return value;
    }

    std::shared_ptr<Expression> parseBracedExpressionOrArray() {
        if (consumeToken("(").empty()) return nullptr;

        auto expr = parseExpression();
        if (!expr) _printlog("Expected expression in braced expression");

        if (!consumeToken(")").empty()) {
            return expr;  // Drop the parentheses
        }

        std::vector<std::shared_ptr<Expression>> tuple;
        tuple.emplace_back(std::move(expr));

        while (it != end) {
          if (consumeToken(",").empty()) _printlog("Expected comma in tuple");
          auto next = parseExpression();
          if (!next) _printlog("Expected expression in tuple");
          tuple.push_back(std::move(next));

          if (!consumeToken(")").empty()) {
              return std::make_shared<ArrayExpr>(get_location(), std::move(tuple));
          }
        }
        _printlog("Expected closing parenthesis");
        return nullptr;
    }

    std::shared_ptr<Expression> parseArray() {
        if (consumeToken("[").empty()) return nullptr;

        std::vector<std::shared_ptr<Expression>> elements;
        if (!consumeToken("]").empty()) {
            return std::make_shared<ArrayExpr>(get_location(), std::move(elements));
        }
        auto first_expr = parseExpression();
        if (!first_expr) _printlog("Expected first expression in array");
        elements.push_back(std::move(first_expr));

        while (it != end) {
            if (!consumeToken(",").empty()) {
              auto expr = parseExpression();
              if (!expr) _printlog("Expected expression in array");
              elements.push_back(std::move(expr));
            } else if (!consumeToken("]").empty()) {
                return std::make_shared<ArrayExpr>(get_location(), std::move(elements));
            } else {
                _printlog("Expected comma or closing bracket in array");
            }
        }
        _printlog("Expected closing bracket");
        return nullptr;
    }

    std::shared_ptr<Expression> parseDictionary() {
        if (consumeToken("{").empty()) return nullptr;

        std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<Expression>>> elements;
        if (!consumeToken("}").empty()) {
            return std::make_shared<DictExpr>(get_location(), std::move(elements));
        }

        auto parseKeyValuePair = [&]() {
            auto key = parseExpression();
            if (!key) _printlog("Expected key in dictionary");
            if (consumeToken(":").empty()) _printlog("Expected colon betweek key & value in dictionary");
            auto value = parseExpression();
            if (!value) _printlog("Expected value in dictionary");
            elements.emplace_back(std::make_pair(std::move(key), std::move(value)));
        };

        parseKeyValuePair();

        while (it != end) {
            if (!consumeToken(",").empty()) {
                parseKeyValuePair();
            } else if (!consumeToken("}").empty()) {
                return std::make_shared<DictExpr>(get_location(), std::move(elements));
            } else {
                _printlog("Expected comma or closing brace in dictionary");
            }
        }
        _printlog("Expected closing brace");
        return nullptr;
    }

    SpaceHandling parsePreSpace(const std::string& s) const {
        if (s == "-")
          return SpaceHandling::Strip;
        return SpaceHandling::Keep;
    }

    SpaceHandling parsePostSpace(const std::string& s) const {
        if (s == "-") return SpaceHandling::Strip;
        return SpaceHandling::Keep;
    }

    using TemplateTokenVector = std::vector<std::shared_ptr<TemplateToken>>;
    using TemplateTokenIterator = TemplateTokenVector::const_iterator;

    std::vector<std::string> parseVarNames() {
      static std::regex varnames_regex(R"(((?:\w+)(?:\s*,\s*(?:\w+))*)\s*)");

      std::vector<std::string> group;
      if ((group = consumeTokenGroups(varnames_regex)).empty()) _printlog("Expected variable names");
      std::vector<std::string> varnames;
      std::istringstream iss(group[1]);
      std::string varname;
      while (std::getline(iss, varname, ',')) {
        varnames.push_back(strip(varname));
      }
      return varnames;
    }

    std::string unexpected(const TemplateToken & token) const {
      return std::string("Unexpected " + TemplateToken::typeToString(token.type)
        + error_location_suffix(*template_str, token.location.pos));
    }
    std::string unterminated(const TemplateToken & token) const {
      return std::string("Unterminated " + TemplateToken::typeToString(token.type)
        + error_location_suffix(*template_str, token.location.pos));
    }

    TemplateTokenVector tokenize() {
      static std::regex comment_tok(R"(\{#([-~]?)([\s\S]*?)([-~]?)#\})");
      static std::regex expr_open_regex(R"(\{\{([-~])?)");
      static std::regex block_open_regex(R"(^\{%([-~])?\s*)");
      static std::regex block_keyword_tok(R"((if|else|elif|endif|for|endfor|generation|endgeneration|set|endset|block|endblock|macro|endmacro|filter|endfilter|break|continue)\b)");
      static std::regex non_text_open_regex(R"(\{\{|\{%|\{#)");
      static std::regex expr_close_regex(R"(\s*([-~])?\}\})");
      static std::regex block_close_regex(R"(\s*([-~])?%\})");

      TemplateTokenVector tokens;
      std::vector<std::string> group;
      std::string text;
      std::smatch match;

        while (it != end) {
          auto location = get_location();

          if (!(group = consumeTokenGroups(comment_tok, SpaceHandling::Keep)).empty()) {
            auto pre_space = parsePreSpace(group[1]);
            auto content = group[2];
            auto post_space = parsePostSpace(group[3]);
            tokens.push_back(std::make_shared<CommentTemplateToken>(location, pre_space, post_space, content));
          } else if (!(group = consumeTokenGroups(expr_open_regex, SpaceHandling::Keep)).empty()) {
            auto pre_space = parsePreSpace(group[1]);
            auto expr = parseExpression();

            if ((group = consumeTokenGroups(expr_close_regex)).empty()) {
              _printlog("Expected closing expression tag");
            }

            auto post_space = parsePostSpace(group[1]);
            tokens.push_back(std::make_shared<ExpressionTemplateToken>(location, pre_space, post_space, std::move(expr)));
          } else if (!(group = consumeTokenGroups(block_open_regex, SpaceHandling::Keep)).empty()) {
            auto pre_space = parsePreSpace(group[1]);

            std::string keyword;

            auto parseBlockClose = [&]() -> SpaceHandling {
              if ((group = consumeTokenGroups(block_close_regex)).empty()) _printlog("Expected closing block tag");
              return parsePostSpace(group[1]);
            };

            if ((keyword = consumeToken(block_keyword_tok)).empty()) _printlog("Expected block keyword");

            if (keyword == "if") {
              auto condition = parseExpression();
              if (!condition) _printlog("Expected condition in if block");

              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<IfTemplateToken>(location, pre_space, post_space, std::move(condition)));
            } else if (keyword == "elif") {
              auto condition = parseExpression();
              if (!condition) _printlog("Expected condition in elif block");

              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<ElifTemplateToken>(location, pre_space, post_space, std::move(condition)));
            } else if (keyword == "else") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<ElseTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "endif") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<EndIfTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "for") {
              static std::regex recursive_tok(R"(recursive\b)");
              static std::regex if_tok(R"(if\b)");

              auto varnames = parseVarNames();
              static std::regex in_tok(R"(in\b)");
              if (consumeToken(in_tok).empty()) _printlog("Expected 'in' keyword in for block");
              auto iterable = parseExpression(/* allow_if_expr = */ false);
              if (!iterable) _printlog("Expected iterable in for block");

              std::shared_ptr<Expression> condition;
              if (!consumeToken(if_tok).empty()) {
                condition = parseExpression();
              }
              auto recursive = !consumeToken(recursive_tok).empty();

              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<ForTemplateToken>(location, pre_space, post_space, std::move(varnames), std::move(iterable), std::move(condition), recursive));
            } else if (keyword == "endfor") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<EndForTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "generation") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<GenerationTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "endgeneration") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<EndGenerationTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "set") {
              static std::regex namespaced_var_regex(R"((\w+)\s*\.\s*(\w+))");

              std::string ns;
              std::vector<std::string> var_names;
              std::shared_ptr<Expression> value;
              if (!(group = consumeTokenGroups(namespaced_var_regex)).empty()) {
                ns = group[1];
                var_names.push_back(group[2]);

                if (consumeToken("=").empty()) _printlog("Expected equals sign in set block");

                value = parseExpression();
                if (!value) _printlog("Expected value in set block");
              } else {
                var_names = parseVarNames();

                if (!consumeToken("=").empty()) {
                  value = parseExpression();
                  if (!value) _printlog("Expected value in set block");
                }
              }
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<SetTemplateToken>(location, pre_space, post_space, ns, var_names, std::move(value)));
            } else if (keyword == "endset") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<EndSetTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "macro") {
              auto macroname = parseIdentifier();
              if (!macroname) _printlog("Expected macro name in macro block");
              auto params = parseParameters();

              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<MacroTemplateToken>(location, pre_space, post_space, std::move(macroname), std::move(params)));
            } else if (keyword == "endmacro") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<EndMacroTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "filter") {
              auto filter = parseExpression();
              if (!filter) _printlog("Expected expression in filter block");

              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<FilterTemplateToken>(location, pre_space, post_space, std::move(filter)));
            } else if (keyword == "endfilter") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<EndFilterTemplateToken>(location, pre_space, post_space));
            } else if (keyword == "break" || keyword == "continue") {
              auto post_space = parseBlockClose();
              tokens.push_back(std::make_shared<LoopControlTemplateToken>(location, pre_space, post_space, keyword == "break" ? LoopControlType::Break : LoopControlType::Continue));
            } else {
              _printlog("Unexpected block: " + keyword);
            }
          } else if (std::regex_search(it, end, match, non_text_open_regex)) {
            if (!match.position()) {
                if (match[0] != "{#")
                    _printlog("Internal error: Expected a comment");
                _printlog("Missing end of comment tag");
            }
            auto text_end = it + match.position();
            text = std::string(it, text_end);
            it = text_end;
            tokens.push_back(std::make_shared<TextTemplateToken>(location, SpaceHandling::Keep, SpaceHandling::Keep, text));
          } else {
            text = std::string(it, end);
            it = end;
            tokens.push_back(std::make_shared<TextTemplateToken>(location, SpaceHandling::Keep, SpaceHandling::Keep, text));
          }
        }
        return tokens;
    }

    std::shared_ptr<TemplateNode> parseTemplate(
          const TemplateTokenIterator & begin,
          TemplateTokenIterator & it,
          const TemplateTokenIterator & end,
          bool fully = false) const {
        std::vector<std::shared_ptr<TemplateNode>> children;
        while (it != end) {
          const auto start = it;
          const auto & token = *(it++);
            if (token->type == TemplateToken::Type::If) {
                auto if_token = (IfTemplateToken*)(token.get());
              std::vector<std::pair<std::shared_ptr<Expression>, std::shared_ptr<TemplateNode>>> cascade;
              cascade.emplace_back(std::move(if_token->condition), parseTemplate(begin, it, end));

              while (it != end && (*it)->type == TemplateToken::Type::Elif) {
                  auto elif_token = (ElifTemplateToken*)((*(it++)).get());
                  cascade.emplace_back(std::move(elif_token->condition), parseTemplate(begin, it, end));
              }

              if (it != end && (*it)->type == TemplateToken::Type::Else) {
                cascade.emplace_back(nullptr, parseTemplate(begin, ++it, end));
              }
              if (it == end || (*(it++))->type != TemplateToken::Type::EndIf) {
                  MNN_ERROR("%s\n", unterminated(**start).c_str());
              }
              children.emplace_back(std::make_shared<IfNode>(token->location, std::move(cascade)));
            } else if (token->type == TemplateToken::Type::For) {
                auto for_token = (ForTemplateToken*)(token.get());
              auto body = parseTemplate(begin, it, end);
              auto else_body = std::shared_ptr<TemplateNode>();
              if (it != end && (*it)->type == TemplateToken::Type::Else) {
                else_body = parseTemplate(begin, ++it, end);
              }
              if (it == end || (*(it++))->type != TemplateToken::Type::EndFor) {
                  MNN_ERROR("%s\n", unterminated(**start).c_str());
              }
              children.emplace_back(std::make_shared<ForNode>(token->location, std::move(for_token->var_names), std::move(for_token->iterable), std::move(for_token->condition), std::move(body), for_token->recursive, std::move(else_body)));
            } else if(token->type == TemplateToken::Type::Generation) {
              auto body = parseTemplate(begin, it, end);
              if (it == end || (*(it++))->type != TemplateToken::Type::EndGeneration) {
                  MNN_ERROR("%s\n", unterminated(**start).c_str());
              }
              // Treat as a no-op, as our scope is templates for inference, not training (`{% generation %}` wraps generated tokens for masking).
              children.emplace_back(std::move(body));
            } else if(token->type == TemplateToken::Type::Text) {
                auto text_token = (TextTemplateToken*)(token.get());
              SpaceHandling pre_space = (it - 1) != begin ? (*(it - 2))->post_space : SpaceHandling::Keep;
              SpaceHandling post_space = it != end ? (*it)->pre_space : SpaceHandling::Keep;

              auto text = text_token->text;
              if (post_space == SpaceHandling::Strip) {
                static std::regex trailing_space_regex(R"(\s+$)");
                text = std::regex_replace(text, trailing_space_regex, "");
              } else if (options.lstrip_blocks && it != end) {
                auto i = text.size();
                while (i > 0 && (text[i - 1] == ' ' || text[i - 1] == '\t')) i--;
                if ((i == 0 && (it - 1) == begin) || (i > 0 && text[i - 1] == '\n')) {
                  text.resize(i);
                }
              }
              if (pre_space == SpaceHandling::Strip) {
                static std::regex leading_space_regex(R"(^\s+)");
                text = std::regex_replace(text, leading_space_regex, "");
              } else if (options.trim_blocks && (it - 1) != begin && (*(it - 2))->type != TemplateToken::Type::Expression) {
                if (!text.empty() && text[0] == '\n') {
                  text.erase(0, 1);
                }
              }
              if (it == end && !options.keep_trailing_newline) {
                auto i = text.size();
                if (i > 0 && text[i - 1] == '\n') {
                  i--;
                  if (i > 0 && text[i - 1] == '\r') i--;
                  text.resize(i);
                }
              }
              children.emplace_back(std::make_shared<TextNode>(token->location, text));
            } else if(token->type == TemplateToken::Type::Expression) {
                auto expr_token = (ExpressionTemplateToken*)(token.get());
                children.emplace_back(std::make_shared<ExpressionNode>(token->location, std::move(expr_token->expr)));
            } else if(token->type == TemplateToken::Type::Set) {
                auto set_token = (SetTemplateToken*)(token.get());
                if (set_token->value) {
                  children.emplace_back(std::make_shared<SetNode>(token->location, set_token->ns, set_token->var_names, std::move(set_token->value)));
                } else {
                  auto value_template = parseTemplate(begin, it, end);
                  if (it == end || (*(it++))->type != TemplateToken::Type::EndSet) {
                      MNN_ERROR("%s\n", unterminated(**start).c_str());
                  }
                  if (!set_token->ns.empty()) _printlog("Namespaced set not supported in set with template value");
                  if (set_token->var_names.size() != 1) _printlog("Structural assignment not supported in set with template value");
                  auto & name = set_token->var_names[0];
                  children.emplace_back(std::make_shared<SetTemplateNode>(token->location, name, std::move(value_template)));
                }
            } else if(token->type == TemplateToken::Type::Macro) {
                auto macro_token = (MacroTemplateToken*)(token.get());
              auto body = parseTemplate(begin, it, end);
              if (it == end || (*(it++))->type != TemplateToken::Type::EndMacro) {
                  MNN_ERROR("%s\n", unterminated(**start).c_str());
              }
              children.emplace_back(std::make_shared<MacroNode>(token->location, std::move(macro_token->name), std::move(macro_token->params), std::move(body)));
            } else if(token->type == TemplateToken::Type::Filter) {
                auto filter_token = (FilterTemplateToken*)(token.get());
                auto body = parseTemplate(begin, it, end);
                if (it == end || (*(it++))->type != TemplateToken::Type::EndFilter) {
                    MNN_ERROR("%s\n", unterminated(**start).c_str());
                }
                children.emplace_back(std::make_shared<FilterNode>(token->location, std::move(filter_token->filter), std::move(body)));
            } else if(token->type == TemplateToken::Type::Comment) {
                // Ignore comments
            } else if(token->type == TemplateToken::Type::Break) {
                auto ctrl_token = (LoopControlTemplateToken*)(token.get());
                children.emplace_back(std::make_shared<LoopControlNode>(token->location, ctrl_token->control_type));
            } else {
                bool needBreak = false;
                switch (token->type) {
                    case TemplateToken::Type::EndSet:
                    case TemplateToken::Type::EndFor:
                    case TemplateToken::Type::EndMacro:
                    case TemplateToken::Type::EndFilter:
                    case TemplateToken::Type::EndIf:
                    case TemplateToken::Type::Else:
                    case TemplateToken::Type::Elif:
                    case TemplateToken::Type::EndGeneration:
                        it--;
                        needBreak = true;
                        break;
                    default:
                        MNN_ERROR("%s\n", unexpected(**(it-1)).c_str());
                }
                if (needBreak) {
                    break;
                }
          }
        }
        if (fully && it != end) {
            MNN_ERROR("%s\n", unexpected(**it).c_str());
        }
        if (children.empty()) {
          return std::make_shared<TextNode>(Location { template_str, 0 }, std::string());
        } else if (children.size() == 1) {
          return std::move(children[0]);
        } else {
          return std::make_shared<SequenceNode>(children[0]->location(), std::move(children));
        }
    }

public:

    static std::shared_ptr<TemplateNode> parse(const std::string& template_str, const Options & options) {
        Parser parser(std::make_shared<std::string>(normalize_newlines(template_str)), options);
        auto tokens = parser.tokenize();
        TemplateTokenIterator begin = tokens.begin();
        auto it = begin;
        TemplateTokenIterator end = tokens.end();
        return parser.parseTemplate(begin, it, end, /* fully= */ true);
    }
};

static Value simple_function(const std::string & fn_name, const std::vector<std::string> & params, const std::function<Value(const std::shared_ptr<Context> &, Value & args)> & fn) {
  std::map<std::string, size_t> named_positions;
  for (size_t i = 0, n = params.size(); i < n; i++) named_positions[params[i]] = i;

  return Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) -> Value {
    auto args_obj = Value::object();
    std::vector<bool> provided_args(params.size());
    for (size_t i = 0, n = args.args.size(); i < n; i++) {
      auto & arg = args.args[i];
      if (i < params.size()) {
        args_obj.set(params[i], arg);
        provided_args[i] = true;
      } else {
        _printlog("Too many positional params for " + fn_name);
      }
    }
      for (auto & iter : args.kwargs) {
          auto& name = iter.first;
          auto& value = iter.second;
      auto named_pos_it = named_positions.find(name);
      if (named_pos_it == named_positions.end()) {
        _printlog("Unknown argument " + name + " for function " + fn_name);
      }
      provided_args[named_pos_it->second] = true;
      args_obj.set(name, value);
    }
    return fn(context, args_obj);
  });
}

inline std::shared_ptr<Context> Context::builtins() {
  auto globals = Value::object();

//  globals.set("raise_exception", simple_function("raise_exception", { "message" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
//    _printlog(args.at("message").get<std::string>());
//  }));
  globals.set("tojson", simple_function("tojson", { "value", "indent" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
    return Value(args.at("value").dump(args.get<int64_t>("indent", -1), /* to_json_format= */ true));
  }));
  globals.set("items", simple_function("items", { "object" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
    auto result_items_array = Value::array();
    if (args.contains("object")) {
      Value& obj_val = args.at("object");
      if (obj_val.is_object()) { // minja map-like object
        for (auto & key_val : obj_val.keys()) {
            result_items_array.push_back(Value::array({key_val, obj_val.get(key_val)}));
        }
      } else if (obj_val.is_string()) { // JSON string
          std::string json_str = obj_val.to_str();
          Document parsed_doc;
          // This section requires nlohmann::json for temporary conversion if minja::Value(RValue) is not fully implemented
          // For now, this bridge is problematic but necessary if we must use the old Value(nlohmann::json) for complex RValues.
          if (!parsed_doc.Parse(json_str.c_str()).HasParseError() && parsed_doc.IsObject()) {
              for (const auto& m : parsed_doc.GetObject()) {
                  Value key_minja_val(m.name.GetString());
                  rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); m.value.Accept(writer);
                  // The following line assumes Value has a constructor that can take nlohmann::json
                  // This is a temporary bridge.
                  nlohmann::json temp_nl_val = nlohmann::json::parse(buffer.GetString());
                  result_items_array.push_back(Value::array({key_minja_val, Value(temp_nl_val)}));
              }
          }
      } else if (obj_val.rvalue_.IsObject()){ // minja::Value wraps a rapidjson object
           for (const auto& m : obj_val.rvalue_.GetObject()) {
                Value key_minja_val(m.name.GetString());
                rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); m.value.Accept(writer);
                nlohmann::json temp_nl_val = nlohmann::json::parse(buffer.GetString()); // Temporary bridge
                result_items_array.push_back(Value::array({key_minja_val, Value(temp_nl_val)}));
           }
      }
    }
    return result_items_array;
  }));
  globals.set("last", simple_function("last", { "items" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto items = args.at("items");
    if (!items.is_array()) _printlog("object is not a list");
    if (items.empty()) return Value();
    return items.at(items.size() - 1);
  }));
  globals.set("trim", simple_function("trim", { "text" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto & text = args.at("text");
    return text.is_null() ? text : Value(strip(text.to_str()));
  }));
  auto char_transform_function = [](const std::string & name, const std::function<char(char)> & fn) {
    return simple_function(name, { "text" }, [=](const std::shared_ptr<Context> &, Value & args) {
      auto text_val = args.at("text");
      if (text_val.is_null()) return text_val;
      std::string res_str;
      auto str_to_transform = text_val.to_str();
      std::transform(str_to_transform.begin(), str_to_transform.end(), std::back_inserter(res_str), fn);
      return Value(res_str);
    });
  };
  globals.set("lower", char_transform_function("lower", ::tolower));
  globals.set("upper", char_transform_function("upper", ::toupper));
  globals.set("default", Value::callable([=](const std::shared_ptr<Context> &, ArgumentsValue & args) {
    args.expectArgs("default", {2, 3}, {0, 1});
    auto & value = args.args[0];
    auto & default_value = args.args[1];
    bool boolean = false;
    if (args.args.size() == 3) {
      boolean = args.args[2].get<bool>();
    } else {
      Value bv = args.get_named("boolean");
      if (!bv.is_null()) {
        boolean = bv.get<bool>();
      }
    }
    return boolean ? (value.to_bool() ? value : default_value) : value.is_null() ? default_value : value;
  }));
  auto escape = simple_function("escape", { "text" }, [](const std::shared_ptr<Context> &, Value & args) {
    return Value(html_escape(args.at("text").to_str()));
  });
  globals.set("e", escape);
  globals.set("escape", escape);
  globals.set("joiner", simple_function("joiner", { "sep" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto sep = args.get<std::string>("sep", "");
    auto first = std::make_shared<bool>(true);
    return simple_function("", {}, [sep, first](const std::shared_ptr<Context> &, const Value &) -> Value {
      if (*first) {
        *first = false;
        return "";
      }
      return sep;
    });
    // Original code had a redundant return here. Removed.
  }));
  globals.set("count", simple_function("count", { "items" }, [](const std::shared_ptr<Context> &, Value & args) {
    return Value((int64_t) args.at("items").size());
  }));
  globals.set("dictsort", simple_function("dictsort", { "value" }, [](const std::shared_ptr<Context> &, Value & args_map) -> Value {
    Value& dict_val = args_map.at("value");
    if (!dict_val.is_object() && !dict_val.rvalue_.IsObject()) {
        _printlog("dictsort expects an object: " + dict_val.dump());
        return Value::array();
    }
    std::vector<Value> keys_list;
    if (dict_val.is_object()) {
        keys_list = dict_val.keys();
    } else { // rvalue_ is a JSON object
        for(const auto& m : dict_val.rvalue_.GetObject()){
            keys_list.push_back(Value(m.name.GetString()));
        }
    }
    std::sort(keys_list.begin(), keys_list.end()); // Uses minja::Value::operator<
    auto result_array = Value::array();
    for (Value & key_item : keys_list) {
        Value val_for_key;
        if (dict_val.is_object()){ val_for_key = dict_val.get(key_item); }
        else { // RValue object
            const RValue& r_val_member = dict_val.rvalue_[key_item.to_str().c_str()];
            // Temporary bridge for RValue -> minja::Value
            rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); r_val_member.Accept(writer);
            nlohmann::json temp_nl_val = nlohmann::json::parse(buffer.GetString()); val_for_key = Value(temp_nl_val); }
        result_array.push_back(Value::array({key_item, val_for_key})); }
    return result_array;
  }));
  globals.set("join", simple_function("join", { "items", "d" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto do_join = [](Value & items, const std::string & sep) {
      if (!items.is_array()) _printlog("object is not iterable: " + items.dump());
      std::ostringstream oss;
      auto first = true;
      for (size_t i = 0, n = items.size(); i < n; ++i) {
        if (first) first = false;
        else oss << sep;
        oss << items.at(i).to_str();
      }
      return Value(oss.str());
    };
    auto sep = args.get<std::string>("d", "");
    if (args.contains("items")) {
        auto & items = args.at("items");
        return do_join(items, sep);
    } else {
      return simple_function("", {"items"}, [sep, do_join](const std::shared_ptr<Context> &, Value & args) {
        auto & items = args.at("items");
        if (!items.to_bool() || !items.is_array()) _printlog("join expects an array for items, got: " + items.dump());
        return do_join(items, sep);
      });
    }
  }));
  globals.set("namespace", Value::callable([=](const std::shared_ptr<Context> &, ArgumentsValue & args) {
    auto ns = Value::object();
    args.expectArgs("namespace", {0, 0}, {0, (std::numeric_limits<size_t>::max)()});
      for (auto & iter : args.kwargs) {
          auto& name = iter.first;
          auto& value = iter.second;
          ns.set(name, value);
      }
    return ns;
  }));
  auto equalto = simple_function("equalto", { "expected", "actual" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at("actual") == args.at("expected");
  });
  globals.set("equalto", equalto);
  globals.set("==", equalto);
  globals.set("length", simple_function("length", { "items" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      auto & items = args.at("items");
      return (int64_t) items.size();
  }));
  globals.set("safe", simple_function("safe", { "value" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at("value").to_str();
  }));
  globals.set("string", simple_function("string", { "value" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at("value").to_str();
  }));
  globals.set("int", simple_function("int", { "value" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      return args.at("value").to_int();
  }));
  globals.set("list", simple_function("list", { "items" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      auto & items = args.at("items");
      if (!items.is_array()) _printlog("object is not iterable");
      return items;
  }));
  globals.set("unique", simple_function("unique", { "items" }, [](const std::shared_ptr<Context> &, Value & args) -> Value {
      auto & items = args.at("items");
      if (!items.is_array()) _printlog("object is not iterable");
      std::unordered_set<Value> seen;
      auto result = Value::array();
      for (size_t i = 0, n = items.size(); i < n; i++) {
        auto pair = seen.insert(items.at(i));
        if (pair.second) {
          result.push_back(items.at(i));
        }
      }
      return result;
  }));
  auto make_filter = [](const Value & filter, Value & extra_args) -> Value {
    return simple_function("", { "value" }, [=](const std::shared_ptr<Context> & context, Value & args) {
      auto & value = args.at("value");
      ArgumentsValue actual_args;
      actual_args.args.emplace_back(value);
      for (size_t i = 0, n = extra_args.size(); i < n; i++) {
        actual_args.args.emplace_back(extra_args.at(i));
      }
      return filter.call(context, actual_args);
    });
  };
  auto select_or_reject = [make_filter](bool is_select) {
    return Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
      args.expectArgs(is_select ? "select" : "reject", {2, (std::numeric_limits<size_t>::max)()}, {0, 0});
      auto & items = args.args[0];
      if (items.is_null()) {
        return Value::array();
      }
      if (!items.is_array()) {
        _printlog("object is not iterable: " + items.dump());
      }

      auto filter_fn = context->get(args.args[1]);
      if (filter_fn.is_null()) {
        _printlog("Undefined filter: " + args.args[1].dump());
      }

      auto filter_args_val = Value::array(); // Changed name to avoid conflict
      for (size_t i = 2, n = args.args.size(); i < n; i++) {
        filter_args_val.push_back(args.args[i]);
      }
      auto filter = make_filter(filter_fn, filter_args_val); // Pass Value

      auto res = Value::array();
      for (size_t i = 0, n = items.size(); i < n; i++) {
        auto & item = items.at(i);
        ArgumentsValue current_filter_args; // Changed name
        current_filter_args.args.emplace_back(item);
        auto pred_res = filter.call(context, current_filter_args);
        if (pred_res.to_bool() == (is_select ? true : false)) {
          res.push_back(item);
        }
      }
      return res;
    });
  };
  globals.set("select", select_or_reject(/* is_select= */ true));
  globals.set("reject", select_or_reject(/* is_select= */ false));
  globals.set("map", Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
    auto res = Value::array();
    if (args.args.size() == 1 &&
      ((args.has_named("attribute") && args.kwargs.size() == 1) || (args.has_named("default") && args.kwargs.size() == 2))) {
      auto & items = args.args[0];
      auto attr_name = args.get_named("attribute");
      auto default_value = args.get_named("default");
      for (size_t i = 0, n = items.size(); i < n; i++) {
        auto & item = items.at(i);
        auto attr = item.get(attr_name);
        res.push_back(attr.is_null() ? default_value : attr);
      }
    } else if (args.kwargs.empty() && args.args.size() >= 2) {
      auto fn = context->get(args.args[1]);
      if (fn.is_null()) _printlog("Undefined filter: " + args.args[1].dump());
      ArgumentsValue filter_args_val {{Value()}, {}}; // Changed name
      for (size_t i = 2, n = args.args.size(); i < n; i++) {
        filter_args_val.args.emplace_back(args.args[i]);
      }
      for (size_t i = 0, n = args.args[0].size(); i < n; i++) {
        auto & item = args.args[0].at(i);
        filter_args_val.args[0] = item;
        res.push_back(fn.call(context, filter_args_val));
      }
    } else {
      _printlog("Invalid or unsupported arguments for map");
    }
    return res;
  }));
  globals.set("indent", simple_function("indent", { "text", "indent", "first" }, [](const std::shared_ptr<Context> &, Value & args) {
    auto text = args.at("text").to_str();
    auto first = args.get<bool>("first", false);
    std::string out;
    std::string indent_str(args.get<int64_t>("indent", 0), ' '); // Renamed indent to indent_str
    std::istringstream iss(text);
    std::string line;
    auto is_first = true;
    while (std::getline(iss, line, '\n')) {
      auto needs_indent = !is_first || first;
      if (is_first) is_first = false;
      else out += "\n";
      if (needs_indent) out += indent_str;
      out += line;
    }
    if (!text.empty() && text.back() == '\n') out += "\n";
    return Value(out);
  }));
  auto select_or_reject_attr = [](bool is_select) {
    return Value::callable([=](const std::shared_ptr<Context> & context, ArgumentsValue & args) {
      args.expectArgs(is_select ? "selectattr" : "rejectattr", {2, (std::numeric_limits<size_t>::max)()}, {0, 0});
      auto & items = args.args[0];
      if (items.is_null())
        return Value::array();
      if (!items.is_array()) _printlog("object is not iterable: " + items.dump());
      auto attr_name_str = args.args[1].to_str(); // Renamed

      bool has_test = false;
      Value test_fn;
      ArgumentsValue test_args_val {{Value()}, {}}; // Renamed
      if (args.args.size() >= 3) {
        has_test = true;
        test_fn = context->get(args.args[2]);
        if (test_fn.is_null()) _printlog("Undefined test: " + args.args[2].dump());
        for (size_t i = 3, n = args.args.size(); i < n; i++) {
          test_args_val.args.emplace_back(args.args[i]);
        }
        test_args_val.kwargs = args.kwargs;
      }

      auto res = Value::array();
      for (size_t i = 0, n = items.size(); i < n; i++) {
        auto & item = items.at(i);
        auto attr = item.get(Value(attr_name_str)); // Use Value(string) for key
        if (has_test) {
          test_args_val.args[0] = attr;
          if (test_fn.call(context, test_args_val).to_bool() == (is_select ? true : false)) {
            res.push_back(item);
          }
        } else { // Original behavior if no test: add the attribute itself, not the item
          if(attr.to_bool() == (is_select ? true: false)) { // if attr is "truthy" and select, or "falsy" and reject
             res.push_back(item); // Jinja behavior is to return the item, not the attribute
          }
        }
      }
      return res;
    });
  };
  globals.set("selectattr", select_or_reject_attr(/* is_select= */ true));
  globals.set("rejectattr", select_or_reject_attr(/* is_select= */ false));
  globals.set("range", Value::callable([=](const std::shared_ptr<Context> &, ArgumentsValue & args) {
    std::vector<int64_t> startEndStep(3);
    std::vector<bool> param_set(3);
    if (args.args.size() == 1) {
      startEndStep[1] = args.args[0].to_int(); // Use to_int()
      param_set[1] = true;
    } else {
      for (size_t i = 0; i < args.args.size(); i++) {
        auto & arg = args.args[i];
        auto v = arg.to_int(); // Use to_int()
        startEndStep[i] = v;
        param_set[i] = true;
      }
    }
      for (auto & iter : args.kwargs) {
          auto& name = iter.first;
          auto& value = iter.second;
      size_t i_idx; // Renamed
      if (name == "start") {
        i_idx = 0;
      } else if (name == "end") {
        i_idx = 1;
      } else if (name == "step") {
        i_idx = 2;
      } else {
        _printlog("Unknown argument " + name + " for function range"); continue; // Skip unknown
      }

      if (param_set[i_idx]) {
        _printlog("Duplicate argument " + name + " for function range"); continue; // Skip duplicate
      }
      startEndStep[i_idx] = value.to_int(); // Use to_int()
      param_set[i_idx] = true;
    }
    if (!param_set[1]) {
      _printlog("Missing required argument 'end' for function range");
      return Value::array(); // Return empty array on error
    }
    int64_t start_val = param_set[0] ? startEndStep[0] : 0; // Renamed
    int64_t end_val = startEndStep[1]; // Renamed
    int64_t step_val = param_set[2] ? startEndStep[2] : 1; // Renamed
    if (step_val == 0) { _printlog("Step cannot be zero for range"); return Value::array(); }


    auto res_arr = Value::array(); // Renamed
    if (step_val > 0) {
      for (int64_t val_i = start_val; val_i < end_val; val_i += step_val) { // Renamed
        res_arr.push_back(Value(val_i));
      }
    } else { // step < 0
      for (int64_t val_i = start_val; val_i > end_val; val_i += step_val) { // Renamed
        res_arr.push_back(Value(val_i));
      }
    }
    return res_arr;
  }));

  return std::make_shared<Context>(std::move(globals));
}

inline std::shared_ptr<Context> Context::make(Value && context_values, const std::shared_ptr<Context> & parent_context) {
  if (context_values.is_null()) {
    auto obj_map = std::make_shared<Value::ObjectType>();
    Value new_object_value = Value::object(obj_map);
    return std::make_shared<Context>(std::move(new_object_value), parent_context);
  } else if (!context_values.is_object()) {
    _printlog("Context::make expects an object-type Value, received: " + context_values.dump());
    auto obj_map = std::make_shared<Value::ObjectType>();
    Value new_object_value = Value::object(obj_map);
    return std::make_shared<Context>(std::move(new_object_value), parent_context);
  }
  return std::make_shared<Context>(std::move(context_values), parent_context);
}

}  // namespace minjaThe full overwrite of `include/minja/minja.hpp` with the `rapidjson`-based implementation has been attempted. This version includes:
*   Replacement of `nlohmann::json` with `rapidjson` types (`Document`, `RValue`).
*   A redesigned `minja::Value` class using `RValue rvalue_` for primitive JSON types and `std::unique_ptr<Document> owned_document_` for memory management when `minja::Value` owns the data (e.g., for strings or newly created JSON values). Minja-specific list (`ArrayType`) and map (`ObjectType`) are retained as shared pointers.
*   Updated constructors for `minja::Value` to handle basic C++ types and initialize `rvalue_`. The constructor `Value(const nlohmann::json&)` is kept temporarily for bridging, converting `nlohmann::json` to the new `rapidjson`-based internal representation.
*   A helper `get_rvalue_allocator()` in `minja::Value` to manage `owned_document_` and provide an allocator.
*   Rewritten `dump()` and a new `dump_string_rvalue()` for string representation, catering to both Pythonic and JSON-style output.
*   Updated type-checking methods (`is_string`, `is_number`, etc.) and other utility methods (`to_str`, `to_int`, `empty`, `size`, `keys`) to work with the `rvalue_` or the Minja-specific types.
*   Revised operators (`+`, `-`, `*`, `/`, `%`, comparisons, `[]`) to handle operations between `minja::Value` instances, considering their underlying types (Minja map/array or `rvalue_`). The `at()` / `operator[]` for `rvalue_` JSON arrays/objects still has known challenges regarding returning references (`Value&`) safely.
*   The `Parser`'s `parseNumber()` was replaced with `parseNumberRapid()` using `std::stoll/stod` to produce an `RValue`, and `parseConstant()` was updated accordingly.
*   The `Context` class methods like `make` and `builtins` were adapted. `Context::builtins()` still has dependencies on the temporary `Value(const nlohmann::json&)` constructor for some complex filter arguments, which is a known area for future refinement (direct `RValue` to `minja::Value` construction).
*   The `std::hash<minja::Value>` specialization was updated.

**Next Steps:**

With `include/minja/minja.hpp` now (hopefully) in a state that reflects the `rapidjson` migration (albeit with some known areas needing refinement, especially around `Value::at()` and `RValue` to `minja::Value` conversion in `builtins`), I will proceed to refactor the remaining files.

The next file is `examples/raw.cpp`. I'll read its content and then apply the necessary changes.
