#pragma once

#include <string>

namespace airline {

// 纯公历日期，不带时区与时刻。
// 航班库存按天划分，所以日期运算必须准确。
struct Date {
    int y = 1970;
    int m = 1;
    int d = 1;
};

bool date_valid(const Date& d);

// 1 = 周一 ... 7 = 周日
int date_weekday(const Date& d);

const char* weekday_name(int wd);          // "周一" .. "周日"
const char* weekday_short(int wd);         // "一" .. "日"

Date date_add_days(const Date& d, int days);

// a - b，单位天
int date_diff_days(const Date& a, const Date& b);

Date date_today();

// 从 from 当天起（含）的下一个 wd，wd 取值 1..7
Date date_next_weekday(const Date& from, int wd);

// "YYYY-MM-DD"
bool date_from_string(const std::string& s, Date* out);
std::string date_to_string(const Date& d);

bool date_equal(const Date& a, const Date& b);

// "YYYY-MM-DD HH:MM:SS"，本地时间
std::string datetime_now_string();

}  // namespace airline
