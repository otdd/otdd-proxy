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

#include "src/envoy/tcp/otdd_recorder/filter.h"

#include "common/common/enum_to_int.h"
#include "extensions/filters/network/well_known_names.h"
#include "src/envoy/utils/utils.h"
#include "src/envoy/utils/mixer_control.h"
#include "src/envoy/utils/grpc_transport.h"
#include "src/istio/mixerclient/status_util.h"
#include "common/common/base64.h"
#include <ctime>

using ::google::protobuf::util::Status;
using ::istio::mixerclient::CheckResponseInfo;
using ::otddrecorder::config::OtddRecorderConfig;
using ::istio::mixer::v1::ReportResponse;
using ::istio::mixerclient::TransportResult;
using ::istio::mixerclient::TransportStatus;

namespace Envoy {
namespace Tcp {
namespace OtddRecorder {

static std::shared_ptr<OtddTestCase> _s_current_otdd_testcase_ptr = NULL;

Filter::Filter(OtddRecorderConfig conf,Server::Configuration::FactoryContext& context):
  context_(context) {
  config_ = conf;
  otdd_call_ptr_ = NULL;
  write_occured_ = false;
}

Filter::~Filter() {
}

void Filter::initializeReadFilterCallbacks(
    Network::ReadFilterCallbacks &callbacks) {
  filter_callbacks_ = &callbacks;
  filter_callbacks_->connection().addConnectionCallbacks(*this);
  start_time_ = std::chrono::system_clock::now();
}

// Network::ReadFilter
Network::FilterStatus Filter::onData(Buffer::Instance &data, bool) {

  if(otdd_call_ptr_==NULL || write_occured_){
    write_occured_ = false;
    // report last recorded otdd test.
    if(config_.is_inbound()){
      if(_s_current_otdd_testcase_ptr!=NULL){
        reportToMixer(_s_current_otdd_testcase_ptr);
      }
      _s_current_otdd_testcase_ptr = std::make_shared<OtddTestCase>();
    }
    // start a new otdd call
    otdd_call_ptr_ = std::make_shared<OtddCall>();
    if(config_.is_inbound()){//if it's inbound connection, it's a client call.
      _s_current_otdd_testcase_ptr->inbound_call_ = otdd_call_ptr_;
    }
    else{//if it's outbound connection, it's an outbound call
      _s_current_otdd_testcase_ptr->outbound_calls_.push_back(otdd_call_ptr_);
    }
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    otdd_call_ptr_->req_timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  }
  otdd_call_ptr_->req_bytes_.append(data.toString());

  ENVOY_LOG(debug,"in tcp filter onRead, content: {}, len: {} is_inbound:{} conn remote:{} local:{}",  data.toString(),data.toString().length(),config_.is_inbound(),
                 filter_callbacks_->connection().remoteAddress()->asString(),filter_callbacks_->connection().localAddress()->asString());
  return Network::FilterStatus::Continue;
}

// Network::WriteFilter
Network::FilterStatus Filter::onWrite(Buffer::Instance &data, bool) {
  write_occured_ = true;
  if(otdd_call_ptr_==NULL){
    ENVOY_LOG(debug,"it's a greeting msg.");
    otdd_call_ptr_ = std::make_shared<OtddCall>();
    _s_current_otdd_testcase_ptr->outbound_calls_.push_back(otdd_call_ptr_);
  }
  if(otdd_call_ptr_->resp_bytes_==""){
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    otdd_call_ptr_->resp_timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  }
  otdd_call_ptr_->resp_bytes_.append(data.toString());
  ENVOY_LOG(debug,"in tcp filter onWrite, content: {}, len: {} conn remote:{} local:{}",data.toString(),data.toString().length(),
                 filter_callbacks_->connection().remoteAddress()->asString(),filter_callbacks_->connection().localAddress()->asString());
  return Network::FilterStatus::Continue;
}

Network::FilterStatus Filter::onNewConnection() {
  // Wait until onData() is invoked.
  return Network::FilterStatus::Continue;
}

// Network::ConnectionCallbacks
void Filter::onEvent(Network::ConnectionEvent ) {
}

bool Filter::reportToMixer(std::shared_ptr<OtddTestCase> otdd_test){
  if(otdd_test==NULL){
    return false;
  }
  std::string testCase;
  ENVOY_LOG(debug,"--- complete otdd test case ---");
  testCase.append("{");
  testCase.append("\"module\":\"");
  testCase.append(config_.module_name());
  testCase.append("\",");
  testCase.append("\"protocol\":\"");
  testCase.append(config_.protocol());
  testCase.append("\",");
  if(otdd_test->inbound_call_!=NULL){
    ENVOY_LOG(debug,"-- inbound call -- \n -- req -- \n {} \n -- resp --\n {} ", otdd_test->inbound_call_->req_bytes_,otdd_test->inbound_call_->resp_bytes_);
    testCase.append("\"inbound\":"+convertTestCallToJson(otdd_test->inbound_call_)+",");
  }
  testCase.append("\"outbound\":[");
  int tmp=0;
  for(auto const& outbound_call : otdd_test->outbound_calls_) {
    ENVOY_LOG(debug,"-- outbound call -- \n -- req -- {} \n -- resp --\n {} ", outbound_call->req_bytes_,outbound_call->resp_bytes_);
    if(tmp!=0){
      testCase.append(",");
    }
    tmp++;
    testCase.append(convertTestCallToJson(outbound_call));
  }
  testCase.append("]");
  testCase.append("}");

  Grpc::AsyncClientFactoryPtr report_client_factory = Utils::GrpcClientFactoryForCluster(config_.report_cluster(), context_.clusterManager(), context_.scope(), context_.dispatcher().timeSource());
  auto reportTransport = Envoy::Utils::ReportTransport::GetFunc( *report_client_factory, Tracing::NullSpan::instance(), "");
  ::istio::mixer::v1::ReportRequest reportRequest;

  // if index < 0 , it's a message attribute not a global attribute.
  int index = -1;
  reportRequest.add_default_words("otdd.testcase");
  reportRequest.add_default_words(testCase);
  (*(reportRequest.add_attributes())->mutable_strings())[index] = (index-1);
  std::shared_ptr<ReportResponse> response{new ReportResponse()};
  reportTransport(reportRequest,&*response,[response](const Status& status) {
    TransportResult result = TransportStatus(status);
    switch (result) {
      case TransportResult::SUCCESS:
        ENVOY_LOG(info,"report otdd test case to mixer: success ");
        break;
      case TransportResult::RESPONSE_TIMEOUT:
        ENVOY_LOG(info,"report otdd test case to mixer: timeout");
        break;
      case TransportResult::SEND_ERROR:
        ENVOY_LOG(info,"report otdd test case to mixer: send error");
        break;
      case TransportResult::OTHER:
        ENVOY_LOG(info,"report otdd test case to mixer: other error");
        break;
    }
  });

  return true;
}

std::string Filter::convertTestCallToJson(std::shared_ptr<OtddCall> otdd_call){
  if(otdd_call==NULL){
    return "";
  }
  std::string ret;
  std::string base64_req = Base64::encode(otdd_call->req_bytes_.c_str(), otdd_call->req_bytes_.size());
  std::string base64_resp = Base64::encode(otdd_call->resp_bytes_.c_str(), otdd_call->resp_bytes_.size());
  ret.append("{");
  ret.append("\"req\":");
  ret.append("\""+base64_req+"\"");
  ret.append(",\"req_time\":");
  ret.append("\""+std::to_string(otdd_call->req_timestamp_)+"\"");
  ret.append(",\"resp\":");
  ret.append("\""+base64_resp+"\"");
  ret.append(",\"resp_time\":");
  ret.append("\""+std::to_string(otdd_call->resp_timestamp_)+"\"");
  ret.append("}");
  return ret;
}

}  // namespace OtddRecorder
}  // namespace Tcp
}  // namespace Envoy
