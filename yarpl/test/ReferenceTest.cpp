// Copyright 2004-present Facebook. All Rights Reserved.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "yarpl/Flowable.h"
#include "yarpl/Refcounted.h"

using yarpl::Refcounted;
using yarpl::Reference;
using yarpl::AtomicReference;
using yarpl::flowable::Subscriber;

namespace {

template <class T>
class MySubscriber : public Subscriber<T> {
  void onNext(T) override {}
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
  EXPECT_EQ(2u, a->count());
  Reference<Sub> c = yarpl::make_ref<Sub>();
  b = c;
  EXPECT_EQ(1u, a->count());
  EXPECT_EQ(2u, b->count());
  EXPECT_EQ(2u, c->count());
  EXPECT_EQ(c, b);
}

TEST(ReferenceTest, MoveAssign) {
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

TEST(ReferenceTest, MoveAssignTemplate) {
  using Sub = MySubscriber<int>;
  Reference<Sub> a = yarpl::make_ref<Sub>();
  Reference<Sub> b(a);
  EXPECT_EQ(2u, a->count());
  using Sub2 = MySubscriber<int>;
  b = yarpl::make_ref<Sub2>();
  EXPECT_EQ(1u, a->count());
}

TEST(ReferenceTest, Atomic) {
  auto a = yarpl::make_ref<MyRefcounted>(1);
  AtomicReference<MyRefcounted> b = a;
  EXPECT_EQ(2u, a->count());
  EXPECT_EQ(2u, b->count()); // b and a point to same object
  EXPECT_EQ(1, a->i);
  EXPECT_EQ(1, b->i);

  auto c = yarpl::make_ref<MyRefcounted>(2);
  {
    auto a_copy = b.exchange(c);
    EXPECT_EQ(2, b->i);
    EXPECT_EQ(2u, a->count());
    EXPECT_EQ(2u, a_copy->count());
    EXPECT_EQ(1, a_copy->i);
  }
  EXPECT_EQ(1u, a->count()); // a_copy destroyed

  EXPECT_EQ(2u, c->count());
  EXPECT_EQ(2u, b->count()); // b and c point to same object
}

TEST(ReferenceTest, Construction) {
  AtomicReference<MyRefcounted> a{yarpl::make_ref<MyRefcounted>(1)};
  EXPECT_EQ(1u, a->count());
  EXPECT_EQ(1, a->i);

  AtomicReference<MyRefcounted> b = yarpl::make_ref<MyRefcounted>(2);
  EXPECT_EQ(1u, b->count());
  EXPECT_EQ(2, b->i);
}
