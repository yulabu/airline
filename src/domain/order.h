#pragma once

#include "domain/types.h"

namespace airline {

// 订单 = 一次订票行为。同航班的同一身份证只允许一个有效订单。
struct Order {
    char order_id[kOrderIdLen] = {0};
    char name[kNameLen] = {0};
    char id_card[kIdCardLen] = {0};

    int cabin = CABIN_ECONOMY;
    int qty = 0;        // 当前持票张数
    int refunded = 0;   // 累计已退张数

    // 座位号：长度 = seat_count。超卖时 seat_count = 0 且 seat_pending = true
    char seats[kMaxSeatsPerOrder][kSeatCodeLen] = {{0}};
    int seat_count = 0;
    bool seat_pending = false;

    int price_at_booking = 0;  // 下单时的票价快照，管理员改价不影响历史订单
    long book_seq = 0;         // 订票序号，强制退票的唯一依据
    char book_time[kTimeLen] = {0};
    int status = ORDER_ACTIVE;

    // 是否仍占用运力（可登机）
    bool holds_tickets() const {
        return status == ORDER_ACTIVE || status == ORDER_PARTIAL_REFUND ||
               status == ORDER_BOARDED || status == ORDER_UPGRADED;
    }
};

}  // namespace airline
