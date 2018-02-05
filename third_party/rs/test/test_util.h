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

#include <vector>

#include <rs/element_count.h>
#include <rs/subscriber.h>
#include <rs/subscription.h>

namespace shk {

inline std::string GetErrorWhat(const std::exception_ptr &error) {
  if (!error) {
    return "[null error]";
  }
  try {
    std::rethrow_exception(error);
  } catch (const std::exception &error) {
    return error.what();
  }
}

template <typename T, typename Publisher>
T GetOne(
    const Publisher &publisher,
    ElementCount request_count = ElementCount::Unbounded()) {
  bool has_value = false;
  bool is_done = false;
  T result{};
  auto sub = publisher.Subscribe(MakeSubscriber(
      [&result, &has_value, &is_done](auto &&val) {
        CHECK(!is_done);
        CHECK(!has_value);
        result = std::forward<decltype(val)>(val);
        has_value = true;
      },
      [](std::exception_ptr &&error) {
        CHECK(!"OnError should not be called");
      },
      [&has_value, &is_done] {
        CHECK(!is_done);
        CHECK(has_value);
        is_done = true;
      }));
  CHECK(!has_value);
  CHECK(!is_done);
  sub.Request(request_count);
  CHECK(is_done == (request_count != 0));
  return result;
}

template <typename T, typename Publisher>
std::vector<T> GetAll(
    const Publisher &publisher,
    ElementCount request_count = ElementCount::Unbounded(),
    bool expect_done = true) {
  std::vector<T> result;
  bool is_done = false;
  auto sub = publisher.Subscribe(MakeSubscriber(
      [&is_done, &result](auto &&val) {
        CHECK(!is_done);
        result.emplace_back(std::forward<decltype(val)>(val));
      },
      [](std::exception_ptr &&error) {
        printf("Error what: %s\n", GetErrorWhat(error).c_str());
        CHECK(!"OnError should not be called");
      },
      [&is_done] {
        CHECK(!is_done);
        is_done = true;
      }));
  sub.Request(request_count);
  CHECK(is_done == expect_done);

  return result;
}

template <typename Publisher>
std::exception_ptr GetError(
    const Publisher &stream,
    ElementCount request_count = ElementCount::Unbounded()) {
  std::exception_ptr received_error;

  auto sub = stream.Subscribe(MakeSubscriber(
      [&received_error](auto &&next) {
        CHECK(!received_error);
      },
      [&received_error](std::exception_ptr &&error) {
        CHECK(!received_error);
        received_error = error;
      },
      [] { CHECK(!"OnComplete should not be called"); }));
  sub.Request(request_count);
  CHECK(received_error);
  return received_error ?
      received_error :
      std::make_exception_ptr(
          std::logic_error("[no error when one was expected]"));
}

template <typename Publisher>
void CheckLeak(Publisher &&publisher) {
  bool destroyed = false;
  auto lifetime_tracer = std::shared_ptr<void>(nullptr, [&destroyed](void *) {
    destroyed = true;
  });
  auto null_subscriber = MakeSubscriber(
      [lifetime_tracer = std::move(lifetime_tracer)](auto &&) {
        CHECK(!"should not happen");
      },
      [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
      [] {});

  publisher.Subscribe(std::move(null_subscriber));

  CHECK(destroyed);
}

inline auto MakeNonDefaultConstructibleSubscriber() {
  return MakeSubscriber(
      [a = std::unique_ptr<int>()](auto &&) {},
      [](std::exception_ptr &&error) {},
      [] {});
}

}  // namespace shk