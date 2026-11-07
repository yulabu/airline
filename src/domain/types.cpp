#include "domain/types.h"

namespace airline {

const char* cabin_name(int cabin) {
    switch (cabin) {
        case CABIN_FIRST:    return "头等舱";
        case CABIN_BUSINESS: return "商务舱";
        case CABIN_ECONOMY:  return "经济舱";
        default:             return "未知舱位";
    }
}

const char* cabin_short(int cabin) {
    switch (cabin) {
        case CABIN_FIRST:    return "头等";
        case CABIN_BUSINESS: return "商务";
        case CABIN_ECONOMY:  return "经济";
        default:             return "?";
    }
}

const char* order_status_name(int status) {
    switch (status) {
        case ORDER_ACTIVE:         return "有效";
        case ORDER_PARTIAL_REFUND: return "部分退票";
        case ORDER_REFUNDED:       return "已退票";
        case ORDER_FORCE_REFUNDED: return "强制退票";
        case ORDER_BOARDED:        return "已登机";
        case ORDER_UPGRADED:       return "已升舱";
        default:                   return "未知";
    }
}

const char* flight_phase_name(int phase) {
    switch (phase) {
        case FLIGHT_OPEN:   return "售票中";
        case FLIGHT_LOCKED: return "已登机";
        default:            return "未知";
    }
}

bool weekday_mask_valid(int mask) {
    return mask > 0 && (mask & ~kWeekdayMaskAll) == 0;
}

bool weekday_mask_contains(int mask, int weekday) {
    if (weekday < 1 || weekday > 7) return false;
    return (mask & (1 << (weekday - 1))) != 0;
}

}  // namespace airline
