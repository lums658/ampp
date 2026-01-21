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

#ifndef AMPLUSPLUS_DETAIL_FACTORY_WRAPPER_HPP
#define AMPLUSPLUS_DETAIL_FACTORY_WRAPPER_HPP

#include <am++/detail/typed_in_place_factory_owning.hpp>
#include <type_traits>
#include <utility>
#include <tuple>

namespace amplusplus {
namespace detail {

// Helper to construct underlying type from tuple of arguments
template <typename T, typename Tuple, std::size_t... Is>
T construct_from_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
  return T(std::get<Is>(t)...);
}

template <typename T, typename... Args>
T construct_from_tuple(const std::tuple<Args...>& t) {
  return construct_from_tuple_impl<T>(t, std::index_sequence_for<Args...>{});
}

} // namespace detail
} // namespace amplusplus

// Macro to create wrapper classes with variadic forwarding constructors
// and factory constructors.
//
// Usage: AMPLUSPLUS_MAKE_WRAPPER(wrapper_name, (TParam1)(TParam2)...)
// where wrapper_name_cls is the underlying type to wrap

#define AMPLUSPLUS_MAKE_WRAPPER(type, tparam_seq) \
AMPLUSPLUS_MAKE_WRAPPER_IMPL(type, tparam_seq)

// Helper macros for sequence processing
#define AMPLUSPLUS_SEQ_SIZE(seq) AMPLUSPLUS_SEQ_SIZE_I(seq)
#define AMPLUSPLUS_SEQ_SIZE_I(seq) AMPLUSPLUS_FW_CAT(AMPLUSPLUS_SEQ_SIZE_, AMPLUSPLUS_SEQ_SIZE_0 seq)
#define AMPLUSPLUS_SEQ_SIZE_0(...) AMPLUSPLUS_SEQ_SIZE_1
#define AMPLUSPLUS_SEQ_SIZE_1(...) AMPLUSPLUS_SEQ_SIZE_2
#define AMPLUSPLUS_SEQ_SIZE_2(...) AMPLUSPLUS_SEQ_SIZE_3
#define AMPLUSPLUS_SEQ_SIZE_3(...) AMPLUSPLUS_SEQ_SIZE_4
#define AMPLUSPLUS_SEQ_SIZE_4(...) AMPLUSPLUS_SEQ_SIZE_5
#define AMPLUSPLUS_SEQ_SIZE_5(...) AMPLUSPLUS_SEQ_SIZE_6
#define AMPLUSPLUS_SEQ_SIZE_AMPLUSPLUS_SEQ_SIZE_0 0
#define AMPLUSPLUS_SEQ_SIZE_AMPLUSPLUS_SEQ_SIZE_1 1
#define AMPLUSPLUS_SEQ_SIZE_AMPLUSPLUS_SEQ_SIZE_2 2
#define AMPLUSPLUS_SEQ_SIZE_AMPLUSPLUS_SEQ_SIZE_3 3
#define AMPLUSPLUS_SEQ_SIZE_AMPLUSPLUS_SEQ_SIZE_4 4
#define AMPLUSPLUS_SEQ_SIZE_AMPLUSPLUS_SEQ_SIZE_5 5
#define AMPLUSPLUS_SEQ_SIZE_AMPLUSPLUS_SEQ_SIZE_6 6

#define AMPLUSPLUS_FW_CAT(a, b) AMPLUSPLUS_FW_CAT_I(a, b)
#define AMPLUSPLUS_FW_CAT_I(a, b) a ## b

// Check if sequence is empty (size 0)
#define AMPLUSPLUS_SEQ_IS_EMPTY(seq) AMPLUSPLUS_FW_CAT(AMPLUSPLUS_SEQ_IS_EMPTY_, AMPLUSPLUS_SEQ_SIZE(seq))
#define AMPLUSPLUS_SEQ_IS_EMPTY_0 1
#define AMPLUSPLUS_SEQ_IS_EMPTY_1 0
#define AMPLUSPLUS_SEQ_IS_EMPTY_2 0
#define AMPLUSPLUS_SEQ_IS_EMPTY_3 0
#define AMPLUSPLUS_SEQ_IS_EMPTY_4 0
#define AMPLUSPLUS_SEQ_IS_EMPTY_5 0
#define AMPLUSPLUS_SEQ_IS_EMPTY_6 0

// Transform sequence elements to typename prefix
#define AMPLUSPLUS_SEQ_ENUM_TYPENAME(seq) AMPLUSPLUS_FW_CAT(AMPLUSPLUS_SEQ_ENUM_TYPENAME_, AMPLUSPLUS_SEQ_SIZE(seq))(seq)
#define AMPLUSPLUS_SEQ_ENUM_TYPENAME_0(seq)
#define AMPLUSPLUS_SEQ_ENUM_TYPENAME_1(seq) typename AMPLUSPLUS_SEQ_HEAD(seq)
#define AMPLUSPLUS_SEQ_ENUM_TYPENAME_2(seq) typename AMPLUSPLUS_SEQ_HEAD(seq), AMPLUSPLUS_SEQ_ENUM_TYPENAME_1(AMPLUSPLUS_SEQ_TAIL(seq))
#define AMPLUSPLUS_SEQ_ENUM_TYPENAME_3(seq) typename AMPLUSPLUS_SEQ_HEAD(seq), AMPLUSPLUS_SEQ_ENUM_TYPENAME_2(AMPLUSPLUS_SEQ_TAIL(seq))
#define AMPLUSPLUS_SEQ_ENUM_TYPENAME_4(seq) typename AMPLUSPLUS_SEQ_HEAD(seq), AMPLUSPLUS_SEQ_ENUM_TYPENAME_3(AMPLUSPLUS_SEQ_TAIL(seq))

