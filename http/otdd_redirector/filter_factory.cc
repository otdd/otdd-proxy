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

#include "common/config/utility.h"
#include "envoy/json/json_object.h"
#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"
#include "src/envoy/http/otdd_redirector/filter.h"
#include "src/envoy/utils/config.h"

using ::otdd::config::OtddRedirectorConfig;

namespace Envoy {
namespace Server {
namespace Configuration {

class OtddRedirectorConfigFactory : public NamedHttpFilterConfigFactory {
 public:
  Http::FilterFactoryCb createFilterFactory(const Json::Object& config_json,
                                            const std::string& prefix,
                                            FactoryContext& context) override {
    OtddRedirectorConfig config_pb;
    config_pb.set_target_cluster(config_json.getString("target_cluster"));
    config_pb.set_interval(config_json.getInteger("interval"));
    if (config_pb.target_cluster()==""){
      throw EnvoyException("Failed to parse JSON config, report_cluster is empty!");
    }
    return createFilterFactory(config_pb, prefix, context);
  }

  Http::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message& proto_config, const std::string& prefix,
      FactoryContext& context) override {
    return createFilterFactory(
        dynamic_cast<const OtddRedirectorConfig&>(proto_config), prefix, context);
  }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new OtddRedirectorConfig};
  }

  std::string name() override { return "otdd.redirector"; }

 private:
  Http::FilterFactoryCb createFilterFactory(const OtddRedirectorConfig config_pb,
                                            const std::string&,
                                            FactoryContext& context) {
    return [config_pb,&context](
               Http::FilterChainFactoryCallbacks& callbacks) -> void {
      std::shared_ptr<Http::OtddRedirector::Filter> instance =
          std::make_shared<Http::OtddRedirector::Filter>(config_pb,context);
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr(instance));
    };
  }
};

static Registry::RegisterFactory<OtddRedirectorConfigFactory,
                                 NamedHttpFilterConfigFactory>
    register_;

}  // namespace Configuration
}  // namespace Server
}  // namespace Envoy
