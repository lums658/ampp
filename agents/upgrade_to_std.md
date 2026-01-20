# AM++ Modernization Plan: Boost to C++ Standard Library Migration

## Overview

The AM++ repository contains a high-performance asynchronous active-messages runtime. Much of it was written using Boost (minimum version 1.52) and targets C++98/03 with optional C++11 support. Many Boost features have since become part of the C++ standard library.

### Goals
1. Create a CMake build infrastructure to replace the current Autotools system
2. Upgrade AM++ to use the C++ standard library instead of Boost
3. Remove remaining Boost vestiges
4. Migrate from libnbc to MPI-3 non-blocking collectives

### Target C++ Standard: **C++17** (minimum)
This provides access to all necessary standard library replacements for the Boost features used.

---

## Codebase Statistics

- **Total Lines**: ~9,206 lines (headers + sources)
- **Header Files**: 41 (24 main + 17 detail)
- **Source Files**: 7 implementation files
- **Test Programs**: 11 executables
- **Boost Headers Used**: 40+ across multiple categories

---

## Phase 1: CMake Build System Migration

### 1.1 Create Core CMake Infrastructure

Create the following files:

```
CMakeLists.txt                    # Root CMake file
cmake/
├── FindNBC.cmake                 # NBC library detection (temporary)
├── AmppConfig.cmake.in           # Package config template
└── AmppOptions.cmake             # Build options
am++/CMakeLists.txt               # Header installation
src/CMakeLists.txt                # Library build
tests/CMakeLists.txt              # Test executables
```

### 1.2 Root CMakeLists.txt Structure

```cmake
cmake_minimum_required(VERSION 3.16)
project(ampp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(AMPP_ENABLE_THREADING "Enable threading support" ON)
option(AMPP_THREADING_MODEL "Threading model: serialized|multiple" "serialized")
option(AMPP_ENABLE_SELFCHECK "Enable self-checking" OFF)
option(AMPP_ENABLE_DEBUGGING "Enable debugging features" OFF)
option(AMPP_USE_MPI3_NBC "Use MPI-3 non-blocking collectives instead of libnbc" ON)
option(BUILD_TESTING "Build test programs" ON)

# Find dependencies
find_package(MPI REQUIRED)
find_package(Threads REQUIRED)

# NBC handling - prefer MPI-3 NBC
if(NOT AMPP_USE_MPI3_NBC)
    find_package(NBC QUIET)
    if(NOT NBC_FOUND)
        message(STATUS "NBC not found, using stub implementation")
        add_subdirectory(libnbc-stub)
    endif()
endif()

# Configure header
configure_file(config.h.cmake.in config.h @ONLY)

# Subdirectories
add_subdirectory(src)
add_subdirectory(am++)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(CMakePackageConfigHelpers)
# ... package config generation
```

### 1.3 Library Target (src/CMakeLists.txt)

```cmake
add_library(ampp
    mpi_make_mpi_datatype.cpp
    mpi_sinha_kale_ramkumar_termination_detector.cpp
    mpi_sinha_kale_ramkumar_termination_detector_bgp.cpp
    mpi_transport.cpp
    termination_detector.cpp
    thread_support.cpp
    transport.cpp
)

target_include_directories(ampp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(ampp
    PUBLIC
        MPI::MPI_CXX
        Threads::Threads
)

target_compile_definitions(ampp
    PRIVATE
        $<$<BOOL:${AMPP_ENABLE_THREADING}>:AMPLUSPLUS_USE_THREAD_${AMPP_THREADING_MODEL}>
        $<$<BOOL:${AMPP_ENABLE_SELFCHECK}>:AMPLUSPLUS_ENABLE_SELFCHECK>
        $<$<BOOL:${AMPP_USE_MPI3_NBC}>:AMPLUSPLUS_USE_MPI3_NBC>
)
```

### 1.4 CMake Migration Checklist

- [ ] Create root `CMakeLists.txt`
- [ ] Create `cmake/` directory with helper modules
- [ ] Create `config.h.cmake.in` (convert from `config.h.in`)
- [ ] Create `src/CMakeLists.txt` for library
- [ ] Create `am++/CMakeLists.txt` for header installation
- [ ] Create `tests/CMakeLists.txt` with test targets
- [ ] Add CPack configuration for packaging
- [ ] Add `find_package(ampp)` support
- [ ] Test build on Linux/macOS
- [ ] Document build instructions

---

## Phase 2: libnbc to MPI-3 Non-Blocking Collectives Migration

### 2.1 Background

