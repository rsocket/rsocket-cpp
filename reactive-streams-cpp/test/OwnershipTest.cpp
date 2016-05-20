// Copyright 2004-present Facebook. All Rights Reserved.

#include <memory>

#include <gtest/gtest.h>

#include "reactive-streams-cpp/utilities/Ownership.h"

using namespace ::testing;
using namespace ::reactivestreams;

class TestObject {
 public:
  bool deleted{false};
};

namespace std {
// Partially specialising std::default_delete must have lead to at least one
// death in the history of C++. But this is only a test, so should be fine,
// right?
template <>
struct default_delete<TestObject> {
  void operator()(TestObject* ptr) const {
    ptr->deleted = true;
  }
};
}

TEST(RefCountedDeleterTest, IncrementAndDecrement) {
  TestObject object;
  RefCountedDeleter<TestObject> refCount(&object);

  ASSERT_FALSE(object.deleted);
  refCount.increment();
  ASSERT_FALSE(object.deleted);
  refCount.increment();
  ASSERT_FALSE(object.deleted);
  refCount.decrement();
  ASSERT_FALSE(object.deleted);
  refCount.increment();
  ASSERT_FALSE(object.deleted);
  refCount.decrement();
  ASSERT_FALSE(object.deleted);
  refCount.decrement();
  ASSERT_TRUE(object.deleted);

  // A bit of cheating. We want to verify that our helper prevents deouble
  // freeing.
  object.deleted = false;
  refCount.increment();
  ASSERT_FALSE(object.deleted);
  refCount.decrement();
  ASSERT_FALSE(object.deleted);
}

TEST(RefCountedDeleterTest, DecrementDeferred0) {
  TestObject object;
  RefCountedDeleter<TestObject> refCount(&object, 1);
  {
    ASSERT_FALSE(object.deleted);
    auto handle = refCount.decrementDeferred();
    ASSERT_FALSE(object.deleted);
  }
  ASSERT_TRUE(object.deleted);
}

TEST(RefCountedDeleterTest, DecrementDeferred1) {
  TestObject object;
  RefCountedDeleter<TestObject> refCount(&object, 1);
  {
    ASSERT_FALSE(object.deleted);
    auto handle = refCount.decrementDeferred();
    ASSERT_FALSE(object.deleted);
    handle.decrement();
    ASSERT_TRUE(object.deleted);
    object.deleted = false;
    handle.decrement();
    ASSERT_FALSE(object.deleted);
  }
  ASSERT_FALSE(object.deleted);
}
