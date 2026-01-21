// Copyright 2024 The Trustees of Indiana University.
//
// First-principles tests for signal/slot implementation
//
// Expected signal/slot contract:
// - Handlers attached to a signal are called when signal is emitted
// - Handlers can be detached and will no longer be called
// - Multiple handlers can be attached
// - Handler order should be stable (FIFO attachment order)
// - Detaching during emission should be safe
// - scoped_attach provides RAII cleanup

#include <catch2/catch_test_macros.hpp>
#include <am++/detail/signal.hpp>
#include <vector>
#include <string>
#include <stdexcept>

using amplusplus::signal0;
using amplusplus::signal1;
using amplusplus::scoped_attach;

TEST_CASE("signal handler attachment order is preserved", "[signal][first-principles]") {
    // First principle: handlers should be called in attachment order
    signal0 sig;
    std::vector<int> call_order;

    void* h1 = sig.attach([&call_order]() { call_order.push_back(1); });
    void* h2 = sig.attach([&call_order]() { call_order.push_back(2); });
    void* h3 = sig.attach([&call_order]() { call_order.push_back(3); });

    sig();

    REQUIRE(call_order == std::vector<int>{1, 2, 3});

    sig.detach(h1);
    sig.detach(h2);
    sig.detach(h3);
}

TEST_CASE("signal with no handlers is safe to emit", "[signal][first-principles]") {
    // First principle: emitting with no handlers should be a no-op, not an error
    signal0 sig0;
    signal1<int> sig1;

    REQUIRE_NOTHROW(sig0());
    REQUIRE_NOTHROW(sig1(42));
}

TEST_CASE("signal detach is idempotent for signal state", "[signal][first-principles]") {
    // First principle: after detach, handler is never called again
    signal0 sig;
    int counter = 0;

    void* handle = sig.attach([&counter]() { ++counter; });

    sig();
    REQUIRE(counter == 1);

    sig.detach(handle);

    // Multiple emissions after detach
    sig();
    sig();
    sig();

    REQUIRE(counter == 1);  // Still 1, never called again
}

TEST_CASE("signal1 passes argument correctly", "[signal][first-principles]") {
    // First principle: argument passed to emit should reach all handlers unchanged
    signal1<int> sig;
    std::vector<int> received;

    void* h1 = sig.attach([&received](int v) { received.push_back(v); });
    void* h2 = sig.attach([&received](int v) { received.push_back(v * 2); });

    sig(5);
    sig(10);

    REQUIRE(received == std::vector<int>{5, 10, 10, 20});

    sig.detach(h1);
    sig.detach(h2);
}

TEST_CASE("signal1 with const reference argument", "[signal][first-principles]") {
    // First principle: const ref arguments should not be copied unnecessarily
    signal1<std::string> sig;
    const std::string* received_ptr = nullptr;

    void* handle = sig.attach([&received_ptr](const std::string& s) {
        received_ptr = &s;
    });

    std::string test_str = "test";
    sig(test_str);

    // The handler should receive a reference to the original (or a copy held by signal)
    // This tests that the signal properly forwards const references
    REQUIRE(received_ptr != nullptr);
    REQUIRE(*received_ptr == "test");

    sig.detach(handle);
}

TEST_CASE("scoped_attach guarantees cleanup on scope exit", "[signal][first-principles]") {
    // First principle: RAII means cleanup happens even with exceptions
    signal0 sig;
    int counter = 0;

    try {
        scoped_attach<signal0> sa(sig, [&counter]() { ++counter; });
        sig();
        REQUIRE(counter == 1);
        throw std::runtime_error("test exception");
    } catch (...) {
        // Exception caught, scoped_attach should have cleaned up
    }

    sig();
    REQUIRE(counter == 1);  // Handler was detached by scoped_attach destructor
}

TEST_CASE("scoped_attach with signal1", "[signal][first-principles]") {
    signal1<int> sig;
    int total = 0;

    {
        scoped_attach<signal1<int>> sa(sig, [&total](int v) { total += v; });
        sig(10);
        sig(20);
        REQUIRE(total == 30);
    }

    sig(100);
    REQUIRE(total == 30);  // Handler detached
}

TEST_CASE("multiple handlers receive same emission", "[signal][first-principles]") {
    // First principle: all attached handlers should receive each emission
    signal1<int> sig;
    int count1 = 0, count2 = 0, count3 = 0;

    void* h1 = sig.attach([&count1](int) { ++count1; });
    void* h2 = sig.attach([&count2](int) { ++count2; });
    void* h3 = sig.attach([&count3](int) { ++count3; });

    for (int i = 0; i < 100; ++i) {
        sig(i);
    }

    REQUIRE(count1 == 100);
    REQUIRE(count2 == 100);
    REQUIRE(count3 == 100);

    sig.detach(h1);
    sig.detach(h2);
    sig.detach(h3);
}

TEST_CASE("handler can capture and modify external state", "[signal][first-principles]") {
    // First principle: handlers are callable objects that can have state
    signal0 sig;

    struct StatefulHandler {
        int& state;
        StatefulHandler(int& s) : state(s) {}
        void operator()() { state += 10; }
    };

    int external_state = 0;
    StatefulHandler handler(external_state);

    void* h = sig.attach(std::ref(handler));

    sig();
    REQUIRE(external_state == 10);

    sig();
    REQUIRE(external_state == 20);

    sig.detach(h);
}

TEST_CASE("detaching first handler doesn't affect others", "[signal][first-principles]") {
    signal0 sig;
    std::vector<int> calls;

    void* h1 = sig.attach([&calls]() { calls.push_back(1); });
    void* h2 = sig.attach([&calls]() { calls.push_back(2); });
    void* h3 = sig.attach([&calls]() { calls.push_back(3); });

    sig.detach(h1);
    sig();

    REQUIRE(calls == std::vector<int>{2, 3});

    sig.detach(h2);
    sig.detach(h3);
}

TEST_CASE("detaching last handler doesn't affect others", "[signal][first-principles]") {
    signal0 sig;
    std::vector<int> calls;

    void* h1 = sig.attach([&calls]() { calls.push_back(1); });
    void* h2 = sig.attach([&calls]() { calls.push_back(2); });
    void* h3 = sig.attach([&calls]() { calls.push_back(3); });

    sig.detach(h3);
    sig();

    REQUIRE(calls == std::vector<int>{1, 2});

    sig.detach(h1);
    sig.detach(h2);
}

TEST_CASE("signal survives handler that throws", "[signal][first-principles]") {
    // First principle: exception in handler shouldn't corrupt signal state
    signal0 sig;
    int counter = 0;

    void* h1 = sig.attach([&counter]() { ++counter; });
    void* h2 = sig.attach([]() { throw std::runtime_error("handler error"); });
    void* h3 = sig.attach([&counter]() { ++counter; });

    // First handler runs, second throws, third may or may not run
    // But signal should remain usable
    try {
        sig();
    } catch (const std::runtime_error&) {
        // Expected
    }

    REQUIRE(counter >= 1);  // At least first handler ran

    // Detach throwing handler and verify signal still works
    sig.detach(h2);
    counter = 0;

    REQUIRE_NOTHROW(sig());
    REQUIRE(counter == 2);  // Both remaining handlers ran

    sig.detach(h1);
    sig.detach(h3);
}
