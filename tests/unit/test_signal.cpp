// Copyright 2024 The Trustees of Indiana University.
//
// Unit tests for signal

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/signal.hpp>
#include <vector>
#include <string>

using amplusplus::signal0;
using amplusplus::signal1;
using amplusplus::scoped_attach;

TEST_CASE("signal0 basic emit", "[signal]") {
    signal0 sig;
    int counter = 0;

    void* handle = sig.attach([&counter]() { ++counter; });

    sig();
    REQUIRE(counter == 1);

    sig();
    REQUIRE(counter == 2);

    sig.detach(handle);
}

TEST_CASE("signal0 multiple handlers", "[signal]") {
    signal0 sig;
    int counter1 = 0;
    int counter2 = 0;

    void* h1 = sig.attach([&counter1]() { ++counter1; });
    void* h2 = sig.attach([&counter2]() { counter2 += 10; });

    sig();
    REQUIRE(counter1 == 1);
    REQUIRE(counter2 == 10);

    sig.detach(h1);
    sig.detach(h2);
}

TEST_CASE("signal0 detach", "[signal]") {
    signal0 sig;
    int counter = 0;

    void* handle = sig.attach([&counter]() { ++counter; });

    sig();
    REQUIRE(counter == 1);

    sig.detach(handle);

    sig();
    REQUIRE(counter == 1);  // Handler no longer called
}

TEST_CASE("signal0 detach middle handler", "[signal]") {
    signal0 sig;
    std::vector<int> calls;

    void* h1 = sig.attach([&calls]() { calls.push_back(1); });
    void* h2 = sig.attach([&calls]() { calls.push_back(2); });
    void* h3 = sig.attach([&calls]() { calls.push_back(3); });

    sig();
    REQUIRE(calls == std::vector<int>{1, 2, 3});

    calls.clear();
    sig.detach(h2);

    sig();
    REQUIRE(calls == std::vector<int>{1, 3});

    sig.detach(h1);
    sig.detach(h3);
}

TEST_CASE("signal1 basic emit", "[signal]") {
    signal1<int> sig;
    int received = 0;

    void* handle = sig.attach([&received](int val) { received = val; });

    sig(42);
    REQUIRE(received == 42);

    sig(100);
    REQUIRE(received == 100);

    sig.detach(handle);
}

TEST_CASE("signal1 with string argument", "[signal]") {
    signal1<std::string> sig;
    std::string received;

    void* handle = sig.attach([&received](const std::string& s) { received = s; });

    sig("hello");
    REQUIRE(received == "hello");

    sig("world");
    REQUIRE(received == "world");

    sig.detach(handle);
}

TEST_CASE("signal1 multiple handlers", "[signal]") {
    signal1<int> sig;
    int sum = 0;
    int product = 1;

    void* h1 = sig.attach([&sum](int val) { sum += val; });
    void* h2 = sig.attach([&product](int val) { product *= val; });

    sig(5);
    REQUIRE(sum == 5);
    REQUIRE(product == 5);

    sig(3);
    REQUIRE(sum == 8);
    REQUIRE(product == 15);

    sig.detach(h1);
    sig.detach(h2);
}

TEST_CASE("scoped_attach signal0", "[signal]") {
    signal0 sig;
    int counter = 0;

    {
        scoped_attach<signal0> sa(sig, [&counter]() { ++counter; });
        sig();
        REQUIRE(counter == 1);
    }
    // scoped_attach destroyed, handler detached

    sig();
    REQUIRE(counter == 1);  // Handler no longer called
}

TEST_CASE("scoped_attach signal1", "[signal]") {
    signal1<int> sig;
    int total = 0;

    {
        scoped_attach<signal1<int>> sa(sig, [&total](int val) { total += val; });
        sig(10);
        REQUIRE(total == 10);

        sig(5);
        REQUIRE(total == 15);
    }

    sig(100);
    REQUIRE(total == 15);  // Handler no longer called
}

TEST_CASE("scoped_attach nested", "[signal]") {
    signal0 sig;
    int counter1 = 0;
    int counter2 = 0;

    {
        scoped_attach<signal0> sa1(sig, [&counter1]() { ++counter1; });

        sig();
        REQUIRE(counter1 == 1);
        REQUIRE(counter2 == 0);

        {
            scoped_attach<signal0> sa2(sig, [&counter2]() { ++counter2; });

            sig();
            REQUIRE(counter1 == 2);
            REQUIRE(counter2 == 1);
        }

        sig();
        REQUIRE(counter1 == 3);
        REQUIRE(counter2 == 1);  // sa2 destroyed
    }

    sig();
    REQUIRE(counter1 == 3);  // sa1 destroyed
    REQUIRE(counter2 == 1);
}

TEST_CASE("signal with reference_wrapper", "[signal]") {
    signal0 sig;

    struct Handler {
        int& counter;
        Handler(int& c) : counter(c) {}
        void operator()() { ++counter; }
    };

    int counter = 0;
    Handler h(counter);

    void* handle = sig.attach(std::ref(h));

    sig();
    REQUIRE(counter == 1);

    sig.detach(handle);
}

TEST_CASE("signal handler can modify captured state", "[signal]") {
    signal1<int> sig;
    std::vector<int> values;

    void* handle = sig.attach([&values](int v) { values.push_back(v); });

    sig(1);
    sig(2);
    sig(3);

    REQUIRE(values == std::vector<int>{1, 2, 3});

    sig.detach(handle);
}
