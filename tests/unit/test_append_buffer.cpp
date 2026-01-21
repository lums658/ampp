// Copyright 2024 The Trustees of Indiana University.
//
// Unit tests for append_buffer

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/append_buffer.hpp>
#include <vector>
#include <algorithm>
#include <thread>

using amplusplus::detail::append_buffer;

TEST_CASE("append_buffer basic operations", "[append_buffer]") {
    append_buffer<int> buf;

    SECTION("empty buffer") {
        REQUIRE(buf.empty());
        REQUIRE(buf.size() == 0);
    }

    SECTION("push_back single element") {
        buf.push_back(42);
        REQUIRE_FALSE(buf.empty());
        REQUIRE(buf.size() == 1);
        REQUIRE(buf[0] == 42);
    }

    SECTION("push_back multiple elements") {
        for (int i = 0; i < 100; ++i) {
            buf.push_back(i);
        }
        REQUIRE(buf.size() == 100);
        for (int i = 0; i < 100; ++i) {
            REQUIRE(buf[i] == i);
        }
    }

    SECTION("push_back triggers chunk allocation") {
        // Default initial allocation is 16, so push enough to trigger new chunks
        for (int i = 0; i < 1000; ++i) {
            buf.push_back(i);
        }
        REQUIRE(buf.size() == 1000);
        for (int i = 0; i < 1000; ++i) {
            REQUIRE(buf[i] == i);
        }
    }
}

TEST_CASE("append_buffer iterator operations", "[append_buffer][iterator]") {
    append_buffer<int> buf;
    for (int i = 0; i < 50; ++i) {
        buf.push_back(i);
    }

    SECTION("begin and end") {
        auto it = buf.begin();
        auto end = buf.end();
        REQUIRE(it != end);
        REQUIRE(*it == 0);
    }

    SECTION("iterator increment") {
        auto it = buf.begin();
        REQUIRE(*it == 0);
        ++it;
        REQUIRE(*it == 1);
        it++;
        REQUIRE(*it == 2);
    }

    SECTION("iterator decrement") {
        auto it = buf.begin();
        std::advance(it, 10);
        REQUIRE(*it == 10);
        --it;
        REQUIRE(*it == 9);
        it--;
        REQUIRE(*it == 8);
    }

    SECTION("iterator arithmetic") {
        auto it = buf.begin();
        it += 5;
        REQUIRE(*it == 5);
        it -= 3;
        REQUIRE(*it == 2);

        auto it2 = it + 10;
        REQUIRE(*it2 == 12);

        auto it3 = it2 - 5;
        REQUIRE(*it3 == 7);
    }

    SECTION("iterator difference") {
        auto it1 = buf.begin();
        auto it2 = buf.begin() + 20;
        REQUIRE(it2 - it1 == 20);
        REQUIRE(it1 - it2 == -20);
    }

    SECTION("iterator comparison") {
        auto it1 = buf.begin();
        auto it2 = buf.begin() + 5;
        auto it3 = buf.begin() + 5;

        REQUIRE(it1 < it2);
        REQUIRE(it2 > it1);
        REQUIRE(it2 == it3);
        REQUIRE(it1 <= it2);
        REQUIRE(it2 >= it1);
        REQUIRE(it2 <= it3);
        REQUIRE(it2 >= it3);
    }

    SECTION("iterator random access operator[]") {
        auto it = buf.begin();
        REQUIRE(it[0] == 0);
        REQUIRE(it[10] == 10);
        REQUIRE(it[49] == 49);
    }

    SECTION("range-based for loop") {
        int expected = 0;
        for (const auto& val : buf) {
            REQUIRE(val == expected);
            ++expected;
        }
        REQUIRE(expected == 50);
    }

    SECTION("std::distance") {
        REQUIRE(std::distance(buf.begin(), buf.end()) == 50);
    }
}

TEST_CASE("append_buffer with custom initial allocation", "[append_buffer]") {
    append_buffer<int> buf(4);  // Small initial allocation

    for (int i = 0; i < 100; ++i) {
        buf.push_back(i);
    }

    REQUIRE(buf.size() == 100);
    for (int i = 0; i < 100; ++i) {
        REQUIRE(buf[i] == i);
    }
}

TEST_CASE("append_buffer swap", "[append_buffer]") {
    append_buffer<int> buf1;
    append_buffer<int> buf2;

    for (int i = 0; i < 10; ++i) {
        buf1.push_back(i);
    }
    for (int i = 100; i < 105; ++i) {
        buf2.push_back(i);
    }

    buf1.swap(buf2);

    REQUIRE(buf1.size() == 5);
    REQUIRE(buf2.size() == 10);
    REQUIRE(buf1[0] == 100);
    REQUIRE(buf2[0] == 0);
}

TEST_CASE("append_buffer comparison operators", "[append_buffer]") {
    append_buffer<int> buf1;
    append_buffer<int> buf2;
    append_buffer<int> buf3;

    for (int i = 0; i < 5; ++i) {
        buf1.push_back(i);
        buf2.push_back(i);
        buf3.push_back(i + 1);
    }

    REQUIRE(buf1 == buf2);
    REQUIRE_FALSE(buf1 != buf2);
    REQUIRE(buf1 < buf3);
    REQUIRE(buf3 > buf1);
    REQUIRE(buf1 <= buf2);
    REQUIRE(buf1 >= buf2);
}

TEST_CASE("append_buffer with non-trivial type", "[append_buffer]") {
    append_buffer<std::string> buf;

    buf.push_back("hello");
    buf.push_back("world");
    buf.push_back("test");

    REQUIRE(buf.size() == 3);
    REQUIRE(buf[0] == "hello");
    REQUIRE(buf[1] == "world");
    REQUIRE(buf[2] == "test");
}

TEST_CASE("append_buffer reverse iterators", "[append_buffer][iterator]") {
    append_buffer<int> buf;
    for (int i = 0; i < 10; ++i) {
        buf.push_back(i);
    }

    SECTION("rbegin and rend") {
        auto rit = buf.rbegin();
        REQUIRE(*rit == 9);
        ++rit;
        REQUIRE(*rit == 8);
    }

    SECTION("reverse iteration") {
        std::vector<int> reversed;
        for (auto rit = buf.rbegin(); rit != buf.rend(); ++rit) {
            reversed.push_back(*rit);
        }
        REQUIRE(reversed.size() == 10);
        REQUIRE(reversed[0] == 9);
        REQUIRE(reversed[9] == 0);
    }
}

TEST_CASE("append_buffer STL algorithm compatibility", "[append_buffer][iterator]") {
    append_buffer<int> buf;
    for (int i = 0; i < 20; ++i) {
        buf.push_back(i % 5);  // 0,1,2,3,4,0,1,2,3,4,...
    }

    SECTION("std::find") {
        auto it = std::find(buf.begin(), buf.end(), 3);
        REQUIRE(it != buf.end());
        REQUIRE(*it == 3);
    }

    SECTION("std::count") {
        auto count = std::count(buf.begin(), buf.end(), 2);
        REQUIRE(count == 4);
    }

    SECTION("std::accumulate") {
        int sum = 0;
        for (auto it = buf.begin(); it != buf.end(); ++it) {
            sum += *it;
        }
        REQUIRE(sum == 40);  // 4 * (0+1+2+3+4) = 40
    }

    SECTION("std::copy to vector") {
        std::vector<int> vec(buf.size());
        std::copy(buf.begin(), buf.end(), vec.begin());
        REQUIRE(vec.size() == 20);
        REQUIRE(vec[0] == 0);
        REQUIRE(vec[5] == 0);
    }
}
