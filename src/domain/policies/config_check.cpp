#include <cstring>

#include "domain/policies.h"

namespace airline {

Status route_config_check(const Route& route) {
    if (route.flight_no[0] == '\0') return Status::INVALID_ARG;
    if (std::strlen(route.flight_no) >= static_cast<size_t>(kFlightNoLen)) {
        return Status::INVALID_ARG;
    }
    if (route.from[0] == '\0' || route.to[0] == '\0') return Status::INVALID_ARG;
    if (std::strcmp(route.from, route.to) == 0) return Status::INVALID_ARG;
    if (!weekday_mask_valid(route.weekdays)) return Status::INVALID_ARG;

    SeatLayout layout;
    if (!seat_layout_for(route.aircraft, &layout)) return Status::INVALID_ARG;

    for (int c = 0; c < CABIN_COUNT; ++c) {
        const CabinConfig& cfg = route.cabins[c];
        if (cfg.quota < 0) return Status::INVALID_ARG;
        if (cfg.price <= 0) return Status::INVALID_ARG;
        if (cfg.overbook_max < 0) return Status::INVALID_ARG;
        // 定额不得超过该舱位的物理座位数
        if (cfg.quota > layout.seats_of(c)) return Status::INVALID_ARG;
    }
    return Status::OK;
}

Status quota_change_check(int new_quota, int sold_max, int physical_seats) {
    if (new_quota < 0) return Status::INVALID_ARG;
    // 文档示例：某天已售 40 张，定额就不能改到 40 以下
    if (new_quota < sold_max) return Status::QUOTA_BELOW_SOLD;
    if (new_quota > physical_seats) return Status::INVALID_ARG;
    return Status::OK;
}

Status flight_no_check(const char* flight_no) {
    if (!flight_no || flight_no[0] == '\0') return Status::INVALID_ARG;
    if (std::strlen(flight_no) >= static_cast<size_t>(kFlightNoLen)) {
        return Status::INVALID_ARG;
    }
    return Status::OK;
}

}  // namespace airline
