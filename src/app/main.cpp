#include <cstdio>
#include <cstring>
#include <string>

#include "api/server.h"
#include "app/student_info.h"
#include "base/date.h"
#include "service/airline_service.h"
#include "store/store.h"

int main(int argc, char** argv) {
    int port = 8080;
    std::string web_dir = "web";
    std::string state_path = "state.json";
    std::string student_env = "student.env";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--web") == 0 && i + 1 < argc) {
            web_dir = argv[++i];
        } else if (std::strcmp(argv[i], "--state") == 0 && i + 1 < argc) {
            state_path = argv[++i];
        } else if (std::strcmp(argv[i], "--student") == 0 && i + 1 < argc) {
            student_env = argv[++i];
        }
    }

    airline::AirlineService svc;

    // 已有状态则恢复；否则灌入内置种子（文档要求启动时 ≥15 条航线）
    if (!airline::store::load(svc.repo(), state_path)) {
        svc.seed_standard();
        airline::store::flush(svc.repo(), state_path);
        std::printf("[init] 已载入内置种子航线，共 %d 条\n", svc.repo().route_count());
    } else {
        std::printf("[init] 已从 %s 恢复状态，共 %d 条航线\n", state_path.c_str(),
                    svc.repo().route_count());
    }

    // 个人信息：从 student.env 读取（不入库、不上传）
    airline::app::StudentInfo stu = airline::app::load_student_info(student_env);
    if (!stu.loaded) {
        std::printf(
            "[warn] 未读到 %s，界面将显示占位符 —— 请复制 student.env.example "
            "为 student.env 并填写\n",
            student_env.c_str());
    }

    std::printf(
        "================================================\n"
        "          航空客运订票系统\n"
        "          班级：%s   学号：%s\n"
        "          姓名：%s\n"
        "================================================\n"
        "[run ] http://localhost:%d   (web: %s)\n",
        stu.class_name.c_str(), stu.student_id.c_str(), stu.name.c_str(), port,
        web_dir.c_str());

    return airline::api::serve(svc, web_dir, state_path, port, stu.class_name,
                               stu.student_id, stu.name);
}
