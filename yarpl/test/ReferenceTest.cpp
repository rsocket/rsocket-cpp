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
  Reference<MySubscriber<int>> derived = yarpl::make_ref<MySubscriber<int>>();
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
  Reference<Sub> a = yarpl::make_ref<Sub>();
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a->count());
  Reference<Sub> c = yarpl::make_ref<Sub>();
  b = c;
  EXPECT_EQ(1u, a->count());
  EXPECT_EQ(2u, b->count());
  EXPECT_EQ(2u, c->count());
  EXPECT_EQ(c, b);
}

TEST(RefcountedTest, MoveAssign) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a = yarpl::make_ref<Sub>();
  Reference<Sub> b(std::move(a));
  EXPECT_EQ(nullptr, a);
  EXPECT_EQ(1u, b->count());

  Reference<Sub> c;
  c = std::move(b);
  EXPECT_EQ(nullptr, b);
  EXPECT_EQ(1u, c->count());
}

TEST(RefcountedTest, MoveAssignTemplate) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a = yarpl::make_ref<Sub>();
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a->count());
  using Sub2 = MySubscriber<int>;
  b = yarpl::make_ref<Sub2>();
  EXPECT_EQ(1u, a->count());
}
