#pragma once

namespace airline {

// 全系统唯一的错误码集合。
// 任何层都不允许自造错误码，也不允许把业务失败编码成字符串返回。
enum class Status {
    OK = 0,

    // 输入与查找
    INVALID_ARG,        // 参数非法（格式、范围、编号越界）
    NOT_FOUND,          // 目标对象不存在

    // 航线管理
    FLIGHT_EXISTS,      // 航班号已存在
    QUOTA_BELOW_SOLD,   // 新定额低于该舱位已售数量

    // 订票
    NO_SEAT,            // 可售余量不足（含超卖额度已用完）
    SEAT_TAKEN,         // 指定的座位已被占用
    DUPLICATE_ORDER,    // 同一航班同一身份证已有有效订单
    CABIN_MISMATCH,     // 舱位与订单/座位不匹配

    // 退票与候补
    ORDER_NOT_FOUND,    // 订单不存在
    WAITLIST_EMPTY,     // 候补队列为空

    // 登机
    FLIGHT_LOCKED,      // 航班已完成登机，禁止再变更
    NOT_ELIGIBLE,       // 不满足本操作的前置条件

    // 兜底
    INTERNAL,           // 内部错误（内存不足、文件写失败等）
};

// 机器可读码，用于 HTTP 响应和日志，例如 "NO_SEAT"
const char* status_code(Status s);

// 面向用户的提示语
const char* status_message(Status s);

}  // namespace airline