MPI-3 (standardized in 2012) introduced non-blocking collective operations directly into the MPI standard, making the separate libnbc library obsolete. All modern MPI implementations (OpenMPI 1.7+, MPICH 3.0+, Intel MPI 5.0+) support MPI-3 NBC.

### 2.2 API Mapping

| libnbc Function | MPI-3 Equivalent |
|-----------------|------------------|
| `NBC_Ibcast()` | `MPI_Ibcast()` |
| `NBC_Ireduce()` | `MPI_Ireduce()` |
| `NBC_Iallreduce()` | `MPI_Iallreduce()` |
| `NBC_Igather()` | `MPI_Igather()` |
| `NBC_Igatherv()` | `MPI_Igatherv()` |
| `NBC_Iscatter()` | `MPI_Iscatter()` |
| `NBC_Iscatterv()` | `MPI_Iscatterv()` |
| `NBC_Iallgather()` | `MPI_Iallgather()` |
| `NBC_Iallgatherv()` | `MPI_Iallgatherv()` |
| `NBC_Ialltoall()` | `MPI_Ialltoall()` |
| `NBC_Ialltoallv()` | `MPI_Ialltoallv()` |
| `NBC_Ibarrier()` | `MPI_Ibarrier()` |
| `NBC_Iscan()` | `MPI_Iscan()` |
| `NBC_Iexscan()` | `MPI_Iexscan()` |
| `NBC_Wait()` | `MPI_Wait()` |
| `NBC_Test()` | `MPI_Test()` |

### 2.3 Key Differences

1. **Handle Types**:
   ```c
   // libnbc
   NBC_Handle handle;
   NBC_Ibcast(buffer, count, datatype, root, comm, &handle);
   NBC_Wait(&handle, MPI_STATUS_IGNORE);

   // MPI-3
   MPI_Request request;
   MPI_Ibcast(buffer, count, datatype, root, comm, &request);
   MPI_Wait(&request, MPI_STATUS_IGNORE);
   ```

2. **Initialization**: libnbc may require `NBC_Init()` - MPI-3 does not

3. **Schedule-based operations**: libnbc supports custom schedules which MPI-3 doesn't directly support (use `MPI_Comm_dup` with info hints instead)

### 2.4 Files to Modify

Search for NBC usage and update:
- [ ] `am++/detail/` headers using NBC
- [ ] `src/` implementation files
- [ ] `tests/` test programs
- [ ] Remove `libnbc-stub/` directory

### 2.5 Migration Strategy

```cpp
// Create a compatibility layer during migration
#ifdef AMPLUSPLUS_USE_MPI3_NBC
    // Use MPI-3 directly
    #define AMPP_NBC_Ibcast MPI_Ibcast
    #define AMPP_NBC_Wait MPI_Wait
    // ... etc
    typedef MPI_Request AMPP_NBC_Handle;
#else
    // Legacy libnbc support
    #define AMPP_NBC_Ibcast NBC_Ibcast
    #define AMPP_NBC_Wait NBC_Wait
    typedef NBC_Handle AMPP_NBC_Handle;
#endif
```

### 2.6 NBC Migration Checklist

- [ ] Identify all NBC function calls in codebase
- [ ] Create MPI-3 compatibility header (optional, for gradual migration)
- [ ] Replace `NBC_Handle` with `MPI_Request`
- [ ] Replace NBC function calls with MPI-3 equivalents
- [ ] Remove NBC initialization calls if present
- [ ] Update CMake to detect MPI-3 NBC support
- [ ] Remove `libnbc-stub/` directory
- [ ] Update documentation
- [ ] Test with multiple MPI implementations

---

## Phase 3: Boost to Standard Library Migration

### 3.1 Direct Replacements (Low Risk)

These Boost components have direct C++17 equivalents:

| Boost | C++ Standard | Files Affected |
|-------|--------------|----------------|
| `boost::shared_ptr<T>` | `std::shared_ptr<T>` | am++.hpp, multiple |
| `boost::weak_ptr<T>` | `std::weak_ptr<T>` | am++.hpp |
| `boost::make_shared<T>` | `std::make_shared<T>` | multiple |
| `boost::function<T>` | `std::function<T>` | message_type_generators.hpp |
| `boost::bind` | `std::bind` or lambdas | multiple |
| `boost::ref` / `boost::cref` | `std::ref` / `std::cref` | multiple |
| `boost::optional<T>` | `std::optional<T>` | reductions.hpp, routing.hpp |
| `boost::any` | `std::any` | am++.hpp |
| `boost::thread` | `std::thread` | detail/mpi_request_manager.hpp |
| `boost::mutex` | `std::mutex` | detail/thread_support.hpp |
| `boost::recursive_mutex` | `std::recursive_mutex` | detail/thread_support.hpp |
| `boost::unique_lock` | `std::unique_lock` | multiple |
| `boost::lock_guard` | `std::lock_guard` | multiple |
| `boost::atomic<T>` | `std::atomic<T>` | detail/thread_support.hpp |
| `boost::noncopyable` | Deleted copy ops | multiple |
| `boost::static_assert` | `static_assert` | multiple |
| `boost::enable_if` | `std::enable_if` | multiple |
| `boost::result_of` | `std::invoke_result` | message_type_generators.hpp |
| `boost::unordered_set` | `std::unordered_set` | reductions.hpp |

