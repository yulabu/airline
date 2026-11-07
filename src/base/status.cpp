#include "base/status.h"

namespace airline {

const char* status_code(Status s) {
    switch (s) {
        case Status::OK:               return "OK";
        case Status::INVALID_ARG:      return "INVALID_ARG";
        case Status::NOT_FOUND:        return "NOT_FOUND";
        case Status::FLIGHT_EXISTS:    return "FLIGHT_EXISTS";
        case Status::QUOTA_BELOW_SOLD: return "QUOTA_BELOW_SOLD";
        case Status::NO_SEAT:          return "NO_SEAT";
        case Status::SEAT_TAKEN:       return "SEAT_TAKEN";
        case Status::DUPLICATE_ORDER:  return "DUPLICATE_ORDER";
        case Status::CABIN_MISMATCH:   return "CABIN_MISMATCH";
        case Status::ORDER_NOT_FOUND:  return "ORDER_NOT_FOUND";
        case Status::WAITLIST_EMPTY:   return "WAITLIST_EMPTY";
        case Status::FLIGHT_LOCKED:    return "FLIGHT_LOCKED";
        case Status::NOT_ELIGIBLE:     return "NOT_ELIGIBLE";
        case Status::INTERNAL:         return "INTERNAL";
    }
    return "INTERNAL";
}

const char* status_message(Status s) {
    switch (s) {
        case Status::OK:               return "操作成功";
        case Status::INVALID_ARG:      return "输入参数不合法";
        case Status::NOT_FOUND:        return "对象不存在";
        case Status::FLIGHT_EXISTS:    return "该航班号已存在";
        case Status::QUOTA_BELOW_SOLD: return "新定额不能小于该舱位已售数量";
        case Status::NO_SEAT:          return "该舱位余票不足";
        case Status::SEAT_TAKEN:       return "指定座位已被占用";
        case Status::DUPLICATE_ORDER:  return "该乘客在此航班已有有效订单";
        case Status::CABIN_MISMATCH:   return "舱位不匹配";
        case Status::ORDER_NOT_FOUND:  return "未找到该乘客的有效订单";
        case Status::WAITLIST_EMPTY:   return "候补队列为空";
        case Status::FLIGHT_LOCKED:    return "该航班已完成登机，不能再变更";
        case Status::NOT_ELIGIBLE:     return "不满足本操作的前置条件";
        case Status::INTERNAL:         return "系统内部错误";
    }
    return "系统内部错误";
}

}  // namespace airline
