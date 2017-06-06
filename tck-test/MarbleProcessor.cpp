// Copyright 2004-present Facebook. All Rights Reserved.

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
}

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

std::tuple<int64_t, bool> MarbleProcessor::run(
    yarpl::flowable::Subscriber<rsocket::Payload>& subscriber,
    int64_t requested) {
  canSend_ += requested;
  if (index_ > marble_.size()) {
    return std::make_tuple(requested, true);
  }

  while (true) {
    auto c = marble_[index_];
    switch (c) {
      case '#':
        LOG(INFO) << "Sending onError";
        subscriber.onError(
            std::make_exception_ptr(std::runtime_error("Marble Error")));
        return std::make_tuple(requested, true);
      case '|':
        LOG(INFO) << "Sending onComplete";
        subscriber.onComplete();
        return std::make_tuple(requested, true);
      default: {
        if (canSend_ > 0) {
          Payload payload;
          auto it = argMap_.find(folly::to<std::string>(c));
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
        } else {
          return std::make_tuple(requested, false);
        }
      }
    }
    index_++;
  }
}

void MarbleProcessor::run(
    yarpl::Reference<yarpl::single::SingleObserver<rsocket::Payload>>
        subscriber) {
  while (true) {
    auto c = marble_[index_];
    switch (c) {
      case '#':
        LOG(INFO) << "Sending onError";
        subscriber->onError(
            std::make_exception_ptr(std::runtime_error("Marble Error")));
        return;
      case '|':
        LOG(INFO) << "Sending onComplete";
        subscriber->onError(
            std::make_exception_ptr(std::runtime_error("No Response found")));
        return;
      default: {
        Payload payload;
        auto it = argMap_.find(folly::to<std::string>(c));
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

} // tck
} // reactivesocket