#### Implementation Steps:

1. **Smart Pointers** (am++/am++.hpp, and detail headers)
   ```cpp
   // Before
   #include <boost/smart_ptr.hpp>
   boost::shared_ptr<transport> t;

   // After
   #include <memory>
   std::shared_ptr<transport> t;
   ```

2. **Function/Bind** (message_type_generators.hpp, multiple)
   ```cpp
   // Before
   #include <boost/bind.hpp>
   #include <boost/function.hpp>
   boost::function<void(int)> f = boost::bind(&Class::method, this, _1);

   // After
   #include <functional>
   std::function<void(int)> f = [this](int x) { this->method(x); };
   ```

3. **Optional** (reductions.hpp, routing.hpp)
   ```cpp
   // Before
   #include <boost/optional.hpp>
   boost::optional<T> val;

   // After
   #include <optional>
   std::optional<T> val;
   ```

4. **Threading** (detail/thread_support.hpp, detail/mpi_request_manager.hpp)
   ```cpp
   // Before
   #include <boost/thread.hpp>
   boost::mutex mtx;
   boost::thread t(func);

   // After
   #include <mutex>
   #include <thread>
   std::mutex mtx;
   std::thread t(func);
   ```

5. **Atomics** (detail/thread_support.hpp)
   ```cpp
   // Before
   #include <boost/atomic.hpp>
   boost::atomic<int> counter;

   // After
   #include <atomic>
   std::atomic<int> counter;
   ```

### 3.2 Type Traits Migration (Medium Risk)

| Boost | C++ Standard |
|-------|--------------|
| `boost::is_same` | `std::is_same_v` |
| `boost::is_base_of` | `std::is_base_of_v` |
| `boost::is_convertible` | `std::is_convertible_v` |
| `boost::remove_reference` | `std::remove_reference_t` |
| `boost::decay` | `std::decay_t` |
| `boost::add_const` | `std::add_const_t` |

**Files**: reductions.hpp, message_type_generators.hpp, detail headers

```cpp
// Before
#include <boost/type_traits.hpp>
typename boost::remove_reference<T>::type

// After
#include <type_traits>
std::remove_reference_t<T>
```

### 3.3 MPL to Modern C++ (Medium-High Risk)

Boost.MPL is heavily used for compile-time metaprogramming. Replace with:
- `constexpr if` (C++17)
- `std::conditional_t`
- `std::enable_if_t`
- `std::conjunction`, `std::disjunction`
- Fold expressions
- Template parameter packs

| Boost MPL | C++17 Equivalent |
|-----------|------------------|
| `boost::mpl::bool_<B>` | `std::bool_constant<B>` |
| `boost::mpl::int_<N>` | `std::integral_constant<int, N>` |
| `boost::mpl::and_<...>` | `std::conjunction<...>` |
| `boost::mpl::or_<...>` | `std::disjunction<...>` |
| `boost::mpl::eval_if` | `std::conditional_t` + `constexpr if` |
| `boost::mpl::has_xxx` | Custom SFINAE or concepts |
| `BOOST_MPL_ASSERT` | `static_assert` |

**Files**: reductions.hpp, message_type_generators.hpp, object_based_addressing.hpp

```cpp
// Before
#include <boost/mpl/and.hpp>
#include <boost/mpl/bool.hpp>
template<typename T>
struct check : boost::mpl::and_<is_valid<T>, is_complete<T>> {};

// After
#include <type_traits>
template<typename T>
struct check : std::conjunction<is_valid<T>, is_complete<T>> {};
```

### 3.4 Components Requiring Alternatives (High Risk)

These Boost libraries have no direct standard replacement:

#### 3.4.1 Boost.Intrusive (KEEP or find alternative)
**Files**: am++/detail/message_queue.hpp, am++/object_based_addressing.hpp

Options:
1. **Keep Boost.Intrusive** - It's highly specialized and efficient
2. **Rewrite with std containers** - May impact performance
3. **Use a header-only alternative** (e.g., from folly or custom)

