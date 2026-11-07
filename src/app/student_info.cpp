#include "app/student_info.h"

#include <fstream>

namespace airline {
namespace app {
namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

}  // namespace

StudentInfo load_student_info(const std::string& path) {
    StudentInfo info;

    std::ifstream in(path);
    if (!in.good()) return info;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF

        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        if (val.empty()) continue;

        if (key == "CLASS") {
            info.class_name = val;
            info.loaded = true;
        } else if (key == "STUDENT_ID") {
            info.student_id = val;
            info.loaded = true;
        } else if (key == "NAME") {
            info.name = val;
            info.loaded = true;
        }
    }
    return info;
}

}  // namespace app
}  // namespace airline
