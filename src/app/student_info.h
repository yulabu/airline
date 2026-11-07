#pragma once

#include <string>

namespace airline {
namespace app {

// 展示用个人信息：启动横幅与网页页头都从这里取。
// 出于信息安全考虑，真实值放在 student.env（已被 .gitignore 排除），
// 仓库里只有模板 student.env.example。
struct StudentInfo {
    std::string class_name = "________";
    std::string student_id = "________";
    std::string name = "________";
    bool loaded = false;
};

// 从 path 读取 KEY=VALUE 配置，识别 CLASS / STUDENT_ID / NAME。
// 支持 # 注释与 CRLF 行尾；文件不存在时返回未配置状态（占位下划线）。
StudentInfo load_student_info(const std::string& path);

}  // namespace app
}  // namespace airline