**Recommendation**: Keep Boost.Intrusive or evaluate if the intrusive patterns can be simplified.

```cpp
// Currently using:
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/list.hpp>
boost::intrusive::slist<message_node> queue;
```

#### 3.4.2 Boost.Pool (KEEP or replace)
**Files**: am++/detail/mpi_pool.hpp

Options:
1. **Keep Boost.Pool** for memory pooling
2. **Use std::pmr::pool_options** (C++17 polymorphic allocators)
3. **Custom simple pool** if usage is limited

#### 3.4.3 Boost.Graph (KEEP)
**Files**: src/transport.cpp (Bellman-Ford shortest paths)

**Recommendation**: Keep Boost.Graph - no standard replacement exists.

```cpp
// Used for routing topology computation
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/bellman_ford_shortest_paths.hpp>
```

#### 3.4.4 Boost.Parameter (Evaluate removal)
**Files**: am++/message_type_generators.hpp

Options:
1. **Simplify API** to not need named parameters
2. **Use designated initializers** (C++20) with struct options
3. **Keep Boost.Parameter** if API stability is critical

#### 3.4.5 Thread-Specific Storage
**Files**: am++/detail/thread_support.hpp

```cpp
// Before
#include <boost/thread/tss.hpp>
boost::thread_specific_ptr<T> tls_data;

// After
#include <thread>
thread_local T tls_data;  // C++11 thread_local keyword
```

#### 3.4.6 Boost.Barrier
**Files**: detail/thread_support.hpp

```cpp
// Before
boost::barrier sync_point(num_threads);

// After (C++20)
#include <barrier>
std::barrier sync_point(num_threads);

// Or for C++17, implement simple barrier using condition_variable
```

### 3.5 Preprocessor and Formatting

| Boost | Replacement |
|-------|-------------|
| `boost/preprocessor.hpp` | Keep or replace with constexpr functions |
| `boost/format.hpp` | `std::format` (C++20) or `fmt::format` |

---

## Phase 4: Implementation Order

### Stage 1: Foundation (Week 1-2)
1. [ ] Create CMake build system alongside Autotools
2. [ ] Verify both build systems produce equivalent output
3. [ ] Set up CI to test both build systems

### Stage 2: NBC Migration (Week 3)
1. [ ] Audit NBC usage in codebase
2. [ ] Create MPI-3 NBC compatibility layer
3. [ ] Replace NBC calls with MPI-3 equivalents
4. [ ] Remove libnbc-stub directory
5. [ ] Test with OpenMPI and MPICH

### Stage 3: Low-Risk Boost Migrations (Week 4-5)
1. [ ] Replace `boost::shared_ptr` → `std::shared_ptr`
2. [ ] Replace `boost::function` → `std::function`
3. [ ] Replace `boost::bind` → lambdas
4. [ ] Replace `boost::optional` → `std::optional`
5. [ ] Replace `boost::thread/mutex` → `std::thread/mutex`
6. [ ] Replace `boost::atomic` → `std::atomic`
7. [ ] Replace `boost::noncopyable` → deleted copy operations
8. [ ] Replace `boost::static_assert` → `static_assert`
9. [ ] Replace `boost::unordered_set` → `std::unordered_set`

### Stage 4: Type Traits and Enable_If (Week 6)
1. [ ] Replace `boost/type_traits.hpp` → `<type_traits>`
2. [ ] Replace `boost::enable_if` → `std::enable_if_t`
3. [ ] Replace `boost::result_of` → `std::invoke_result_t`

### Stage 5: MPL Migration (Week 7-8)
1. [ ] Replace `boost::mpl::bool_` → `std::bool_constant`
2. [ ] Replace `boost::mpl::and_/or_` → `std::conjunction/disjunction`
3. [ ] Replace `boost::mpl::eval_if` → `std::conditional_t` + `if constexpr`
4. [ ] Replace custom `has_xxx` traits with modern SFINAE or concepts

### Stage 6: Threading Refinements (Week 9)
1. [ ] Replace `boost::thread_specific_ptr` → `thread_local`
2. [ ] Replace `boost::barrier` → `std::barrier` (C++20) or custom
3. [ ] Remove `BOOST_SP_DISABLE_THREADS` usage

### Stage 7: Evaluate Remaining Boost (Week 10-11)
1. [ ] Evaluate Boost.Intrusive usage - keep or migrate
2. [ ] Evaluate Boost.Pool usage - keep or use `std::pmr`
3. [ ] Keep Boost.Graph (no replacement)
4. [ ] Evaluate Boost.Parameter - simplify or keep
5. [ ] Replace `boost::format` → `std::format` or `fmt`

