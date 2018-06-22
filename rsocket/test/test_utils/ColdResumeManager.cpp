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

#include "ColdResumeManager.h"

#include <fstream>
#include <sstream>

#include <folly/json.h>

namespace {
constexpr folly::StringPiece FIRST_SENT_POSITION = "FirstSentPosition";
constexpr folly::StringPiece LAST_SENT_POSITION = "LastSentPosition";
constexpr folly::StringPiece IMPLIED_POSITION = "ImpliedPosition";
constexpr folly::StringPiece LARGEST_USED_STREAMID = "LargestUsedStreamId";
constexpr folly::StringPiece STREAM_RESUME_INFOS = "StreamResumeInfos";
constexpr folly::StringPiece FRAMES = "Frames";
constexpr folly::StringPiece STREAM_TYPE = "StreamType";
constexpr folly::StringPiece REQUESTER = "Requester";
constexpr folly::StringPiece STREAM_TOKEN = "StreamToken";
constexpr folly::StringPiece PROD_ALLOWANCE = "ProducerAllowance";
constexpr folly::StringPiece CONS_ALLOWANCE = "ConsumerAllowance";
} // namespace

namespace rsocket {

ColdResumeManager::ColdResumeManager(
    std::shared_ptr<RSocketStats> stats,
    std::string inputFile)
    : WarmResumeManager(std::move(stats)) {
  if (inputFile.empty()) {
    return;
  }
  LOG(INFO) << "Reading state from " << inputFile;

  try {
    std::ifstream f(inputFile);
    std::stringstream buffer;
    buffer << f.rdbuf();
    auto state = folly::parseJson(buffer.str());
    f.close();

    if (!state.isObject() || state.size() != 6) {
      throw std::runtime_error(
          "Invalid file content.  Expected dynamic object of 6 elements");
    }

    if (state.count(FIRST_SENT_POSITION) != 1 ||
        state.count(LAST_SENT_POSITION) != 1 ||
        state.count(IMPLIED_POSITION) != 1 ||
        state.count(LARGEST_USED_STREAMID) != 1 ||
        state.count(STREAM_RESUME_INFOS) != 1 || state.count(FRAMES) != 1) {
      throw std::runtime_error("Invalid file content.  Keys Missing");
    }

    firstSentPosition_ = state[FIRST_SENT_POSITION].getInt();
    lastSentPosition_ = state[LAST_SENT_POSITION].getInt();
    impliedPosition_ = state[IMPLIED_POSITION].getInt();
    largestUsedStreamId_ = state[LARGEST_USED_STREAMID].getInt();

    for (const auto& item : state[STREAM_RESUME_INFOS].items()) {
      auto streamId = folly::to<int64_t>(item.first.getString());
      auto streamResumeInfoObj = item.second;
      if (streamResumeInfoObj.count(STREAM_TYPE) != 1 ||
          streamResumeInfoObj.count(STREAM_TOKEN) != 1 ||
          streamResumeInfoObj.count(PROD_ALLOWANCE) != 1 ||
          streamResumeInfoObj.count(CONS_ALLOWANCE) != 1 ||
          streamResumeInfoObj.count(CONS_ALLOWANCE) != 1) {
        throw std::runtime_error(
            "Invalid file content.  StreamResumeInfo Keys Missing");
      }
      StreamResumeInfo streamResumeInfo(
          static_cast<StreamType>(streamResumeInfoObj[STREAM_TYPE].getInt()),
          static_cast<RequestOriginator>(
              streamResumeInfoObj[REQUESTER].getInt()),
          streamResumeInfoObj[STREAM_TOKEN].getString());
      streamResumeInfo.producerAllowance =
          streamResumeInfoObj[PROD_ALLOWANCE].getInt();
      streamResumeInfo.consumerAllowance =
          streamResumeInfoObj[CONS_ALLOWANCE].getInt();
      streamResumeInfos_.emplace(streamId, std::move(streamResumeInfo));
    }

    auto framesObj = state[FRAMES];
    if (!framesObj.isArray()) {
      throw std::runtime_error(
          "Invalid file content. Frames not in right format");
    }

    for (const auto& item : framesObj) {
      if (!item.isObject() || item.size() != 1) {
        throw std::runtime_error(
            "Invalid file content.  Expected dynamic object of 1 element");
      }
      auto ioBuf = folly::IOBuf::copyBuffer(
          item.values().begin()->getString().c_str(),
          item.values().begin()->getString().size());
      frames_.emplace_back(
          folly::to<int64_t>(item.keys().begin()->getString()),
          std::move(ioBuf));
    }
  } catch (const std::exception& ex) {
    throw std::runtime_error(
        folly::sformat("Failed parsing file {}. {}", inputFile, ex.what()));
  }
}

void ColdResumeManager::persistState(std::string outputFile) {
  VLOG(1) << "~ColdResumeManager";
  if (outputFile.empty()) {
    throw std::runtime_error("Persisting to file failed.  Empty filename");
  }
  LOG(INFO) << "Persisting state to " << outputFile;
  try {
    folly::dynamic state = folly::dynamic::object();
    state[FIRST_SENT_POSITION] = firstSentPosition_;
    state[LAST_SENT_POSITION] = lastSentPosition_;
    state[IMPLIED_POSITION] = impliedPosition_;
    state[LARGEST_USED_STREAMID] = largestUsedStreamId_;
    state[STREAM_RESUME_INFOS] = folly::dynamic::object();
    for (const auto& streamResumeInfo : streamResumeInfos_) {
      folly::dynamic val = folly::dynamic::object();
      val[STREAM_TYPE] = folly::to<int>(streamResumeInfo.second.streamType);
      val[STREAM_TOKEN] = streamResumeInfo.second.streamToken;
      val[REQUESTER] = folly::to<int>(streamResumeInfo.second.requester);
      val[CONS_ALLOWANCE] = streamResumeInfo.second.consumerAllowance;
      val[PROD_ALLOWANCE] = streamResumeInfo.second.producerAllowance;
      state[STREAM_RESUME_INFOS].insert(
          folly::to<std::string>(streamResumeInfo.first), val);
    }
    state[FRAMES] = folly::dynamic::array();
    for (const auto& frame : frames_) {
      state[FRAMES].push_back(folly::dynamic::object(
          folly::to<std::string>(frame.first),
          frame.second->moveToFbString().toStdString()));
    }
    std::string jsonState = folly::toPrettyJson(state);
    std::ofstream f(outputFile);
    f << jsonState;
    f.close();
  } catch (const std::exception& ex) {
    throw std::runtime_error(folly::sformat(
        "Persisting state to {} failed. {}", outputFile, ex.what()));
  }
  LOG(INFO) << "Done persisting state to " << outputFile;
}

void ColdResumeManager::trackReceivedFrame(
    size_t frameLength,
    FrameType frameType,
    StreamId streamId,
    size_t consumerAllowance) {
  if (!shouldTrackFrame(frameType)) {
    return;
  }
  auto it = streamResumeInfos_.find(streamId);
  // If streamId is not present in streamResumeInfo it likely means a
  // COMPLETE/CANCEL/ERROR was received in this frame and the
  // ResumeMananger::onCloseStream() was already invoked resulting in the entry
  // being deleted.
  if (it != streamResumeInfos_.end()) {
    it->second.consumerAllowance = consumerAllowance;
  }
  WarmResumeManager::trackReceivedFrame(
      frameLength, frameType, streamId, consumerAllowance);
}

void ColdResumeManager::trackSentFrame(
    const folly::IOBuf& serializedFrame,
    FrameType frameType,
    StreamId streamId,
    size_t consumerAllowance) {
  if (!shouldTrackFrame(frameType)) {
    return;
  }
  auto it = streamResumeInfos_.find(streamId);
  CHECK(it != streamResumeInfos_.end());
  it->second.consumerAllowance = consumerAllowance;
  WarmResumeManager::trackSentFrame(
      std::move(serializedFrame), frameType, streamId, consumerAllowance);
}

void ColdResumeManager::onStreamClosed(StreamId streamId) {
  streamResumeInfos_.erase(streamId);
}

void ColdResumeManager::onStreamOpen(
    StreamId streamId,
    RequestOriginator requester,
    std::string streamToken,
    StreamType streamType) {
  CHECK(streamType != StreamType::FNF);
  CHECK(streamResumeInfos_.find(streamId) == streamResumeInfos_.end());
  if (requester == RequestOriginator::LOCAL &&
      streamId > largestUsedStreamId_) {
    largestUsedStreamId_ = streamId;
  }
  streamResumeInfos_.emplace(
      streamId, StreamResumeInfo(streamType, requester, streamToken));
}

} // namespace rsocket
