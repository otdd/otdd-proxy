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

#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "src/envoy/tcp/otdd_recorder/filter.h"
#include "src/envoy/tcp/otdd_recorder/otdd_recorder_config.pb.h"

using ::otddrecorder::config::OtddRecorderConfig;

namespace Envoy {
namespace Server {
namespace Configuration {

class OtddRecorderFilterFactory : public NamedNetworkFilterConfigFactory ,
			public Logger::Loggable<Logger::Id::filter>{
 public:
  Network::FilterFactoryCb createFilterFactory(
      const Json::Object& config_json, FactoryContext& context ) override {
    OtddRecorderConfig config_pb;
    config_pb.set_is_inbound(config_json.getBoolean("is_inbound"));
    config_pb.set_module_name(config_json.getString("module_name"));
    config_pb.set_protocol(config_json.getString("protocol"));
    config_pb.set_report_cluster(config_json.getString("report_cluster"));
    if (config_pb.module_name()==""){
      throw EnvoyException("Failed to parse JSON config, module_name is empty!");
    }
    if (config_pb.protocol()==""){
      throw EnvoyException("Failed to parse JSON config, protocol is empty!");
    }
    if (config_pb.report_cluster()==""){
      throw EnvoyException("Failed to parse JSON config, report_cluster is empty!");
    }
    return createFilterFactory(context,config_pb);
  }

  Network::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message& config, FactoryContext& context ) override {
    return createFilterFactory(context,dynamic_cast<const OtddRecorderConfig&>(config));
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new OtddRecorderConfig};
  }

  std::string name() override { return "otdd.recorder"; }

 private:
  Network::FilterFactoryCb createFilterFactory(FactoryContext& context, const OtddRecorderConfig config_pb) {
    return [config_pb,&context](Network::FilterManager& filter_manager) -> void {
      std::shared_ptr<Tcp::OtddRecorder::Filter> instance =
          std::make_shared<Tcp::OtddRecorder::Filter>(config_pb,context);
      filter_manager.addReadFilter(Network::ReadFilterSharedPtr(instance));
      filter_manager.addWriteFilter(Network::WriteFilterSharedPtr(instance));
    };
  }
};

static Registry::RegisterFactory<OtddRecorderFilterFactory, NamedNetworkFilterConfigFactory>
    register_;

}  // namespace Configuration
}  // namespace Server
}  // namespace Envoy
