/* Copyright 2018 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "common/common/logger.h"
#include "envoy/network/connection.h"
#include "envoy/network/filter.h"
#include "google/protobuf/struct.pb.h"
#include "include/istio/mixerclient/check_response.h"
#include "src/envoy/tcp/otdd_recorder/config.h"
#include "src/envoy/tcp/otdd_recorder/otdd_test_case.h"
#include "envoy/server/filter_config.h"

using ::otddrecorder::config::OtddRecorderConfig;

namespace Envoy {
namespace Tcp {
namespace OtddRecorder {

class Filter : public Network::Filter,
               public Network::ConnectionCallbacks,
               public Logger::Loggable<Logger::Id::filter> {
 public:
  Filter(OtddRecorderConfig conf,Server::Configuration::FactoryContext& context);
  ~Filter();

  void initializeReadFilterCallbacks(
      Network::ReadFilterCallbacks &callbacks) override;

  // Network::ReadFilter
  Network::FilterStatus onData(Buffer::Instance &data, bool) override;

  // Network::WriteFilter
  Network::FilterStatus onWrite(Buffer::Instance &data, bool) override;
  Network::FilterStatus onNewConnection() override;

  // Network::ConnectionCallbacks
  void onEvent(Network::ConnectionEvent ) override;

  void onAboveWriteBufferHighWatermark() override {}
  void onBelowWriteBufferLowWatermark() override {}

 private:
  bool reportToMixer(std::shared_ptr<OtddTestCase> otdd_test);
  std::string convertTestCallToJson(std::shared_ptr<OtddCall> otdd_call);

  // filter callback
  Network::ReadFilterCallbacks *filter_callbacks_{};

  // start_time
  std::chrono::time_point<std::chrono::system_clock> start_time_;

  OtddRecorderConfig config_;
  std::shared_ptr<OtddCall> otdd_call_ptr_;
  bool write_occured_;
  Server::Configuration::FactoryContext& context_;

};

}  // namespace OtddRecorder
}  // namespace Tcp
}  // namespace Envoy
