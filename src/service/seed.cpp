// 内置种子数据：18 条航线（文档要求 ≥15）。
// 覆盖 4 种机型、每日/隔日班次、同城对飞、同目的地多航线（演示推荐功能）。
#include "service/airline_service.h"

namespace airline {
namespace {

struct SeedRoute {
    const char* no;
    const char* from;
    const char* to;
    const char* aircraft;
    int weekdays;  // bit0=周一 ... bit6=周日
    int quota[CABIN_COUNT];
    int overbook[CABIN_COUNT];
    int price[CABIN_COUNT];
};

const int kDaily = kWeekdayMaskAll;

// B737-800: 头等8/商务16/经济156；A320: 8/12/150；A330: 12/24/180；B787-9: 12/28/168
const SeedRoute kSeed[] = {
    {"CZ3101", "广州", "北京", "B737-800", 0b0101011, {8, 16, 150}, {0, 2, 6}, {2600, 1800, 1200}},
    {"CZ3102", "北京", "广州", "B737-800", 0b1010010, {8, 16, 150}, {0, 2, 6}, {2600, 1800, 1200}},
    {"MU5101", "上海", "北京", "A330-300", kDaily, {12, 24, 170}, {0, 3, 8}, {2800, 1900, 1250}},
    {"CA1301", "北京", "上海", "B737-800", kDaily, {8, 16, 150}, {0, 2, 6}, {2600, 1800, 1200}},
    {"HO1201", "深圳", "上海", "A320", 0b0010101, {8, 12, 140}, {0, 2, 5}, {2300, 1600, 1050}},
    {"ZH9601", "深圳", "成都", "A320", kDaily, {8, 12, 140}, {0, 2, 5}, {2200, 1500, 980}},
    {"3U8801", "成都", "厦门", "A320", 0b0001001, {8, 12, 140}, {0, 2, 5}, {2100, 1450, 950}},
    {"HU7181", "海口", "哈尔滨", "B737-800", 0b0010010, {8, 16, 150}, {0, 2, 6}, {3200, 2200, 1500}},
    {"CZ3401", "广州", "西安", "A320", kDaily, {8, 12, 140}, {0, 2, 5}, {2000, 1400, 900}},
    {"MF8101", "厦门", "杭州", "B737-800", 0b0001000, {8, 16, 150}, {0, 2, 6}, {1600, 1150, 750}},
    {"SC4621", "青岛", "武汉", "A320", 0b0000101, {8, 12, 140}, {0, 2, 5}, {1500, 1050, 680}},
    {"MU2733", "南京", "昆明", "A320", 0b0101001, {8, 12, 140}, {0, 2, 5}, {2400, 1650, 1100}},
    {"CA4301", "成都", "杭州", "A330-300", 0b1001001, {12, 24, 170}, {0, 3, 8}, {2300, 1600, 1050}},
    {"CZ3501", "广州", "重庆", "B787-9", 0b1010110, {12, 28, 160}, {0, 3, 8}, {1900, 1350, 880}},
    {"FM9301", "上海", "深圳", "B737-800", kDaily, {8, 16, 150}, {0, 2, 6}, {2300, 1600, 1050}},
    {"GJ8805", "杭州", "长沙", "A320", 0b0000101, {8, 12, 140}, {0, 2, 5}, {1200, 850, 560}},
    {"MU5203", "西安", "北京", "B737-800", 0b0101001, {8, 16, 150}, {0, 2, 6}, {2400, 1650, 1100}},
    {"TV9908", "西安", "拉萨", "A320", 0b0010101, {8, 12, 130}, {0, 0, 0}, {2800, 1950, 1300}},
};

const int kSeedCount = static_cast<int>(sizeof(kSeed) / sizeof(kSeed[0]));

}  // namespace

void AirlineService::seed_standard() {
    std::lock_guard<std::mutex> lock(mu_);

    for (int i = 0; i < kSeedCount; ++i) {
        const SeedRoute& s = kSeed[i];
        Route r;
        std::snprintf(r.flight_no, kFlightNoLen, "%s", s.no);
        std::snprintf(r.from, kCityLen, "%s", s.from);
        std::snprintf(r.to, kCityLen, "%s", s.to);
        std::snprintf(r.aircraft, kAircraftLen, "%s", s.aircraft);
        r.weekdays = s.weekdays;
        for (int c = 0; c < CABIN_COUNT; ++c) {
            r.cabins[c].quota = s.quota[c];
            r.cabins[c].overbook_max = s.overbook[c];
            r.cabins[c].price = s.price[c];
        }
        repo_.add_route(r);
    }
}

}  // namespace airline
