// Copyright 2004-present Facebook. All Rights Reserved.

#include "SwappableEventBase.h"

namespace rsocket {

bool SwappableEventBase::runInEventBaseThread(CbFunc cb) {
  std::lock_guard<std::mutex> l(hasSebDtored_->l_);

  if(this->isSwapping()) {
    queued_.push_back(std::move(cb));
    return false;
  }

  return eb_->runInEventBaseThread([eb = eb_, cb_ = std::move(cb)]() mutable {
    return cb_(*eb);
  });
}

void SwappableEventBase::setEventBase(folly::EventBase& newEb) {
  std::lock_guard<std::mutex> l(hasSebDtored_->l_);

  auto const alreadySwapping = this->isSwapping();
  nextEb_ = &newEb;
  if(alreadySwapping) {
    return;
  }

  eb_->runInEventBaseThread([this, hasSebDtored = hasSebDtored_]() {
    std::lock_guard<std::mutex> lInner(hasSebDtored->l_);
    if(hasSebDtored->destroyed_) {
      // SEB was destroyed, any queued callbacks were appended to the old eb_
      return;
    }

    eb_ = nextEb_;
    nextEb_ = nullptr;

    // enqueue tasks that were being buffered while this was waiting
    // for the previous EB to drain
    for(auto& cb : queued_) {
      eb_->runInEventBaseThread([cb = std::move(cb), eb = eb_]() mutable {
        return cb(*eb);
      });
    }

    queued_.clear();
  });
}

bool SwappableEventBase::isSwapping() const {
  return nextEb_ != nullptr;
}

SwappableEventBase::~SwappableEventBase() {
  std::lock_guard<std::mutex> l(hasSebDtored_->l_);

  hasSebDtored_->destroyed_ = true;
  for(auto& cb : queued_) {
    eb_->runInEventBaseThread([cb = std::move(cb), eb = eb_]() mutable {
      return cb(*eb);
    });
  }
  queued_.clear();
}

} /* namespace rsocket */
