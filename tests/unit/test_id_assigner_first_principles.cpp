// Copyright 2024 The Trustees of Indiana University.
//
// First-principles tests for id_assigner
//
// Expected contract for an ID allocator:
// - allocate() returns a unique ID not currently in use
// - free(id) makes that ID available for reuse
// - IDs are non-negative integers
// - No ID is returned twice while still allocated
// - scoped_id provides RAII cleanup

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/id_assigner.hpp>
#include <set>
#include <vector>
#include <thread>
#include <algorithm>

using amplusplus::detail::id_assigner;
using amplusplus::detail::scoped_id;

TEST_CASE("id_assigner uniqueness invariant", "[id_assigner][first-principles]") {
    // First principle: no two concurrent allocations return the same ID
    id_assigner assigner;
    std::set<unsigned int> allocated;

    for (int i = 0; i < 10000; ++i) {
        unsigned int id = assigner.allocate();
        bool inserted = allocated.insert(id).second;
        REQUIRE(inserted);  // Must be unique
    }

    REQUIRE(allocated.size() == 10000);
}

TEST_CASE("id_assigner free enables reuse", "[id_assigner][first-principles]") {
    // First principle: freed IDs can be reallocated
    id_assigner assigner;

    unsigned int id1 = assigner.allocate();
    unsigned int id2 = assigner.allocate();
    unsigned int id3 = assigner.allocate();

    assigner.free(id2);

    // Allocate many more IDs - id2 should appear among them
    std::set<unsigned int> new_ids;
    for (int i = 0; i < 100; ++i) {
        new_ids.insert(assigner.allocate());
    }

    // id2 should have been reused (it was freed)
    REQUIRE(new_ids.count(id2) == 1);
    // id1 and id3 should NOT appear (they weren't freed)
    REQUIRE(new_ids.count(id1) == 0);
    REQUIRE(new_ids.count(id3) == 0);
}

TEST_CASE("id_assigner free all then reallocate", "[id_assigner][first-principles]") {
    // First principle: freeing all IDs allows full reuse
    id_assigner assigner;

    std::vector<unsigned int> first_batch;
    for (int i = 0; i < 100; ++i) {
        first_batch.push_back(assigner.allocate());
    }

    // Free all in reverse order
    for (auto it = first_batch.rbegin(); it != first_batch.rend(); ++it) {
        assigner.free(*it);
    }

    // Reallocate same number - should get the same IDs back (in some order)
    std::set<unsigned int> second_batch;
    for (int i = 0; i < 100; ++i) {
        second_batch.insert(assigner.allocate());
    }

    std::set<unsigned int> first_set(first_batch.begin(), first_batch.end());
    REQUIRE(first_set == second_batch);
}

TEST_CASE("scoped_id RAII guarantee", "[id_assigner][first-principles]") {
    // First principle: scoped_id frees on destruction, even with exceptions
    id_assigner assigner;
    unsigned int captured_id;

    try {
        scoped_id sid(assigner);
        captured_id = sid.get_value();

        // Verify the ID is allocated
        REQUIRE(captured_id == 0);

        throw std::runtime_error("test");
    } catch (...) {
        // scoped_id should have freed the ID
    }

    // The freed ID should be available again
    unsigned int new_id = assigner.allocate();
    REQUIRE(new_id == captured_id);
}

TEST_CASE("scoped_id nested allocation", "[id_assigner][first-principles]") {
    // First principle: nested scoped_ids allocate distinct IDs
    id_assigner assigner;

    {
        scoped_id s1(assigner);
        REQUIRE(s1.get_value() == 0);

        {
            scoped_id s2(assigner);
            REQUIRE(s2.get_value() == 1);

            {
                scoped_id s3(assigner);
                REQUIRE(s3.get_value() == 2);
            }
            // s3 freed, 2 available

            scoped_id s4(assigner);
            // Should reuse 2 or get 3
            REQUIRE((s4.get_value() == 2 || s4.get_value() == 3));
        }
    }
}

TEST_CASE("id_assigner stress test with interleaved alloc/free", "[id_assigner][first-principles]") {
    // First principle: complex alloc/free patterns maintain uniqueness
    id_assigner assigner;
    std::set<unsigned int> currently_allocated;

    for (int round = 0; round < 1000; ++round) {
        // Allocate some
        for (int i = 0; i < 5; ++i) {
            unsigned int id = assigner.allocate();
            bool inserted = currently_allocated.insert(id).second;
            REQUIRE(inserted);  // Must be unique among currently allocated
        }

        // Free some (every other one)
        std::vector<unsigned int> to_free;
        int idx = 0;
        for (unsigned int id : currently_allocated) {
            if (idx++ % 2 == 0) to_free.push_back(id);
        }
        for (unsigned int id : to_free) {
            assigner.free(id);
            currently_allocated.erase(id);
        }
    }
}

TEST_CASE("id_assigner double-free detection", "[id_assigner][first-principles]") {
    // Note: Current implementation doesn't detect double-free
    // This test documents the expected behavior (or lack thereof)
    // A robust implementation would either:
    // 1. Assert/throw on double-free
    // 2. Be idempotent (no-op on double-free)

    // For now, we just verify the allocator remains usable after potential misuse
    id_assigner assigner;

    unsigned int id = assigner.allocate();
    assigner.free(id);

    // After free, allocator should still work
    unsigned int id2 = assigner.allocate();
    unsigned int id3 = assigner.allocate();

    REQUIRE(id2 != id3);
}

TEST_CASE("id_assigner monotonic growth without free", "[id_assigner][first-principles]") {
    // First principle: without frees, IDs grow monotonically from 0
    id_assigner assigner;

    for (unsigned int expected = 0; expected < 1000; ++expected) {
        unsigned int actual = assigner.allocate();
        REQUIRE(actual == expected);
    }
}

TEST_CASE("scoped_id get_value is stable", "[id_assigner][first-principles]") {
    // First principle: get_value returns same value throughout lifetime
    id_assigner assigner;

    scoped_id sid(assigner);
    unsigned int first_read = sid.get_value();

    // Multiple reads should return same value
    for (int i = 0; i < 100; ++i) {
        REQUIRE(sid.get_value() == first_read);
    }
}
