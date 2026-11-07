#pragma once

#include "base/date.h"
#include "domain/seat_layout.h"
#include "domain/types.h"

namespace airline {

// 单个舱位的配置。管理员修改的是 quota，余票是派生值，不允许直接改。
struct CabinConfig {
    int quota = 0;         // 售票定额（不得超过该舱位的物理座位数）
    int overbook_max = 0;  // 允许超出定额售出的张数（超卖阈值）
    int price = 0;         // 票价（元）
};

struct Route {
    char flight_no[kFlightNoLen] = {0};
    char from[kCityLen] = {0};
    char to[kCityLen] = {0};
    char aircraft[kAircraftLen] = {0};
    int weekdays = 0;  // bit0=周一 ... bit6=周日
    CabinConfig cabins[CABIN_COUNT];

    // 乘员定额 = 各舱位定额之和
    int capacity() const;

    bool flies_on(int weekday) const;

    // 本周期的下一个航班日期（含 today 当天）
    Date next_date(const Date& today) const;

    // "周一/周三"
    void weekday_text(char* out, int out_len) const;
};

}  // namespace airline
