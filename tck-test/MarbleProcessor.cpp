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

MarbleProcessor::MarbleProcessor(
    const std::string marble,
    const yarpl::Reference<yarpl::flowable::Subscriber<Payload>>& subscriber)
    : marble_(std::move(marble)), subscriber_(std::move(subscriber)) {
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

void MarbleProcessor::run() {
  for (const char& c : marble_) {
    if (terminated_) {
      return;
    }
    switch (c) {
      case '#':
        while (!canTerminate_)
          ;
        LOG(INFO) << "Sending onError";
        subscriber_->onError(std::make_exception_ptr(
            std::runtime_error("Marble Triggered Error")));
        return;
      case '|':
        while (!canTerminate_)
          ;
        LOG(INFO) << "Sending onComplete";
        subscriber_->onComplete();
        return;
      default:
        while (canSend_ <= 0)
          ;
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
        subscriber_->onNext(std::move(payload));
        canSend_--;
        break;
    }
  }
}

void MarbleProcessor::request(size_t n) {
  LOG(INFO) << "Received request (" << n << ")";
  canTerminate_ = true;
  canSend_ += n;
}

void MarbleProcessor::cancel() {
  LOG(INFO) << "Received cancel";
  terminated_ = true;
}

} // tck
} // reactivesocket
