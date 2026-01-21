// Copyright 2024 The Trustees of Indiana University.
//
// Unit tests for type_info_map

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/type_info_map.hpp>
#include <string>

using amplusplus::detail::type_info_map;
using amplusplus::detail::get_type_info;

TEST_CASE("type_info_map basic insert and lookup", "[type_info_map]") {
    type_info_map<int> map;

    map.insert(typeid(int), 42);
    map.insert(typeid(double), 100);
    map.insert(typeid(std::string), 200);

    const int* p1 = map.lookup(typeid(int));
    REQUIRE(p1 != nullptr);
    REQUIRE(*p1 == 42);

    const int* p2 = map.lookup(typeid(double));
    REQUIRE(p2 != nullptr);
    REQUIRE(*p2 == 100);

    const int* p3 = map.lookup(typeid(std::string));
    REQUIRE(p3 != nullptr);
    REQUIRE(*p3 == 200);
}

TEST_CASE("type_info_map lookup missing type", "[type_info_map]") {
    type_info_map<int> map;

    map.insert(typeid(int), 1);

    const int* p = map.lookup(typeid(double));
    REQUIRE(p == nullptr);
}

TEST_CASE("type_info_map clear", "[type_info_map]") {
    type_info_map<int> map;

    map.insert(typeid(int), 1);
    map.insert(typeid(double), 2);

    const int* p1 = map.lookup(typeid(int));
    REQUIRE(p1 != nullptr);

    map.clear();

    const int* p2 = map.lookup(typeid(int));
    REQUIRE(p2 == nullptr);
}

TEST_CASE("type_info_map with string values", "[type_info_map]") {
    type_info_map<std::string> map;

    map.insert(typeid(int), "integer");
    map.insert(typeid(double), "floating point");
    map.insert(typeid(char), "character");

    const std::string* p = map.lookup(typeid(double));
    REQUIRE(p != nullptr);
    REQUIRE(*p == "floating point");
}

TEST_CASE("get_type_info helper", "[type_info_map]") {
    const std::type_info& ti1 = get_type_info<int>();
    const std::type_info& ti2 = get_type_info<int>();
    const std::type_info& ti3 = get_type_info<double>();

    // Same type should give same type_info
    REQUIRE(ti1 == ti2);
    // Different types should give different type_info
    REQUIRE(ti1 != ti3);
}

TEST_CASE("type_info_map with get_type_info", "[type_info_map]") {
    type_info_map<int> map;

    map.insert(get_type_info<int>(), 1);
    map.insert(get_type_info<double>(), 2);

    const int* p1 = map.lookup(get_type_info<int>());
    REQUIRE(p1 != nullptr);
    REQUIRE(*p1 == 1);

    const int* p2 = map.lookup(get_type_info<double>());
    REQUIRE(p2 != nullptr);
    REQUIRE(*p2 == 2);
}

// Test with custom types
struct MyType1 {};
struct MyType2 {};

TEST_CASE("type_info_map with custom types", "[type_info_map]") {
    type_info_map<std::string> map;

    map.insert(get_type_info<MyType1>(), "MyType1");
    map.insert(get_type_info<MyType2>(), "MyType2");

    const std::string* p1 = map.lookup(get_type_info<MyType1>());
    REQUIRE(p1 != nullptr);
    REQUIRE(*p1 == "MyType1");

    const std::string* p2 = map.lookup(get_type_info<MyType2>());
    REQUIRE(p2 != nullptr);
    REQUIRE(*p2 == "MyType2");

    // Different type should not match
    const std::string* p3 = map.lookup(get_type_info<int>());
    REQUIRE(p3 == nullptr);
}
