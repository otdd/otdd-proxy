# Copyright 2016 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
)

load("@com_google_protobuf//:protobuf.bzl", "cc_proto_library")

envoy_cc_library(
    name = "filter_lib",
    srcs = [
        "filter.cc",
        "filter.h",
        "filter_factory.cc",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//src/envoy/http/jwt_auth:http_filter_lib",
        "//src/envoy/utils:authn_lib",
        "//src/envoy/utils:utils_lib",
        "//src/istio/control/http:control_lib",
        "//src/istio/utils:utils_lib",
        "@envoy//source/exe:envoy_common_lib",
        ":otdd_redirector_config_cc_proto",
    ],
)

cc_proto_library(
    name = "otdd_redirector_config_cc_proto",
    srcs = [
        "otdd_redirector_config.proto",
    ],
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"]
)
