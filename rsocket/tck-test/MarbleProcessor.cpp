// Copyright (c) Facebook, Inc. and its affiliates.
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

#include "MarbleProcessor.h"

#include <folly/Conv.h>
#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/String.h>

namespace {

std::string trimQuotes(std::string input) {
  if (input.size() < 3) {
    return "";
  }
  CHECK(input[0] == '\"');
  CHECK(input[input.size() - 1] == '\"');
  return std::string(input, 1, input.size() - 2);
}

std::string trimBraces(std::string input) {
  if (input.size() < 3) {
    return "";
  }
  CHECK(input[0] == '{');
  CHECK(input[input.size() - 1] == '}');
  return std::string(input, 1, input.size() - 2);
}

std::map<std::string, std::pair<std::string, std::string>> getArgMap(
    std::string input) {
  std::map<std::string, std::pair<std::string, std::string>> argMap;
  std::vector<std::string> payloads;
  input = trimBraces(input);
  folly::split(",", input, payloads);
  for (const auto& payload : payloads) {
    std::string key, value;
    folly::split<false>(":", folly::StringPiece(payload), key, value);
    value = trimBraces(value);
    std::string data, metadata;
    folly::split<true>(":", folly::StringPiece(value), data, metadata);
    argMap[trimQuotes(key)] =
        std::make_pair(trimQuotes(data), trimQuotes(metadata));
  }
  return argMap;
}
} // namespace

namespace rsocket {
namespace tck {

MarbleProcessor::MarbleProcessor(const std::string marble)
    : marble_(std::move(marble)) {
  // Remove '-' which is of no consequence for the tests
  marble_.erase(
      std::remove(marble_.begin(), marble_.end(), '-'), marble_.end());

  LOG(INFO) << "Using marble: " << marble_;

  // Populate argMap_
  if (marble_.find("&&") != std::string::npos) {
    std::vector<std::string> parts;
    folly::split("&&", marble_, parts);
    CHECK(parts.size() == 2);
    argMap_ = getArgMap(parts[1]);
    marble_ = parts[0];
  }
}

void MarbleProcessor::run(
    yarpl::flowable::Subscriber<rsocket::Payload>& subscriber,
    int64_t requested) {
  canSend_ += requested;

  while (canSend_ > 0 && index_ < marble_.size()) {
    const auto c = marble_[index_];
    switch (c) {
      case '#':
        LOG(INFO) << "Sending onError";
        subscriber.onError(std::runtime_error("Marble Error"));
        break;
      case '|':
        LOG(INFO) << "Sending onComplete";
        subscriber.onComplete();
        break;
      default:
        if (canSend_ > 0) {
          Payload payload;
          const auto it = argMap_.find(folly::to<std::string>(c));
          LOG(INFO) << "Sending data " << c;
          if (it != argMap_.end()) {
            LOG(INFO) << folly::sformat(
                "Using mapping {}->{}:{}",
                c,
                it->second.first,
                it->second.second);
            payload = Payload(it->second.first, it->second.second);
          } else {
            payload =
                Payload(folly::to<std::string>(c), folly::to<std::string>(c));
          }
          subscriber.onNext(std::move(payload));
          canSend_--;
        }
        break;
    }
    index_++;
  }
}

void MarbleProcessor::run(
    std::shared_ptr<yarpl::single::SingleObserver<rsocket::Payload>>
        subscriber) {
  while (true) {
    const auto c = marble_[index_];
    switch (c) {
      case '#':
        LOG(INFO) << "Sending onError";
        subscriber->onError(std::runtime_error("Marble Error"));
        return;
      case '|':
        LOG(INFO) << "Sending onComplete";
        subscriber->onError(std::runtime_error("No Response found"));
        return;
      default: {
        Payload payload;
        const auto it = argMap_.find(folly::to<std::string>(c));
        LOG(INFO) << "Sending data " << c;
        if (it != argMap_.end()) {
          LOG(INFO) << folly::sformat(
              "Using mapping {}->{}:{}",
              c,
              it->second.first,
              it->second.second);
          payload = Payload(it->second.first, it->second.second);
        } else {
          payload =
              Payload(folly::to<std::string>(c), folly::to<std::string>(c));
        }
        subscriber->onSuccess(std::move(payload));
        return;
      }
    }
    index_++;
  }
}

} // namespace tck
} // namespace rsocket
