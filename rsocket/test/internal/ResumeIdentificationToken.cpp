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

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "rsocket/framing/ResumeIdentificationToken.h"

using namespace rsocket;

TEST(ResumeIdentificationTokenTest, Conversion) {
  for (int i = 0; i < 10; i++) {
    auto token = ResumeIdentificationToken::generateNew();
    auto token2 = ResumeIdentificationToken(token.str());
    CHECK_EQ(token, token2);
    CHECK_EQ(token.str(), token2.str());
  }
}
