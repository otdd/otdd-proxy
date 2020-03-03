#pragma once

namespace Envoy {
namespace Tcp {
namespace OtddRecorder {
  class OtddCall{
    public:
      std::string req_bytes_;
      uint64_t req_timestamp_;
      std::string resp_bytes_;
      uint64_t resp_timestamp_;
  };
class OtddTestCase{
    public:
      std::shared_ptr<OtddCall> inbound_call_;
      std::vector<std::shared_ptr<OtddCall>> outbound_calls_;
  };
}
}
}
