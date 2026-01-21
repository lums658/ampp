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
#include <barrier>
#include <atomic>
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
#if defined(__i386__) || defined(__x86_64__) && !defined(_CRAYC)
#include <xmmintrin.h>
#endif
namespace amplusplus {
  namespace detail {
    typedef std::mutex mutex;
    typedef std::recursive_mutex recursive_mutex;

    // C++20 std::barrier wrapper with compatible API
    class barrier {
      std::barrier<> impl;
    public:
      explicit barrier(unsigned int count) : impl(count) {}

      bool wait() {
        impl.arrive_and_wait();
        return true;  // Return value not used, kept for API compatibility
      }
    };

#if defined(__i386__) || defined(__x86_64__) && !defined(_CRAYC)
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
// No-op atomic for single-threaded mode
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
#else
// Use std::atomic for multi-threaded mode (C++20)
namespace amplusplus {namespace detail {
using std::atomic;
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
