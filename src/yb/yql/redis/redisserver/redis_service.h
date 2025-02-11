// Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//

#ifndef YB_YQL_REDIS_REDISSERVER_REDIS_SERVICE_H_
#define YB_YQL_REDIS_REDISSERVER_REDIS_SERVICE_H_

#include "yb/yql/redis/redisserver/redis_fwd.h"
#include "yb/yql/redis/redisserver/redis_service.service.h"

#include <memory>

namespace yb {
namespace redisserver {

class RedisServer;

class RedisServiceImpl : public RedisServerServiceIf {
 public:
  RedisServiceImpl(RedisServer* server, std::string yb_tier_master_address);
  ~RedisServiceImpl();

  void FillEndpoints(const rpc::RpcServicePtr& service, rpc::RpcEndpointMap* map) override;
  void Handle(yb::rpc::InboundCallPtr call) override;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace redisserver
}  // namespace yb

#endif  // YB_YQL_REDIS_REDISSERVER_REDIS_SERVICE_H_
