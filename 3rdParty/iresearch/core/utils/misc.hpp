////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_MISC_H
#define IRESEARCH_MISC_H

#include "math_utils.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @brief Cross-platform 'COUNTOF' implementation
////////////////////////////////////////////////////////////////////////////////
#if __cplusplus >= 201103L || _MSC_VER >= 1900 || IRESEARCH_COMPILER_HAS_FEATURE(cxx_constexpr) // C++ 11 implementation
  NS_BEGIN(detail)
  template <typename T, std::size_t N>
  CONSTEXPR std::size_t countof(T const (&)[N]) noexcept { return N; }
  NS_END // detail
  #define IRESEARCH_COUNTOF(x) ::iresearch::detail::countof(x)
#elif _MSC_VER // Visual C++ fallback
  #define IRESEARCH_COUNTOF(x) _countof(x)
#elif __cplusplus >= 199711L && \
      (defined(__clang__) \
       || (defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))) \
      ) // C++ 98 trick
  template <typename T, std::size_t N>
  char(&COUNTOF_ARRAY_ARGUMENT(T(&)[N]))[N];
  #define IRESEARCH_COUNTOF(x) sizeof(COUNTOF_ARRAY_ARGUMENT(x))
#else
  #define IRESEARCH_COUNTOF(x) sizeof(x) / sizeof(x[0])
#endif

template<typename Func>
class finally {
 public:
  finally(const Func& func) : func_(func) { }
  finally(Func&& func) : func_(std::move(func)) { }
  ~finally() { func_(); }

 private:
  Func func_;
};

template<typename Func>
finally<Func> make_finally(Func&& func) {
  return finally<Func>(std::forward<Func>(func));
}

NS_END

#endif