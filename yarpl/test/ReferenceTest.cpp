// Copyright 2004-present Facebook. All Rights Reserved.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "yarpl/Flowable.h"
#include "yarpl/Refcounted.h"

using yarpl::Refcounted;
using yarpl::Reference;
using yarpl::flowable::Subscriber;
using yarpl::flowable::BaseSubscriber;

namespace {

template <class T>
class MySubscriber : public BaseSubscriber<T> {
  void onSubscribeImpl() override {}
  void onNextImpl(T) override {}
  void onCompleteImpl() override {}
  void onErrorImpl(folly::exception_wrapper) override {}
};
}

struct MyRefcounted : virtual Refcounted {
  MyRefcounted(int i) : i(i) {}
  int i;
};

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

TEST(ReferenceTest, CopyAssign) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a = yarpl::make_ref<Sub>();
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a.use_count());
  Reference<Sub> c = yarpl::make_ref<Sub>();
  b = c;
  EXPECT_EQ(1u, a.use_count());
  EXPECT_EQ(2u, b.use_count());
  EXPECT_EQ(2u, c.use_count());
  EXPECT_EQ(c, b);
}

TEST(ReferenceTest, MoveAssign) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a = yarpl::make_ref<Sub>();
  Reference<Sub> b(std::move(a));
  EXPECT_EQ(nullptr, a);
  EXPECT_EQ(1u, b.use_count());
}

TEST(ReferenceTest, MoveAssignTemplate) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a = yarpl::make_ref<Sub>();
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a.use_count());
  using Sub2 = MySubscriber<int>;
  b = yarpl::make_ref<Sub2>();
  EXPECT_EQ(1u, a.use_count());
}

TEST(ReferenceTest, Construction) {
  Reference<MyRefcounted> a{yarpl::make_ref<MyRefcounted>(1)};
  EXPECT_EQ(1u, a.use_count());
  EXPECT_EQ(1, a->i);

  Reference<MyRefcounted> b = yarpl::make_ref<MyRefcounted>(2);
  EXPECT_EQ(1u, b.use_count());
  EXPECT_EQ(2, b->i);
}
