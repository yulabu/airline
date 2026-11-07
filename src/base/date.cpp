#include "base/date.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace airline {
namespace {

// 统一锚定在当天 12:00，避开夏令时切换时 00:00 可能不存在的边界情况。
void to_tm(const Date& d, std::tm* out) {
    std::tm t{};
    t.tm_year  = d.y - 1900;
    t.tm_mon   = d.m - 1;
    t.tm_mday  = d.d;
    t.tm_hour  = 12;
    t.tm_min   = 0;
    t.tm_sec   = 0;
    t.tm_isdst = -1;
    *out = t;
}

Date from_tm(const std::tm& t) {
    Date d;
    d.y = t.tm_year + 1900;
    d.m = t.tm_mon + 1;
    d.d = t.tm_mday;
    return d;
}

// 交给 mktime 归一化：2 月 30 日会被推成 3 月 2 日，往返比较即可识别非法日期。
bool normalize(const Date& in, Date* out, std::tm* tm_out) {
    std::tm t;
    to_tm(in, &t);
    std::time_t tt = std::mktime(&t);
    if (tt == static_cast<std::time_t>(-1)) return false;
    *out = from_tm(t);
    if (tm_out) *tm_out = t;
    return true;
}

}  // namespace

bool date_valid(const Date& d) {
    if (d.y < 1900 || d.y > 2999) return false;
    if (d.m < 1 || d.m > 12) return false;
    if (d.d < 1 || d.d > 31) return false;

    Date round;
    if (!normalize(d, &round, nullptr)) return false;
    return round.y == d.y && round.m == d.m && round.d == d.d;
}

int date_weekday(const Date& d) {
    Date n;
    std::tm t;
    if (!normalize(d, &n, &t)) return 0;
    // tm_wday: 0 = 周日 ... 6 = 周六  →  1 = 周一 ... 7 = 周日
    return ((t.tm_wday + 6) % 7) + 1;
}

const char* weekday_name(int wd) {
    static const char* kNames[] = {"", "周一", "周二", "周三", "周四", "周五", "周六", "周日"};
    if (wd < 1 || wd > 7) return "未知";
    return kNames[wd];
}

const char* weekday_short(int wd) {
    static const char* kShort[] = {"", "一", "二", "三", "四", "五", "六", "日"};
    if (wd < 1 || wd > 7) return "?";
    return kShort[wd];
}

Date date_add_days(const Date& d, int days) {
    std::tm t;
    to_tm(d, &t);
    t.tm_mday += days;
    std::time_t tt = std::mktime(&t);
    if (tt == static_cast<std::time_t>(-1)) return d;
    return from_tm(t);
}

int date_diff_days(const Date& a, const Date& b) {
    std::tm ta, tb;
    to_tm(a, &ta);
    to_tm(b, &tb);
    std::time_t tta = std::mktime(&ta);
    std::time_t ttb = std::mktime(&tb);
    double secs = std::difftime(tta, ttb);
    return static_cast<int>(std::lround(secs / 86400.0));
}

Date date_today() {
    std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    return from_tm(t);
}

Date date_next_weekday(const Date& from, int wd) {
    if (wd < 1 || wd > 7) return from;
    int cur = date_weekday(from);
    if (cur < 1) return from;
    int delta = (wd - cur + 7) % 7;
    return date_add_days(from, delta);
}

bool date_from_string(const std::string& s, Date* out) {
    if (!out) return false;
    int y = 0, m = 0, d = 0;
    char extra = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%d%c", &y, &m, &d, &extra) != 3) return false;
    if (s.size() != 10) return false;

    Date t;
    t.y = y;
    t.m = m;
    t.d = d;
    if (!date_valid(t)) return false;
    *out = t;
    return true;
}

std::string date_to_string(const Date& d) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.y, d.m, d.d);
    return std::string(buf);
}

bool date_equal(const Date& a, const Date& b) {
    return a.y == b.y && a.m == b.m && a.d == b.d;
}

std::string datetime_now_string() {
    std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
                  t.tm_sec);
    return std::string(buf);
}

}  // namespace airline
