#pragma once

#include <string>
#include <vector>

#include "base/date.h"
#include "domain/types.h"

namespace airline {

// ── 查询视图 ──────────────────────────────────────────

struct CabinView {
    int cabin = 0;
    int quota = 0;
    int sold = 0;
    int avail = 0;  // 对外展示的余票（不含超卖额度）
    int overbook_max = 0;
    int price = 0;
};

struct RouteView {
    std::string flight_no;
    std::string from;
    std::string to;
    std::string aircraft;
    int weekdays = 0;
    std::string weekday_text;
    std::string next_date;  // 最近班次日期
    CabinView cabins[CABIN_COUNT];
};

// ── 订票 / 退票 / 候补 ────────────────────────────────

struct BookCmd {
    std::string flight_no;
    std::string date;  // 可空 = 最近班次
    std::string name;
    std::string id_card;
    int cabin = CABIN_ECONOMY;
    int qty = 1;
    std::string seat;  // 可空 = 自动分配；超卖部分无实体座位
};

struct BookingOut {
    std::string order_id;
    std::string flight_no;
    std::string date;
    int cabin = 0;
    int qty = 0;
    std::vector<std::string> seats;
    bool seat_pending = false;
    int remaining = 0;      // NO_SEAT 时：该舱位当前可售余量
    int waitlist_size = 0;  // NO_SEAT 时：该舱位候补人数
};

struct RefundCmd {
    std::string flight_no;
    std::string date;
    std::string id_card;
    int qty = 0;  // 0 = 整单退
};

struct PromotedRow {
    std::string name;
    std::string id_card;
    int qty = 0;
    int cabin = 0;
    std::vector<std::string> seats;
    bool seat_pending = false;
};

struct RefundOut {
    std::string order_id;
    int refunded = 0;
    std::vector<PromotedRow> promoted;  // 退票后撮合成功的候补
};

struct WaitCmd {
    std::string flight_no;
    std::string date;
    std::string name;
    std::string id_card;
    int cabin = CABIN_ECONOMY;
    int qty = 1;
};

struct WaitOut {
    std::string flight_no;
    std::string date;
    int cabin = 0;
    int qty = 0;
    int position = 0;  // 该舱位候补队列中的位次（从 1 起）
};

// ── 座位图 ────────────────────────────────────────────

struct SeatCell {
    int row = 0;
    char letter = 0;
    bool occupied = false;
};

struct SeatMapView {
    std::string flight_no;
    std::string date;
    int cabin = 0;
    std::string aircraft;
    int row_start = 0;
    int row_end = 0;
    std::vector<std::string> letters;
    std::vector<SeatCell> cells;
};

// ── 管理员 ────────────────────────────────────────────

struct RouteCmd {
    std::string flight_no;
    std::string from;
    std::string to;
    std::string aircraft;
    int weekdays = 0;
    CabinConfig cabins[CABIN_COUNT];
};

struct RecordRow {
    std::string flight_no;
    std::string date;
    std::string order_id;
    std::string name;
    std::string id_card;
    int cabin = 0;
    int qty = 0;
    int price = 0;
    std::vector<std::string> seats;
    bool seat_pending = false;
    long book_seq = 0;
    std::string book_time;
    int status = 0;
};

struct UpgradeRow {
    std::string order_id;
    std::string name;
    int from_cabin = 0;
    int to_cabin = 0;
    int qty = 0;
    std::vector<std::string> seats;
};

struct ForfeitRow {
    std::string order_id;
    std::string name;
    int cabin = 0;
    int qty = 0;
    long book_seq = 0;
};

struct CheckinOut {
    std::string flight_no;
    std::string date;
    std::vector<UpgradeRow> upgrades;
    std::vector<ForfeitRow> forfeits;
    int boarded[CABIN_COUNT] = {0};   // 调整后各舱位登机人数
    int physical[CABIN_COUNT] = {0};  // 各舱位物理座位数
};

}  // namespace airline
