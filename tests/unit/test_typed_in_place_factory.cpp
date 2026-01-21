// Copyright 2024 The Trustees of Indiana University.
//
// Unit tests for typed_in_place_factory_owning

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/typed_in_place_factory_owning.hpp>
#include <string>
#include <memory>

using namespace amplusplus::detail;

// Test class with various constructors
struct TestObject {
    int a;
    double b;
    std::string c;

    TestObject() : a(0), b(0.0), c() {}
    explicit TestObject(int a_) : a(a_), b(0.0), c() {}
    TestObject(int a_, double b_) : a(a_), b(b_), c() {}
    TestObject(int a_, double b_, const std::string& c_) : a(a_), b(b_), c(c_) {}
};

TEST_CASE("typed_in_place_factory_owning zero arguments", "[factory]") {
    typed_in_place_factory_owning<TestObject> factory;

    alignas(TestObject) char buffer[sizeof(TestObject)];
    factory.apply(buffer);

    TestObject* obj = reinterpret_cast<TestObject*>(buffer);
    REQUIRE(obj->a == 0);
    REQUIRE(obj->b == 0.0);
    REQUIRE(obj->c.empty());

    obj->~TestObject();
}

TEST_CASE("typed_in_place_factory_owning one argument", "[factory]") {
    typed_in_place_factory_owning<TestObject, int> factory(42);

    alignas(TestObject) char buffer[sizeof(TestObject)];
    factory.apply(buffer);

    TestObject* obj = reinterpret_cast<TestObject*>(buffer);
    REQUIRE(obj->a == 42);
    REQUIRE(obj->b == 0.0);

    obj->~TestObject();
}

TEST_CASE("typed_in_place_factory_owning two arguments", "[factory]") {
    typed_in_place_factory_owning<TestObject, int, double> factory(10, 3.14);

    alignas(TestObject) char buffer[sizeof(TestObject)];
    factory.apply(buffer);

    TestObject* obj = reinterpret_cast<TestObject*>(buffer);
    REQUIRE(obj->a == 10);
    REQUIRE(obj->b == 3.14);

    obj->~TestObject();
}

TEST_CASE("typed_in_place_factory_owning three arguments", "[factory]") {
    typed_in_place_factory_owning<TestObject, int, double, std::string> factory(5, 2.71, "hello");

    alignas(TestObject) char buffer[sizeof(TestObject)];
    factory.apply(buffer);

    TestObject* obj = reinterpret_cast<TestObject*>(buffer);
    REQUIRE(obj->a == 5);
    REQUIRE(obj->b == 2.71);
    REQUIRE(obj->c == "hello");

    obj->~TestObject();
}

TEST_CASE("make_typed_in_place_factory_owning helper", "[factory]") {
    auto factory = make_typed_in_place_factory_owning<TestObject>(100, 1.5, std::string("test"));

    alignas(TestObject) char buffer[sizeof(TestObject)];
    factory.apply(buffer);

    TestObject* obj = reinterpret_cast<TestObject*>(buffer);
    REQUIRE(obj->a == 100);
    REQUIRE(obj->b == 1.5);
    REQUIRE(obj->c == "test");

    obj->~TestObject();
}

TEST_CASE("typed_in_place_factory_owning get_args", "[factory]") {
    typed_in_place_factory_owning<TestObject, int, double> factory(42, 3.14);

    const auto& args = factory.get_args();
    REQUIRE(std::get<0>(args) == 42);
    REQUIRE(std::get<1>(args) == 3.14);
}

TEST_CASE("typed_in_place_factory_owning get<I>", "[factory]") {
    typed_in_place_factory_owning<TestObject, int, double, std::string> factory(1, 2.0, "three");

    REQUIRE(factory.get<0>() == 1);
    REQUIRE(factory.get<1>() == 2.0);
    REQUIRE(factory.get<2>() == "three");
}

TEST_CASE("typed_in_place_factory_owning legacy aliases", "[factory]") {
    SECTION("typed_in_place_factory_owning0") {
        typed_in_place_factory_owning0<TestObject> factory;
        alignas(TestObject) char buffer[sizeof(TestObject)];
        factory.apply(buffer);
        TestObject* obj = reinterpret_cast<TestObject*>(buffer);
        REQUIRE(obj->a == 0);
        obj->~TestObject();
    }

    SECTION("typed_in_place_factory_owning1") {
        typed_in_place_factory_owning1<TestObject, int> factory(99);
        alignas(TestObject) char buffer[sizeof(TestObject)];
        factory.apply(buffer);
        TestObject* obj = reinterpret_cast<TestObject*>(buffer);
        REQUIRE(obj->a == 99);
        obj->~TestObject();
    }

    SECTION("typed_in_place_factory_owning2") {
        typed_in_place_factory_owning2<TestObject, int, double> factory(7, 8.0);
        alignas(TestObject) char buffer[sizeof(TestObject)];
        factory.apply(buffer);
        TestObject* obj = reinterpret_cast<TestObject*>(buffer);
        REQUIRE(obj->a == 7);
        REQUIRE(obj->b == 8.0);
        obj->~TestObject();
    }
}

TEST_CASE("typed_in_place_factory_owning with move-only arguments", "[factory]") {
    // Test that factory properly handles arguments that decay
    auto factory = make_typed_in_place_factory_owning<std::string>("hello world");

    alignas(std::string) char buffer[sizeof(std::string)];
    factory.apply(buffer);

    std::string* obj = reinterpret_cast<std::string*>(buffer);
    REQUIRE(*obj == "hello world");

    obj->~basic_string();
}
