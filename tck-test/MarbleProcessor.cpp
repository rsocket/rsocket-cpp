// Copyright 2004-present Facebook. All Rights Reserved.

#include "MarbleProcessor.h"

#include <algorithm>

#include <folly/Conv.h>
#include <folly/ExceptionWrapper.h>
#include <folly/json.h>

#include "src/Payload.h"

namespace reactivesocket {
namespace tck {

MarbleProcessor::MarbleProcessor(
    const std::string marble,
    const std::shared_ptr<Subscriber<Payload>>& subscriber)
    : marble_(std::move(marble)), subscriber_(std::move(subscriber)) {
  // Remove '-' which is of no consequence for the tests
  marble_.erase(
      std::remove(marble_.begin(), marble_.end(), '-'), marble_.end());

  LOG(INFO) << "Using marble: " << marble_;

  // Populate argMap_
  if (marble_.find("&&") != std::string::npos) {
    std::vector<folly::StringPiece> parts;
    folly::split("&&", marble_, parts);
    CHECK(parts.size() == 2);
    std::string argMap = parts[1].toString();
    LOG(INFO) << "Parsing argMap `" << argMap << "`";
    folly::dynamic parsedJson = folly::parseJson(argMap);
    LOG(INFO) << parsedJson.size();
    LOG(INFO) << parsedJson;
    for (const auto& item : parsedJson.items()) {
      try {
        LOG(INFO) << item.first.typeName();
        LOG(INFO) << item.first.asString();
        LOG(INFO) << item.second.typeName();
        LOG(INFO) << item.second.size();
        LOG(INFO) << item.second.keys().begin()->typeName();
        LOG(INFO) << item.second.keys().begin()->asString();
        LOG(INFO) << item.second.values().begin()->typeName();
        LOG(INFO) << item.second.values().begin()->asString();
        argMap_[item.first.asString()] = std::make_pair(
            item.second.keys().begin()->asString(),
            item.second.values().begin()->asString());
      } catch (std::exception& ex) {
        LOG(INFO) << ex.what();
      }
    }
    for (const auto& i : argMap_) {
      LOG(INFO) << i.first << " -> " << i.second.first << " " << i.second.second;
    }
    marble_ = parts[0].toString();
    LOG(INFO) << "Actual marble `" << marble_ << "`";
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
        subscriber_->onError(std::runtime_error("Marble Triggered Error"));
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
