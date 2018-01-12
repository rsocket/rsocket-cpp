// Copyright 2017 Per Grön. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <type_traits>

#include <rs/publisher.h>
#include <rs/subscription.h>

namespace shk {

inline auto Throw(const std::exception_ptr &error) {
  return MakePublisher([error](auto &&subscriber) {
    subscriber.OnError(std::exception_ptr(error));
    return MakeSubscription();
  });
}

template <
    typename T,
    class = typename std::enable_if<
        !std::is_same<
            std::exception_ptr,
            typename std::decay<T>::type>::value>::type>
auto Throw(T &&t) {
  return Throw(std::make_exception_ptr(std::forward<T>(t)));
}

}  // namespace shk
