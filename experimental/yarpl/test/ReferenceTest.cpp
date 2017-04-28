// Copyright 2004-present Facebook. All Rights Reserved.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "yarpl/Flowable.h"
#include "yarpl/Refcounted.h"

using yarpl::Refcounted;
using yarpl::Reference;
using yarpl::flowable::Subscriber;

namespace {

template <class T>
class MySubscriber : public Subscriber<T> {};

}

TEST(ReferenceTest, Upcast) {
  Reference<MySubscriber<int>> derived(new MySubscriber<int>());
  Reference<Subscriber<int>> base1(derived);

  Reference<Subscriber<int>> base2;
  base2 = derived;

  Reference<MySubscriber<int>> derivedCopy1(derived);
  Reference<MySubscriber<int>> derivedCopy2(derived);

  Reference<Subscriber<int>> base3(std::move(derivedCopy1));

  Reference<Subscriber<int>> base4;
  base4 = std::move(derivedCopy2);
}
