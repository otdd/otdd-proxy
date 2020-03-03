/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#include "src/envoy/http/otdd_redirector/filter.h"

#include "common/common/base64.h"
#include "common/protobuf/utility.h"
#include "include/istio/utils/status.h"
#include "src/envoy/utils/authn.h"
#include "common/http/async_client_impl.h"
#include "common/common/stack_array.h"
#include "common/common/enum_to_int.h"

using ::google::protobuf::util::Status;

namespace Envoy {
namespace Http {
namespace OtddRedirector {

    static unsigned long _s_last_redirect_time = 0;
    static bool _s_is_redirecting = false;
    static Http::AsyncClient::Request* _s_active_request_ptr = nullptr;
    static std::thread::id*  _s_max_worker_thread_id_ptr = nullptr;

    Filter::Filter(OtddRedirectorConfig conf,Server::Configuration::FactoryContext& context)
      :context_(context){
      conf_ = conf;
      if(_s_max_worker_thread_id_ptr == nullptr){
        _s_max_worker_thread_id_ptr = new std::thread::id();
        *_s_max_worker_thread_id_ptr = std::this_thread::get_id();
      } 
      if(*_s_max_worker_thread_id_ptr < std::this_thread::get_id()){
        *_s_max_worker_thread_id_ptr = std::this_thread::get_id();
      }
    }
    
    Filter::~Filter() {
    }

    FilterHeadersStatus Filter::decodeHeaders(HeaderMap& headers, bool end_stream) {
      ENVOY_LOG(debug,"decodeHeaders end_stream:{}",end_stream);
      headers.iterate(
            [](const Http::HeaderEntry& header,void* ){
                  ENVOY_LOG(debug,"decodeHeaders header key:{} value:{}",header.key().getStringView(),header.value().getStringView());
                  return Http::HeaderMap::Iterate::Continue;
             }, nullptr 
      );
      headers_ = &headers;
      if(end_stream){
            if(redirectRequest()){
                return FilterHeadersStatus::StopIteration;
            }
            else{
                return FilterHeadersStatus::Continue;
            }
      }
      else{
            return FilterHeadersStatus::Continue;
      }
    }
    
    FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
      ENVOY_LOG(debug,"decodeData, end_stream:{}",end_stream);
      uint64_t num_slices = data.getRawSlices(nullptr, 0);
      STACK_ARRAY(slices, Buffer::RawSlice, num_slices);
      data.getRawSlices(slices.begin(), num_slices);
      for (const Buffer::RawSlice& slice : slices) {
        body_.append(static_cast<const char*>(slice.mem_), slice.len_);
      }
      ENVOY_LOG(debug,"decodeData, body:{}",body_);
      if(end_stream){
            if(redirectRequest()){
                return FilterDataStatus::StopIterationNoBuffer;
            }
            else{
                return FilterDataStatus::Continue;
            }
      }
      else{
            return FilterDataStatus::Continue;
      }
    }
    
    FilterTrailersStatus Filter::decodeTrailers(HeaderMap& headers) {
      ENVOY_LOG(debug,"decodeTrailers");
      headers.iterate(
            [](const Http::HeaderEntry& header,void* ){
                  ENVOY_LOG(debug,"decodeTrailers header key:{} value:{}",header.key().getStringView(),header.value().getStringView());
                  return Http::HeaderMap::Iterate::Continue;
             }, nullptr 
      );
      return FilterTrailersStatus::Continue;
    }
    
    void Filter::setDecoderFilterCallbacks(
        StreamDecoderFilterCallbacks& callbacks) {
      decoder_callbacks_ = &callbacks;
    }
    
    void Filter::onDestroy() {
    }

    bool Filter::redirectRequest(){
      //only allow one worker thread to perform the redirection in order to avoid locking.
      //we choose the worker thread that has the max thread_id.
      if(*_s_max_worker_thread_id_ptr != std::this_thread::get_id()){
          return false;
      }
      if( _s_is_redirecting ){
          return false;
      }
      const auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>( 
        std::chrono::system_clock::now().time_since_epoch()).count();
      if(current_time - _s_last_redirect_time < conf_.interval()){
          return false;
      }
      _s_is_redirecting = true;
      _s_last_redirect_time = current_time;
      ENVOY_LOG(info,"redirecting request to: {}",conf_.target_cluster());
      Http::MessagePtr request(new Http::RequestMessageImpl(Http::HeaderMapPtr{new Http::HeaderMapImpl(*headers_)}));
      if(body_.length()>0){
          request->body() = std::make_unique<Buffer::OwnedImpl>(body_);
          request->headers().insertContentLength().value(body_.length());
      }

      //the upstream timeout will not be treated as an failure and will crash proxy in current async client implementation. so just set this timeout to a very big value.
      auto timeout = std::chrono::milliseconds(60000);
      auto &cm = context_.clusterManager();
      auto &client = cm.httpAsyncClientForCluster(conf_.target_cluster());
      if(_s_active_request_ptr!=nullptr){
        _s_active_request_ptr->cancel();
      }
      _s_active_request_ptr = client.send(std::move(request), *this,AsyncClient::RequestOptions().setTimeout(timeout));
      //active_request_ = context_.clusterManager().httpAsyncClientForCluster(conf_.target_cluster())
      //                 .send(std::move(request), *this,
      //                      AsyncClient::RequestOptions().setTimeout(timeout));
      return true;
    }

    void Filter::onSuccess(Http::MessagePtr&& response){
      ENVOY_LOG(info,"redirect request succeed.");
      requestComplete();

      decoder_callbacks_->sendLocalReply(
        Code(Http::Utility::getResponseStatus(response->headers())), response->bodyAsString(),
        [& headers = response->headers()](HeaderMap& response_headers) {

          headers.iterate(
            [](const Http::HeaderEntry& header, void* context) -> Http::HeaderMap::Iterate {
            HeaderMap* response_headers = static_cast<HeaderMap*>(context);
            if(std::string(header.key().getStringView())!=":status"&&std::string(header.key().getStringView())!="content-length"){
              ENVOY_LOG(debug,"add header: {}={}",std::string(header.key().getStringView()),std::string(header.value().getStringView()));
              response_headers->addCopy(Http::LowerCaseString{std::string(header.key().getStringView())},std::string(header.value().getStringView()));
            }
            return Http::HeaderMap::Iterate::Continue;
          },&response_headers);
           //HeaderUtility::addHeaders(response_headers,headers);
        },
        absl::nullopt, "otdd_redirected_response");
      return;
    }

    void Filter::onFailure(Http::AsyncClient::FailureReason reason){
      ENVOY_LOG(warn,"redirect request failed. reason: {}, the request will continue to be processed locally.",enumToInt(reason));
      requestComplete();
      decoder_callbacks_->continueDecoding();
    }
    void Filter::requestComplete(){
      _s_active_request_ptr = nullptr;
      _s_is_redirecting = false;
      headers_ = nullptr;
      body_ = "";
      const auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>( 
        std::chrono::system_clock::now().time_since_epoch()).count();
      _s_last_redirect_time = current_time;
    }

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