### Stage 8: Cleanup and Testing (Week 12)
1. [ ] Remove all unused Boost includes
2. [ ] Update all `#include` paths
3. [ ] Remove Autotools files
4. [ ] Run full test suite
5. [ ] Performance benchmarking
6. [ ] Update documentation

---

## Phase 5: File-by-File Migration Guide

### Core Headers

| File | Boost Dependencies | Migration Complexity |
|------|-------------------|---------------------|
| `am++/am++.hpp` | shared_ptr, function, bind, noncopyable, any | Low |
| `am++/transport.hpp` | shared_ptr, function, noncopyable | Low |
| `am++/message_queue.hpp` | intrusive, function | Medium (intrusive) |
| `am++/reductions.hpp` | optional, unordered_set, mpl, type_traits | Medium |
| `am++/message_type_generators.hpp` | parameter, typeof, result_of, mpl | High |
| `am++/object_based_addressing.hpp` | property_map, intrusive, graph | High |
| `am++/routing.hpp` | optional, shared_ptr | Low |
| `am++/mpi_transport.hpp` | shared_ptr, function | Low |

### Detail Headers

| File | Boost Dependencies | Migration Complexity |
|------|-------------------|---------------------|
| `am++/detail/thread_support.hpp` | thread, mutex, atomic, signals2, tss | Medium |
| `am++/detail/mpi_pool.hpp` | pool | Medium (evaluate) |
| `am++/detail/mpi_request_manager.hpp` | thread, bind, function | Low |
| `am++/detail/signal_and_slot.hpp` | function, bind | Low |
| `am++/detail/buffer_cache.hpp` | shared_ptr | Low |

### Source Files

| File | Boost Dependencies | Migration Complexity |
|------|-------------------|---------------------|
| `src/transport.cpp` | graph (Bellman-Ford) | Keep Boost.Graph |
| `src/thread_support.cpp` | thread | Low |
| `src/mpi_transport.cpp` | shared_ptr, function | Low |

---

## Phase 6: Testing Strategy

### 6.1 Unit Testing
- Compile each header independently
- Verify template instantiation works
- Test thread safety

### 6.2 Integration Testing
- Run existing test suite after each migration stage
- Compare output with pre-migration baseline

### 6.3 Performance Testing
- Benchmark message throughput
- Measure memory usage
- Profile threading overhead

### 6.4 Platform Testing
- Linux (GCC 9+, Clang 10+)
- macOS (Apple Clang)
- HPC systems (Cray, IBM)

---

## Summary of Dependencies After Migration

### Will Remove Completely
- **libnbc** → MPI-3 non-blocking collectives
- `boost/smart_ptr.hpp` → `<memory>`
- `boost/function.hpp` → `<functional>`
- `boost/bind.hpp` → lambdas
- `boost/optional.hpp` → `<optional>`
- `boost/any.hpp` → `<any>`
- `boost/thread.hpp` → `<thread>`, `<mutex>`
- `boost/atomic.hpp` → `<atomic>`
- `boost/type_traits.hpp` → `<type_traits>`
- `boost/static_assert.hpp` → `static_assert`
- `boost/unordered_set.hpp` → `<unordered_set>`
- Most of `boost/mpl/` → Modern metaprogramming

### May Keep (No Good Replacement)
- `boost/intrusive/` - Evaluate performance impact
- `boost/graph/` - Required for routing algorithms
- `boost/pool/` - Evaluate `std::pmr` as alternative
- `boost/parameter.hpp` - Evaluate API simplification

### Requires C++20 for Full Replacement
- `boost::barrier` → `std::barrier`
- `boost::format` → `std::format`

---

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Performance regression | Benchmark before/after each stage |
| API breakage | Maintain backward-compatible aliases initially |
| Template error messages | Test compilation early and often |
| Threading bugs | Extensive concurrent testing |
| Platform compatibility | CI testing on target platforms |
| MPI-3 availability | Verify target systems have MPI-3 support |

---

## Final Dependencies After Migration

**Required:**
- MPI-3 compliant implementation (OpenMPI 1.7+, MPICH 3.0+, Intel MPI 5.0+)
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- POSIX Threads

**Optional (may keep):**
- Boost.Graph (for routing algorithms)
- Boost.Intrusive (for high-performance queues)
- Boost.Pool (for memory pooling)

---

## Notes

- The codebase is approximately 9,200 lines total
- Consider keeping Boost.Graph and possibly Boost.Intrusive
- Target C++17 minimum; C++20 for some features (barrier, format)
- Migration can be done incrementally with both build systems active
- MPI-3 NBC has been standard since 2012 - all modern MPI implementations support it