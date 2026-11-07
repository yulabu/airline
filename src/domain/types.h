#pragma once

namespace airline {

// ── 舱位 ──────────────────────────────────────────────
// 数值顺序即升舱方向：经济舱(2) → 商务舱(1) → 头等舱(0)
enum Cabin {
    CABIN_FIRST = 0,
    CABIN_BUSINESS = 1,
    CABIN_ECONOMY = 2,
    CABIN_COUNT = 3
};

const char* cabin_name(int cabin);   // "头等舱"
const char* cabin_short(int cabin);  // "头等"

// ── 订单状态 ──────────────────────────────────────────
enum OrderStatus {
    ORDER_ACTIVE = 0,        // 有效
    ORDER_PARTIAL_REFUND,    // 部分退票后仍有余额
    ORDER_REFUNDED,          // 主动退票，已全部退完
    ORDER_FORCE_REFUNDED,    // 超卖登机时被强制退票
    ORDER_BOARDED,           // 已登机
    ORDER_UPGRADED           // 升舱后登机
};

const char* order_status_name(int status);

// ── 航班阶段 ──────────────────────────────────────────
enum FlightPhase {
    FLIGHT_OPEN = 0,   // 正常售票
    FLIGHT_LOCKED = 1  // 已完成登机，禁止任何变更
};

const char* flight_phase_name(int phase);

// ── 容量常量 ──────────────────────────────────────────
const int kMaxSeatsPerOrder = 6;   // 单次订票上限
const int kMaxRows = 40;           // 座位布局最大排数
const int kMaxLetters = 6;         // 每排最大座位数

const int kFlightNoLen = 8;
const int kCityLen = 24;
const int kAircraftLen = 16;
const int kNameLen = 32;
const int kIdCardLen = 20;
const int kSeatCodeLen = 4;        // "12A"
const int kOrderIdLen = 16;
const int kTimeLen = 20;           // "2026-11-06 14:30:00"

// 飞行日掩码：bit0 = 周一 ... bit6 = 周日
const int kWeekdayMaskAll = 0x7F;

bool weekday_mask_valid(int mask);
bool weekday_mask_contains(int mask, int weekday);  // weekday: 1..7

}  // namespace airline
