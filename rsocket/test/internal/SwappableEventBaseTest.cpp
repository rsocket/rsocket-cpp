// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <folly/ExceptionWrapper.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "rsocket/internal/SwappableEventBase.h"

using SwappableEventBase = rsocket::SwappableEventBase;

namespace {

// helpers for defining new eventbases/"did this callback run" trackers
#define EB(name) auto& name = get_event_base()
#define MAKE_DID_EXEC(name) \
  auto name = make_did_exec_tracker_impl(__LINE__, __FILE__, #name)

struct DidExecTracker {
  const int line;
  const std::string file;
  const std::string name;
  DidExecTracker(int line, std::string file, std::string name)
      : line(line), file(file), name(name) {}
  MOCK_METHOD0(mark, void());
};

struct DETMarkedOnce : public ::testing::CardinalityInterface {
  explicit DETMarkedOnce(DidExecTracker const& det) : det(det) {}
  DidExecTracker const& det;

  int ConservativeLowerBound() const override {
    return 1;
  }
  int ConservativeUpperBound() const override {
    return 1;
  }
  bool IsSatisfiedByCallCount(int cc) const override {
    return cc == 1;
  }
  bool IsSaturatedByCallCount(int cc) const override {
    return cc == 1;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "is called exactly once on ";
    *os << "Tracker<" << det.file << ":" << det.line << ">";
  }
};
::testing::Cardinality MarkedOnce(DidExecTracker const& det) {
  return ::testing::Cardinality(new DETMarkedOnce(det));
}

class SwappableEbTest : public ::testing::Test {
 public:
  std::vector<std::unique_ptr<folly::EventBase>> ebs;
  std::vector<std::shared_ptr<DidExecTracker>> did_exec_trackers;

  void loop_ebs() {
    {
      ::testing::InSequence s;
      for (auto tracker : did_exec_trackers) {
        EXPECT_CALL(*tracker, mark()).Times(MarkedOnce(*tracker));
      }
    }

    for (auto& eb : ebs) {
      ASSERT_TRUE(eb->loop());
    }

    // dtor verifies EXPECT_CALL
    did_exec_trackers.clear();
  }

  std::shared_ptr<DidExecTracker> make_did_exec_tracker_impl(
      int line,
      std::string const& file,
      std::string const& name) {
    did_exec_trackers.emplace_back(new DidExecTracker(line, file, name));
    return did_exec_trackers.back();
  }

  folly::EventBase& get_event_base() {
    ebs.emplace_back(new folly::EventBase());
    return *ebs.back();
  }

  void TearDown() override {
    // verify any trackers created after the last loop_ebs call
    loop_ebs();
  }
};

TEST_F(SwappableEbTest, MarkedOnceSanityCheck) {
  MAKE_DID_EXEC(t1);
  MAKE_DID_EXEC(t2);

  {
    ::testing::InSequence s;
    EXPECT_CALL(*t1, mark()).Times(MarkedOnce(*t1));
    EXPECT_CALL(*t2, mark()).Times(MarkedOnce(*t2));
  }

  t1->mark();
  t2->mark();

  did_exec_trackers.clear();
}

TEST_F(SwappableEbTest, RunningInCorrectEb) {
  EB(EbA);

  SwappableEventBase seb(EbA);

  MAKE_DID_EXEC(t1);
  seb.runInEventBaseThread([&](folly::EventBase& eb) {
    ASSERT_EQ(&eb, &EbA);
    t1->mark();
  });

  loop_ebs();
}

TEST_F(SwappableEbTest, CanSwapEbs) {
  EB(EbA);
  EB(EbB);

  SwappableEventBase seb(EbA);

  seb.setEventBase(EbB);

  MAKE_DID_EXEC(t1);
  seb.runInEventBaseThread([&](folly::EventBase& eb) {
    t1->mark();
    ASSERT_EQ(&eb, &EbB);
  });

  loop_ebs();
}

TEST_F(SwappableEbTest, SkipsToLastEb) {
  EB(EbA);
  EB(EbB);
  EB(EbC);

  SwappableEventBase seb(EbA);

  MAKE_DID_EXEC(t1);
  seb.runInEventBaseThread([&](auto& eb) {
    t1->mark();
    ASSERT_EQ(&eb, &EbA);
  });
  loop_ebs();

  seb.setEventBase(EbB);
  MAKE_DID_EXEC(t2);
  seb.runInEventBaseThread([&](auto& eb) {
    t2->mark();
    ASSERT_EQ(&eb, &EbC);
  });

  seb.setEventBase(EbC);
  MAKE_DID_EXEC(t3);
  seb.runInEventBaseThread([&](auto& eb) {
    t3->mark();
    ASSERT_EQ(&eb, &EbC);
  });

  loop_ebs();
}

TEST_F(SwappableEbTest, CanDestroySEB) {
  EB(EbA);
  EB(EbB);

  auto seb = std::make_shared<SwappableEventBase>(EbA);

  MAKE_DID_EXEC(t1);
  seb->runInEventBaseThread([&](auto& eb) {
    t1->mark();
    ASSERT_EQ(&eb, &EbA);
  });
  loop_ebs();

  seb->setEventBase(EbB);
  MAKE_DID_EXEC(t2);
  seb->runInEventBaseThread([&](auto& eb) {
    t2->mark();
    ASSERT_EQ(&eb, &EbA);
  });

  seb = nullptr;
  loop_ebs();
}

} /* namespace */
