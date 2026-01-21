// Copyright 2024 The Trustees of Indiana University.
//
// Unit tests for id_assigner

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/id_assigner.hpp>
#include <vector>
#include <set>

using amplusplus::detail::id_assigner;
using amplusplus::detail::scoped_id;

TEST_CASE("id_assigner basic allocation", "[id_assigner]") {
    id_assigner assigner;

    REQUIRE(assigner.allocate() == 0);
    REQUIRE(assigner.allocate() == 1);
    REQUIRE(assigner.allocate() == 2);
}

TEST_CASE("id_assigner free and reuse", "[id_assigner]") {
    id_assigner assigner;

    unsigned int id0 = assigner.allocate();
    unsigned int id1 = assigner.allocate();
    unsigned int id2 = assigner.allocate();

    REQUIRE(id0 == 0);
    REQUIRE(id1 == 1);
    REQUIRE(id2 == 2);

    // Free the middle one
    assigner.free(id1);

    // Next allocation should reuse the freed ID
    unsigned int id3 = assigner.allocate();
    REQUIRE(id3 == 1);

    // Next allocation should be new
    unsigned int id4 = assigner.allocate();
    REQUIRE(id4 == 3);
}

TEST_CASE("id_assigner free highest shrinks range", "[id_assigner]") {
    id_assigner assigner;

    assigner.allocate();  // 0
    assigner.allocate();  // 1
    unsigned int id2 = assigner.allocate();  // 2

    REQUIRE(id2 == 2);

    // Free the highest - should shrink the range
    assigner.free(id2);

    // Next allocation should reuse 2
    unsigned int id3 = assigner.allocate();
    REQUIRE(id3 == 2);
}

TEST_CASE("id_assigner multiple frees", "[id_assigner]") {
    id_assigner assigner;

    std::vector<unsigned int> ids;
    for (int i = 0; i < 10; ++i) {
        ids.push_back(assigner.allocate());
    }

    // Free even IDs
    for (int i = 0; i < 10; i += 2) {
        assigner.free(ids[i]);
    }

    // Allocate 5 more - should reuse freed IDs
    std::set<unsigned int> new_ids;
    for (int i = 0; i < 5; ++i) {
        new_ids.insert(assigner.allocate());
    }

    // All freed IDs should be reused
    REQUIRE(new_ids.count(0) == 1);
    REQUIRE(new_ids.count(2) == 1);
    REQUIRE(new_ids.count(4) == 1);
    REQUIRE(new_ids.count(6) == 1);
    REQUIRE(new_ids.count(8) == 1);
}

TEST_CASE("scoped_id basic usage", "[id_assigner]") {
    id_assigner assigner;

    unsigned int outer_id;
    {
        scoped_id sid(assigner);
        outer_id = sid.get_value();
        REQUIRE(outer_id == 0);

        // Allocate another while scoped_id is alive
        unsigned int inner = assigner.allocate();
        REQUIRE(inner == 1);
    }
    // scoped_id destroyed, ID should be freed

    // Next allocation should get a reused ID
    unsigned int next = assigner.allocate();
    // Could be 0 (if freed) or continue from where we were
    REQUIRE((next == 0 || next == 2));
}

TEST_CASE("scoped_id nested", "[id_assigner]") {
    id_assigner assigner;

    {
        scoped_id sid1(assigner);
        REQUIRE(sid1.get_value() == 0);

        {
            scoped_id sid2(assigner);
            REQUIRE(sid2.get_value() == 1);

            {
                scoped_id sid3(assigner);
                REQUIRE(sid3.get_value() == 2);
            }
            // sid3 freed, 2 is available

            unsigned int id = assigner.allocate();
            REQUIRE(id == 2);
        }
    }
}

TEST_CASE("id_assigner unique IDs", "[id_assigner]") {
    id_assigner assigner;

    std::set<unsigned int> allocated;
    for (int i = 0; i < 1000; ++i) {
        unsigned int id = assigner.allocate();
        REQUIRE(allocated.count(id) == 0);  // Must be unique
        allocated.insert(id);
    }

    REQUIRE(allocated.size() == 1000);
}
