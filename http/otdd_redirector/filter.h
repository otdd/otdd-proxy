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
#include "envoy/http/filter.h"
#include "envoy/server/filter_config.h"
#include "src/envoy/http/otdd_redirector/otdd_redirector_config.pb.h"

using ::otdd::config::OtddRedirectorConfig;

namespace Envoy {
namespace Http {
namespace OtddRedirector {

class Filter : public StreamFilter,
               public Logger::Loggable<Logger::Id::filter>, 
               public Http::AsyncClient::Callbacks{
 public:
  Filter(OtddRedirectorConfig conf,Server::Configuration::FactoryContext& context);
  ~Filter();

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool) override;
  FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  FilterTrailersStatus decodeTrailers(HeaderMap& trailers) override;
  void setDecoderFilterCallbacks(
      StreamDecoderFilterCallbacks& callbacks) override;

  // Http::StreamFilterBase
  void onDestroy() override;

      // Http::StreamEncoderFilter
  FilterHeadersStatus encode100ContinueHeaders(HeaderMap&) override {
    return FilterHeadersStatus::Continue;
  }
  FilterHeadersStatus encodeHeaders(HeaderMap& , bool) override {
    return FilterHeadersStatus::Continue;
  }
  FilterDataStatus encodeData(Buffer::Instance&, bool) override {
    return FilterDataStatus::Continue;
  }
  FilterTrailersStatus encodeTrailers(HeaderMap&) override {
    return FilterTrailersStatus::Continue;
  }
  Http::FilterMetadataStatus encodeMetadata(MetadataMap&) override {
    return FilterMetadataStatus::Continue;
  }
  void setEncoderFilterCallbacks(StreamEncoderFilterCallbacks&) override {}

 private:
  // The stream decoder filter callback.
  StreamDecoderFilterCallbacks* decoder_callbacks_{nullptr};
  bool redirectRequest();
  // Http::AsyncClient::Callbacks
  void onSuccess(Http::MessagePtr&& response) override;
  void onFailure(Http::AsyncClient::FailureReason reason) override;
  void requestComplete();

  OtddRedirectorConfig conf_;
  Server::Configuration::FactoryContext& context_;
  HeaderMap* headers_;
  std::string body_;
  Http::AsyncClient::Request* active_request_{};
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
