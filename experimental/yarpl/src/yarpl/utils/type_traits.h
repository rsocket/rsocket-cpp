// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <type_traits>

#if __cplusplus < 201500

namespace std {

namespace implementation {

template <typename C, typename R, typename Enable = void>
struct is_callable : std::false_type {};

template <typename F, typename... Args, typename R>
struct is_callable<
    F(Args...), R,
    std::enable_if_t<std::is_same<R, std::result_of_t<F(Args...)>>::value>>
    : std::true_type {};

}  // implementation

template <typename Call, typename Return>
struct is_callable : implementation::is_callable<Call, Return> {};

}  // std
#endif  // __cplusplus
