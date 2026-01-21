// Copyright 2024 The Trustees of Indiana University.
//
// Unit tests for vector_of_noncopyable

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/vector_of_noncopyable.hpp>
#include <string>

using amplusplus::detail::vector_of_noncopyable;

// A simple non-copyable type for testing
struct NonCopyable {
    int value;

    NonCopyable() : value(0) {}
    explicit NonCopyable(int v) : value(v) {}

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    // Move operations are needed for erase to work
    NonCopyable(NonCopyable&& other) noexcept : value(other.value) { other.value = 0; }
    NonCopyable& operator=(NonCopyable&& other) noexcept {
        value = other.value;
        other.value = 0;
        return *this;
    }

    void swap(NonCopyable& other) {
        std::swap(value, other.value);
    }
};

TEST_CASE("vector_of_noncopyable default construction", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;
    REQUIRE(vec.empty());
    REQUIRE(vec.size() == 0);
}

TEST_CASE("vector_of_noncopyable sized construction", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec(5);
    REQUIRE_FALSE(vec.empty());
    REQUIRE(vec.size() == 5);

    // All elements should be default-constructed
    for (size_t i = 0; i < vec.size(); ++i) {
        REQUIRE(vec[i].value == 0);
    }
}

TEST_CASE("vector_of_noncopyable push_back_empty", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;

    vec.push_back_empty();
    REQUIRE(vec.size() == 1);
    REQUIRE(vec.back().value == 0);

    vec.push_back_empty();
    REQUIRE(vec.size() == 2);
}

TEST_CASE("vector_of_noncopyable push_back_swap", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;

    NonCopyable item(42);
    vec.push_back_swap(item);

    REQUIRE(vec.size() == 1);
    REQUIRE(vec.back().value == 42);
    REQUIRE(item.value == 0);  // Original was swapped out
}

TEST_CASE("vector_of_noncopyable back", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;

    NonCopyable a(10), b(20), c(30);
    vec.push_back_swap(a);
    vec.push_back_swap(b);
    vec.push_back_swap(c);

    REQUIRE(vec.back().value == 30);

    const auto& const_vec = vec;
    REQUIRE(const_vec.back().value == 30);
}

TEST_CASE("vector_of_noncopyable operator[]", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;

    NonCopyable a(100), b(200), c(300);
    vec.push_back_swap(a);
    vec.push_back_swap(b);
    vec.push_back_swap(c);

    REQUIRE(vec[0].value == 100);
    REQUIRE(vec[1].value == 200);
    REQUIRE(vec[2].value == 300);

    // Modify through operator[]
    vec[1].value = 999;
    REQUIRE(vec[1].value == 999);
}

TEST_CASE("vector_of_noncopyable iterators", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;

    NonCopyable a(1), b(2), c(3);
    vec.push_back_swap(a);
    vec.push_back_swap(b);
    vec.push_back_swap(c);

    SECTION("begin and end") {
        REQUIRE(vec.begin() != vec.end());
        REQUIRE(vec.end() - vec.begin() == 3);
    }

    SECTION("iteration") {
        int expected = 1;
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            REQUIRE(it->value == expected);
            ++expected;
        }
    }

    SECTION("const iteration") {
        const auto& const_vec = vec;
        int expected = 1;
        for (auto it = const_vec.begin(); it != const_vec.end(); ++it) {
            REQUIRE(it->value == expected);
            ++expected;
        }
    }
}

TEST_CASE("vector_of_noncopyable erase", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;

    NonCopyable a(1), b(2), c(3), d(4);
    vec.push_back_swap(a);
    vec.push_back_swap(b);
    vec.push_back_swap(c);
    vec.push_back_swap(d);

    SECTION("erase middle element") {
        vec.erase(vec.begin() + 1);  // Erase element with value 2
        REQUIRE(vec.size() == 3);
        REQUIRE(vec[0].value == 1);
        REQUIRE(vec[1].value == 3);
        REQUIRE(vec[2].value == 4);
    }

    SECTION("erase first element") {
        vec.erase(vec.begin());
        REQUIRE(vec.size() == 3);
        REQUIRE(vec[0].value == 2);
        REQUIRE(vec[1].value == 3);
        REQUIRE(vec[2].value == 4);
    }

    SECTION("erase last element") {
        vec.erase(vec.end() - 1);
        REQUIRE(vec.size() == 3);
        REQUIRE(vec[0].value == 1);
        REQUIRE(vec[1].value == 2);
        REQUIRE(vec[2].value == 3);
    }
}

TEST_CASE("vector_of_noncopyable growth", "[vector_of_noncopyable]") {
    vector_of_noncopyable<NonCopyable> vec;

    // Push many elements to trigger reallocation
    for (int i = 0; i < 100; ++i) {
        NonCopyable item(i);
        vec.push_back_swap(item);
    }

    REQUIRE(vec.size() == 100);

    // Verify all elements are correct
    for (int i = 0; i < 100; ++i) {
        REQUIRE(vec[i].value == i);
    }
}
