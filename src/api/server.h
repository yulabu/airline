#pragma once

#include <string>

#include "service/airline_service.h"

namespace airline {
namespace api {

// 启动 HTTP 服务器并阻塞当前线程。
// web_dir: 静态资源目录；state_path: 状态文件；每次变更成功后自动落盘。
int serve(AirlineService& svc, const std::string& web_dir,
          const std::string& state_path, int port);

}  // namespace api
}  // namespace airline
