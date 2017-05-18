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
class MySubscriber : public Subscriber<T> {
  void onNext(T) override {}
};
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

TEST(RefcountedTest, CopyAssign) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a(new Sub());
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a->count());
  Sub* ptr = nullptr;
  Reference<Sub> c(ptr = new Sub());
  b = c;
  EXPECT_EQ(1u, a->count());
  EXPECT_EQ(ptr, b.get());
}

TEST(RefcountedTest, MoveAssign) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a(new Sub());
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a->count());
  Sub* ptr = nullptr;
  b = Reference<Sub>(ptr = new Sub());
  EXPECT_EQ(1u, a->count());
  EXPECT_EQ(ptr, b.get());
}

TEST(RefcountedTest, CopyAssignTemplate) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a(new Sub());
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a->count());
  using Sub2 = MySubscriber<int>;
  Sub2* ptr = nullptr;
  Reference<Sub2> c(ptr = new Sub2());
  b = c;
  EXPECT_EQ(1u, a->count());
  EXPECT_EQ(ptr, b.get());
}

TEST(RefcountedTest, MoveAssignTemplate) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a(new Sub());
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a->count());
  using Sub2 = MySubscriber<int>;
  Sub2* ptr = nullptr;
  b = Reference<Sub2>(ptr = new Sub2());
  EXPECT_EQ(1u, a->count());
  EXPECT_EQ(ptr, b.get());
}
