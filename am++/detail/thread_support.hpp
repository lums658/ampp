// Copyright 2010-2013 The Trustees of Indiana University.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met: 

// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer. 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution. 

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//  Authors: Jeremiah Willcock
//           Andrew Lumsdaine

#ifndef AMPLUSPLUS_DETAIL_THREAD_SUPPORT_HPP
#define AMPLUSPLUS_DETAIL_THREAD_SUPPORT_HPP

#include <mutex>
#include <condition_variable>
#include <cassert>
#include <type_traits>
#include <unordered_map>
#include <memory>

#ifdef AMPLUSPLUS_SINGLE_THREADED
namespace amplusplus {
  namespace detail {
    // No-op mutex for single-threaded mode
    struct dummy_mutex {
      void lock() {}
      void unlock() {}
      bool try_lock() { return true; }
    };

    typedef dummy_mutex recursive_mutex;
    typedef dummy_mutex mutex;

    class barrier {
      public:
      barrier(unsigned int count) {(void)count; assert (count == 1);}
      bool wait() {return true;}
    };
    inline void do_pause() {}
  }
}
#define AMPLUSPLUS_MULTITHREAD(x) /**/
#else
#include <pthread.h>
#if defined(__i386) || defined(__x86_64) && !_CRAYC
#include <xmmintrin.h>
#endif
namespace amplusplus {
  namespace detail {
    typedef std::mutex mutex;
    typedef std::recursive_mutex recursive_mutex;

    // C++17-compatible barrier implementation (std::barrier is C++20)
    class barrier {
      std::mutex mtx;
      std::condition_variable cv;
      unsigned int threshold;
      unsigned int count;
      unsigned int generation;
    public:
      explicit barrier(unsigned int count)
        : threshold(count), count(count), generation(0) {}

      bool wait() {
        std::unique_lock<std::mutex> lock(mtx);
        unsigned int gen = generation;
        if (--count == 0) {
          generation++;
          count = threshold;
          cv.notify_all();
          return true;
        }
        cv.wait(lock, [this, gen] { return gen != generation; });
        return false;
      }
    };

#if defined(__i386) || defined(__x86_64) && !_CRAYC
    inline void do_pause() {_mm_pause();}
#else
    inline void do_pause() {}
#endif

  }
}
#define AMPLUSPLUS_MULTITHREAD(x) x
#endif

#ifdef AMPLUSPLUS_SINGLE_THREADED
namespace amplusplus {namespace detail {
template <typename T>
class atomic {
  T value;
  public:
  atomic(T x = T()): value(x) {}
  T load() const {return value;}
  void store(T x) {value = x;}
  T exchange(T x) {T old_value = value; value = x; return old_value;}
  bool compare_exchange_strong(T& old_value, T new_value) {if (value == old_value) {value = new_value; return true;} else {old_value = value; return false;}}
  bool compare_exchange_weak(T& old_value, T new_value) {if (value == old_value) {value = new_value; return true;} else {old_value = value; return false;}}
  T fetch_add(T x) {value += x; return value - x;}
  T fetch_sub(T x) {value -= x; return value + x;}
  T fetch_or(T x) {T old_value = value; value |= x; return old_value;}
  T fetch_and(T x) {T old_value = value; value &= x; return old_value;}
  atomic& operator++() {fetch_add(1); return *this;}
  atomic& operator--() {fetch_add(-1); return *this;}
  atomic& operator+=(T x) {value += x; return *this;}
  atomic& operator-=(T x) {value -= x; return *this;}
};
}}
#elif 1
#ifdef AMPLUSPLUS_BUILTIN_ATOMICS
#include <atomic>
#else
#include <boost/atomic.hpp>
#endif
// #include <cstdatomic>
namespace amplusplus {namespace detail {
#ifdef AMPLUSPLUS_BUILTIN_ATOMICS
using std::atomic;
#else
using boost::atomic;
#endif
}}
#elif 1
namespace amplusplus {namespace detail {

template <typename T, typename = void> struct atomics_supported: std::false_type {};

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_1
template <typename T> struct atomics_supported<T, typename std::enable_if_t<sizeof(T) == 1>::type>: std::true_type {};
#endif

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_2
template <typename T> struct atomics_supported<T, typename std::enable_if_t<sizeof(T) == 2>::type>: std::true_type {};
#endif

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
template <typename T> struct atomics_supported<T, typename std::enable_if_t<sizeof(T) == 4>::type>: std::true_type {};
#endif

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
template <typename T> struct atomics_supported<T, typename std::enable_if_t<sizeof(T) == 8>::type>: std::true_type {};
#endif

#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16
template <typename T> struct atomics_supported<T, typename std::enable_if_t<sizeof(T) == 16>::type>: std::true_type {};
#endif

template <typename T>
class atomic {
  static_assert(atomics_supported<T>::value, "Atomics not supported for this type size");

