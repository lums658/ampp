// Copyright 2024 The Trustees of Indiana University.
//
// First-principles tests for append_buffer thread safety
//
// From the header documentation:
// - "atomic push_back (i.e., multiple push_back operations do not conflict)"
// - "has stable iterators and references"
// - "There is no guard that elements that have been pushed are actually stored
//    before they are accessed"

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/append_buffer.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <algorithm>

using amplusplus::detail::append_buffer;

TEST_CASE("append_buffer concurrent push_back produces unique indices", "[append_buffer][threaded]") {
    // First principle: atomic push_back means each call returns a unique index
    append_buffer<int> buf;
    constexpr int num_threads = 8;
    constexpr int pushes_per_thread = 1000;

    std::vector<std::vector<size_t>> indices(num_threads);
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&buf, &indices, t]() {
            for (int i = 0; i < pushes_per_thread; ++i) {
                size_t idx = buf.push_back(t * pushes_per_thread + i);
                indices[t].push_back(idx);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Verify: all indices must be unique
    std::set<size_t> all_indices;
    for (const auto& vec : indices) {
        for (size_t idx : vec) {
            REQUIRE(all_indices.insert(idx).second);  // Must be unique
        }
    }

    REQUIRE(all_indices.size() == num_threads * pushes_per_thread);
    REQUIRE(buf.size() == num_threads * pushes_per_thread);
}

TEST_CASE("append_buffer concurrent push_back all values stored", "[append_buffer][threaded]") {
    // First principle: every pushed value should be retrievable
    append_buffer<int> buf;
    constexpr int num_threads = 8;
    constexpr int pushes_per_thread = 1000;

    std::vector<std::pair<size_t, int>> stored(num_threads * pushes_per_thread);
    std::atomic<size_t> store_idx{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&buf, &stored, &store_idx, t]() {
            for (int i = 0; i < pushes_per_thread; ++i) {
                int value = t * 10000 + i;  // Unique value per push
                size_t idx = buf.push_back(value);
                size_t si = store_idx.fetch_add(1);
                stored[si] = {idx, value};
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Verify: each stored (index, value) pair matches what's in the buffer
    for (const auto& [idx, expected] : stored) {
        REQUIRE(buf[idx] == expected);
    }
}

TEST_CASE("append_buffer stable references under growth", "[append_buffer][threaded]") {
    // First principle: "stable iterators and references" means pointers
    // obtained before growth remain valid after growth
    append_buffer<int> buf(4);  // Small initial allocation to force growth

    // Push initial elements and capture references
    for (int i = 0; i < 4; ++i) {
        buf.push_back(i * 100);
    }

    int* ref0 = &buf[0];
    int* ref1 = &buf[1];
    int* ref2 = &buf[2];
    int* ref3 = &buf[3];

    // Force growth by pushing many more elements
    for (int i = 4; i < 1000; ++i) {
        buf.push_back(i * 100);
    }

    // Original references must still be valid and point to correct values
    REQUIRE(*ref0 == 0);
    REQUIRE(*ref1 == 100);
    REQUIRE(*ref2 == 200);
    REQUIRE(*ref3 == 300);
}

TEST_CASE("append_buffer iterator stability under concurrent growth", "[append_buffer][threaded]") {
    // First principle: iterators remain valid while buffer grows
    append_buffer<int> buf(4);

    // Push initial elements
    for (int i = 0; i < 10; ++i) {
        buf.push_back(i);
    }

    auto it_begin = buf.begin();
    auto it_5 = buf.begin() + 5;

    // Grow buffer concurrently
    std::thread grower([&buf]() {
        for (int i = 10; i < 1000; ++i) {
            buf.push_back(i);
        }
    });

    // Read through original iterators while growth happens
    // (Note: we can only safely read elements that existed before growth started)
    int sum = 0;
    for (int i = 0; i < 10; ++i) {
        sum += it_begin[i];
    }

    grower.join();

    REQUIRE(sum == 45);  // 0+1+2+...+9
    REQUIRE(*it_5 == 5);
}

TEST_CASE("append_buffer size consistency", "[append_buffer][threaded]") {
    // First principle: size() should always reflect the number of completed push_back calls
    append_buffer<int> buf;
    constexpr int num_threads = 4;
    constexpr int pushes_per_thread = 500;

    std::atomic<int> completed_pushes{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&buf, &completed_pushes]() {
            for (int i = 0; i < pushes_per_thread; ++i) {
                buf.push_back(i);
                completed_pushes.fetch_add(1);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    REQUIRE(buf.size() == num_threads * pushes_per_thread);
    REQUIRE(completed_pushes.load() == num_threads * pushes_per_thread);
}

TEST_CASE("append_buffer max_size boundary", "[append_buffer]") {
    // First principle: buffer should respect max_capacity
    constexpr size_t max_cap = 100;
    append_buffer<int> buf(16, max_cap);

    REQUIRE(buf.max_size() == max_cap);

    // Fill to capacity
    for (size_t i = 0; i < max_cap; ++i) {
        buf.push_back(static_cast<int>(i));
    }

    REQUIRE(buf.size() == max_cap);

    // Verify all values
    for (size_t i = 0; i < max_cap; ++i) {
        REQUIRE(buf[i] == static_cast<int>(i));
    }
}

TEST_CASE("append_buffer chunk allocation boundaries", "[append_buffer]") {
    // First principle: elements near chunk boundaries should work correctly
    constexpr size_t initial_alloc = 8;
    append_buffer<int> buf(initial_alloc);

    // Push elements to cross multiple chunk boundaries
    // Chunk sizes: 8, 8, 16, 32, 64, ...
    constexpr size_t test_size = 200;

    for (size_t i = 0; i < test_size; ++i) {
        buf.push_back(static_cast<int>(i));
    }

    // Verify all elements, especially at boundaries
    for (size_t i = 0; i < test_size; ++i) {
        REQUIRE(buf[i] == static_cast<int>(i));
    }

    // Verify iterator traversal crosses boundaries correctly
    int expected = 0;
    for (auto it = buf.begin(); it != buf.end(); ++it, ++expected) {
        REQUIRE(*it == expected);
    }
}

TEST_CASE("append_buffer empty state invariants", "[append_buffer]") {
    // First principle: empty buffer has consistent state
    append_buffer<int> buf;

    REQUIRE(buf.empty());
    REQUIRE(buf.size() == 0);
    REQUIRE(buf.begin() == buf.end());
    REQUIRE(std::distance(buf.begin(), buf.end()) == 0);
}

TEST_CASE("append_buffer single element edge case", "[append_buffer]") {
    // First principle: single-element buffer works correctly
    append_buffer<int> buf;

    buf.push_back(42);

    REQUIRE_FALSE(buf.empty());
    REQUIRE(buf.size() == 1);
    REQUIRE(buf[0] == 42);
    REQUIRE(*buf.begin() == 42);
    REQUIRE(buf.begin() + 1 == buf.end());
    REQUIRE(std::distance(buf.begin(), buf.end()) == 1);
}
