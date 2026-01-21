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

#ifndef AMPLUSPLUS_DETAIL_TYPED_IN_PLACE_FACTORY_OWNING_HPP
#define AMPLUSPLUS_DETAIL_TYPED_IN_PLACE_FACTORY_OWNING_HPP

#include <tuple>
#include <utility>
#include <type_traits>
#include <new>

// Like boost::typed_in_place_factoryN, but owning the data used for
// construction to avoid dangling references.
// Now implemented using C++ variadic templates instead of Boost.Preprocessor.

namespace amplusplus {
  namespace detail {
    // Base class for type identification (no longer inherits from Boost)
    struct typed_in_place_factory_owning_base {};

    // Primary variadic template for typed_in_place_factory_owning
    template <typename Obj, typename... Args>
    class typed_in_place_factory_owning : public typed_in_place_factory_owning_base {
      std::tuple<std::decay_t<Args>...> args;

      template <std::size_t... Is>
      void apply_impl(void* obj, std::index_sequence<Is...>) const {
        new(obj) Obj(std::get<Is>(args)...);
      }

    public:
      typedef Obj value_type;

      explicit typed_in_place_factory_owning(Args... a)
        : args(std::forward<Args>(a)...) {}

      void apply(void* obj) const {
        apply_impl(obj, std::index_sequence_for<Args...>{});
      }

      // Provide access to individual arguments for backward compatibility
      // with factory_wrapper.hpp which accesses f.a0, f.a1, etc.
      template <std::size_t I>
      const auto& get() const { return std::get<I>(args); }

      // Get the stored tuple for use in constructors
      const std::tuple<std::decay_t<Args>...>& get_args() const { return args; }
    };

    // Specialization for zero arguments
    template <typename Obj>
    class typed_in_place_factory_owning<Obj> : public typed_in_place_factory_owning_base {
    public:
      typedef Obj value_type;

      typed_in_place_factory_owning() = default;

      void apply(void* obj) const {
        new(obj) Obj();
      }

      const std::tuple<>& get_args() const {
        static std::tuple<> empty;
        return empty;
      }
    };

    // Helper function to create typed_in_place_factory_owning
    template <typename Obj, typename... Args>
    auto make_typed_in_place_factory_owning(Args&&... args) {
      return typed_in_place_factory_owning<Obj, std::decay_t<Args>...>(std::forward<Args>(args)...);
    }

    // Legacy aliases for backward compatibility with numbered factory classes
    // These allow code using typed_in_place_factory_owning0, typed_in_place_factory_owning1, etc. to still work
    template <typename Obj>
    using typed_in_place_factory_owning0 = typed_in_place_factory_owning<Obj>;

    template <typename Obj, typename A0>
    using typed_in_place_factory_owning1 = typed_in_place_factory_owning<Obj, A0>;

    template <typename Obj, typename A0, typename A1>
    using typed_in_place_factory_owning2 = typed_in_place_factory_owning<Obj, A0, A1>;

    template <typename Obj, typename A0, typename A1, typename A2>
    using typed_in_place_factory_owning3 = typed_in_place_factory_owning<Obj, A0, A1, A2>;

    template <typename Obj, typename A0, typename A1, typename A2, typename A3>
    using typed_in_place_factory_owning4 = typed_in_place_factory_owning<Obj, A0, A1, A2, A3>;

    template <typename Obj, typename A0, typename A1, typename A2, typename A3, typename A4>
    using typed_in_place_factory_owning5 = typed_in_place_factory_owning<Obj, A0, A1, A2, A3, A4>;

    template <typename Obj, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
    using typed_in_place_factory_owning6 = typed_in_place_factory_owning<Obj, A0, A1, A2, A3, A4, A5>;

    template <typename Obj, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6>
    using typed_in_place_factory_owning7 = typed_in_place_factory_owning<Obj, A0, A1, A2, A3, A4, A5, A6>;

    template <typename Obj, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7>
    using typed_in_place_factory_owning8 = typed_in_place_factory_owning<Obj, A0, A1, A2, A3, A4, A5, A6, A7>;

    template <typename Obj, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8>
    using typed_in_place_factory_owning9 = typed_in_place_factory_owning<Obj, A0, A1, A2, A3, A4, A5, A6, A7, A8>;

    template <typename Obj, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9>
    using typed_in_place_factory_owning10 = typed_in_place_factory_owning<Obj, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9>;
  }
}

#endif