  volatile T value;
  public:
  atomic(T x = T()): value(x) {}
  T load() const {__sync_synchronize(); T val = value; __sync_synchronize(); return val;}
  void store(T x) {value = x; __sync_synchronize();}
  T exchange(T x) {
    T prev = 0, prev_old = 0;
    do {
      prev_old = prev;
      prev = __sync_val_compare_and_swap(&value, prev_old, x);
    } while (prev != prev_old);
    return prev;
  }
  bool compare_exchange_strong(T& old_value, T new_value) {
    T prev = __sync_val_compare_and_swap(&value, old_value, new_value);
    if (prev == old_value) return true;
    old_value = prev;
    return false;
  }
  bool compare_exchange_weak(T& old_value, T new_value) {return compare_exchange_strong(old_value, new_value);}
  atomic<T>& operator+=(T x) {fetch_add(x); return *this;}
  atomic<T>& operator-=(T x) {fetch_sub(x); return *this;}
  T fetch_add(T x) {T val = __sync_fetch_and_add(&value, x); return val;}
  T fetch_sub(T x) {T val = __sync_fetch_and_sub(&value, x); return val;}
  T fetch_or(T x) {T val = __sync_fetch_and_or(&value, x); return val;}
  T fetch_and(T x) {T val = __sync_fetch_and_and(&value, x); return val;}
  atomic& operator++() {*this += 1; return *this;}
  atomic& operator--() {*this -= 1; return *this;}
};
}}
#else
// Lock-based version for thread debugging tools
namespace amplusplus {namespace detail {
template <typename T>
class atomic {
  mutable std::mutex lock;
  T value;
  public:
  atomic(const atomic&) = delete;
  atomic& operator=(const atomic&) = delete;

  atomic(T x = T()): lock(), value(x) {}
  T load() const {std::lock_guard<std::mutex> l(lock); return value;}
  void store(T x) {std::lock_guard<std::mutex> l(lock); value = x;}
  T exchange(T x) {
    std::lock_guard<std::mutex> l(lock);
    std::swap(value, x);
    return x;
  }
  bool compare_exchange_strong(T& old_value, T new_value) {
    std::lock_guard<std::mutex> l(lock);
    if (value == old_value) {
      value = new_value;
      return true;
    } else {
      old_value = value;
      return false;
    }
  }
  bool compare_exchange_weak(T& old_value, T new_value) {return compare_exchange_strong(old_value, new_value);}
  T fetch_add(T x) {std::lock_guard<std::mutex> l(lock); T old_value = value; value += x; return old_value;}
  T fetch_sub(T x) {return fetch_add(-x);}
  T fetch_or(T x) {std::lock_guard<std::mutex> l(lock); T old_value = value; value |= x; return old_value;}
  T fetch_and(T x) {std::lock_guard<std::mutex> l(lock); T old_value = value; value &= x; return old_value;}
  atomic& operator++() {fetch_add(1); return *this;}
  atomic& operator--() {fetch_add(-1); return *this;}
  atomic& operator+=(T x) {fetch_add(x); return *this;}
  atomic& operator-=(T x) {fetch_sub(x); return *this;}
};
}}
#endif

namespace amplusplus {
  namespace detail {

    // Replacement for boost::thread_specific_ptr using thread_local storage
    // Provides per-thread, per-instance storage for class members
    template <typename T>
    class thread_local_ptr {
      using storage_map = std::unordered_map<const thread_local_ptr*, std::unique_ptr<T>>;
      static storage_map& get_storage() {
        static thread_local storage_map storage;
        return storage;
      }

    public:
      using element_type = T;

      explicit thread_local_ptr(void (*cleanup)(T*) = nullptr) noexcept
        : cleanup_(cleanup) {}

      ~thread_local_ptr() {
        // Note: destructor only cleans up in the calling thread
        // Other threads' data will be cleaned when those threads exit
        get_storage().erase(this);
      }

      thread_local_ptr(const thread_local_ptr&) = delete;
      thread_local_ptr& operator=(const thread_local_ptr&) = delete;

      T* get() const {
        auto& storage = get_storage();
        auto it = storage.find(this);
        return it != storage.end() ? it->second.get() : nullptr;
      }

      T* operator->() const { return get(); }
      T& operator*() const { return *get(); }

      void reset(T* p = nullptr) {
        auto& storage = get_storage();
        if (p) {
          storage[this] = std::unique_ptr<T>(p);
        } else {
          storage.erase(this);
        }
      }

      T* release() {
        auto& storage = get_storage();
        auto it = storage.find(this);
        if (it != storage.end()) {
          T* p = it->second.release();
          storage.erase(it);
          return p;
        }
        return nullptr;
      }

    private:
      void (*cleanup_)(T*);  // Not used but kept for API compatibility
    };

    extern __thread int internal_thread_id;

    static inline int get_thread_id() {
      assert (internal_thread_id != -1); // Ensure that it has been set
      return internal_thread_id;
    }

    class push_thread_id_obj {
      int old_id;

      public:
      push_thread_id_obj(int new_id) // Can't be explicit
        : old_id(internal_thread_id)
        {internal_thread_id = new_id;}
      ~push_thread_id_obj() {internal_thread_id = old_id;}
      operator bool() const {return false;} // For use in if statements
    };

    // Helper macro for token concatenation
    #define AMPLUSPLUS_PP_CAT_IMPL(a, b) a##b
    #define AMPLUSPLUS_PP_CAT(a, b) AMPLUSPLUS_PP_CAT_IMPL(a, b)
    #define AMPLUSPLUS_WITH_THREAD_ID(id) if (::amplusplus::detail::push_thread_id_obj AMPLUSPLUS_PP_CAT(tidobj_, __LINE__) = (id)) {} else
  }
}

#endif // AMPLUSPLUS_DETAIL_THREAD_SUPPORT_HPP