// Enumerate sequence elements
#define AMPLUSPLUS_SEQ_ENUM(seq) AMPLUSPLUS_FW_CAT(AMPLUSPLUS_SEQ_ENUM_, AMPLUSPLUS_SEQ_SIZE(seq))(seq)
#define AMPLUSPLUS_SEQ_ENUM_0(seq)
#define AMPLUSPLUS_SEQ_ENUM_1(seq) AMPLUSPLUS_SEQ_HEAD(seq)
#define AMPLUSPLUS_SEQ_ENUM_2(seq) AMPLUSPLUS_SEQ_HEAD(seq), AMPLUSPLUS_SEQ_ENUM_1(AMPLUSPLUS_SEQ_TAIL(seq))
#define AMPLUSPLUS_SEQ_ENUM_3(seq) AMPLUSPLUS_SEQ_HEAD(seq), AMPLUSPLUS_SEQ_ENUM_2(AMPLUSPLUS_SEQ_TAIL(seq))
#define AMPLUSPLUS_SEQ_ENUM_4(seq) AMPLUSPLUS_SEQ_HEAD(seq), AMPLUSPLUS_SEQ_ENUM_3(AMPLUSPLUS_SEQ_TAIL(seq))

// Head and tail of sequence
#define AMPLUSPLUS_SEQ_HEAD(seq) AMPLUSPLUS_SEQ_HEAD_I(seq)
#define AMPLUSPLUS_SEQ_HEAD_I(seq) AMPLUSPLUS_SEQ_HEAD_II seq
#define AMPLUSPLUS_SEQ_HEAD_II(x) x

#define AMPLUSPLUS_SEQ_TAIL(seq) AMPLUSPLUS_SEQ_TAIL_I(seq)
#define AMPLUSPLUS_SEQ_TAIL_I(seq) AMPLUSPLUS_SEQ_TAIL_II seq
#define AMPLUSPLUS_SEQ_TAIL_II(x)

// Conditional based on whether sequence has template params
#define AMPLUSPLUS_IF_HAS_TPARAMS(seq, then_clause, else_clause) \
  AMPLUSPLUS_FW_CAT(AMPLUSPLUS_IF_HAS_TPARAMS_, AMPLUSPLUS_SEQ_IS_EMPTY(seq))(then_clause, else_clause)
#define AMPLUSPLUS_IF_HAS_TPARAMS_0(then_clause, else_clause) then_clause
#define AMPLUSPLUS_IF_HAS_TPARAMS_1(then_clause, else_clause) else_clause

// Main implementation macro
#define AMPLUSPLUS_MAKE_WRAPPER_IMPL(type, tparam_seq) \
AMPLUSPLUS_IF_HAS_TPARAMS(tparam_seq, \
  template <AMPLUSPLUS_SEQ_ENUM_TYPENAME(tparam_seq)>, \
  /* no template declaration */) \
class type { \
public: \
  typedef AMPLUSPLUS_FW_CAT(type, _cls) AMPLUSPLUS_IF_HAS_TPARAMS(tparam_seq, \
    <AMPLUSPLUS_SEQ_ENUM(tparam_seq)>, \
    /* no template args */) underlying_type; \
  typedef amplusplus::message_type_traits<underlying_type> traits; \
  type(type&&) = default; \
  AMPLUSPLUS_MAKE_WRAPPER_BODY(type) \
};

#define AMPLUSPLUS_MAKE_WRAPPER_BODY(type_) \
public: \
  underlying_type underlying; \
  \
  type_(): underlying() {} \
  \
  /* Variadic forwarding constructor - excludes factory types */ \
  template <typename Arg, typename... Args, \
            typename = typename std::enable_if< \
              !std::is_base_of<amplusplus::detail::typed_in_place_factory_owning_base, \
                               typename std::decay<Arg>::type>::value>::type> \
  type_(Arg&& arg, Args&&... args) \
    : underlying(std::forward<Arg>(arg), std::forward<Args>(args)...) {} \
  \
  /* Factory constructor - accepts any typed_in_place_factory_owning */ \
  template <typename... FactoryArgs> \
  type_(const amplusplus::detail::typed_in_place_factory_owning<underlying_type, FactoryArgs...>& f) \
    : underlying(amplusplus::detail::construct_from_tuple<underlying_type>(f.get_args())) {} \
  \
  underlying_type& get() { return underlying; } \
  const underlying_type& get() const { return underlying; } \
  underlying_type* operator->() { return &underlying; } \
  const underlying_type* operator->() const { return &underlying; }

#endif // AMPLUSPLUS_DETAIL_FACTORY_WRAPPER_HPP
