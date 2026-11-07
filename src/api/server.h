#pragma once

#include <string>

#include "service/airline_service.h"

namespace airline {
namespace api {

// 启动 HTTP 服务器并阻塞当前线程。
// web_dir: 静态资源目录；state_path: 状态文件；每次变更成功后自动落盘。
// class_name/student_id/name: 展示用个人信息（来自 student.env，经 /api/meta 下发）。
int serve(AirlineService& svc, const std::string& web_dir,
          const std::string& state_path, int port, const std::string& class_name,
          const std::string& student_id, const std::string& name);

}  // namespace api
}  // namespace airline
