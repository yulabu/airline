#pragma once

#include <cstring>

#include "base/date.h"
#include "domain/order.h"
#include "domain/types.h"
#include "domain/waitlist.h"
#include "ds/link_queue.h"
#include "ds/seq_list.h"

namespace airline {

// 航班实例 = 航线 × 日期。库存、座位、订单、候补都挂在这里。
// 由仓储层惰性创建：第一次被访问时才建。
struct FlightInstance {
    char flight_no[kFlightNoLen] = {0};
    Date date;
    int sold[CABIN_COUNT] = {0};

    // 实体座位占用位图：occupied[舱位][排-1][字母-'A']
    unsigned char occupied[CABIN_COUNT][kMaxRows][kMaxLetters];

    SeqList<Order> orders;
    LinkQueue<WaitEntry> wait_lists[CABIN_COUNT];

    int phase = FLIGHT_OPEN;

    FlightInstance() {
        std::memset(occupied, 0, sizeof(occupied));
    }

    bool is_free(int cabin, int row, char letter) const {
        if (cabin < 0 || cabin >= CABIN_COUNT) return false;
        if (row < 1 || row > kMaxRows) return false;
        int col = letter - 'A';
        if (col < 0 || col >= kMaxLetters) return false;
        return occupied[cabin][row - 1][col] == 0;
    }

    void set_occupied(int cabin, int row, char letter, bool used) {
        if (cabin < 0 || cabin >= CABIN_COUNT) return;
        if (row < 1 || row > kMaxRows) return;
        int col = letter - 'A';
        if (col < 0 || col >= kMaxLetters) return;
        occupied[cabin][row - 1][col] = used ? 1 : 0;
    }

    // 该舱位已分配出去的实体座位数
    int seats_taken(int cabin) const {
        if (cabin < 0 || cabin >= CABIN_COUNT) return 0;
        int n = 0;
        for (int r = 0; r < kMaxRows; ++r) {
            for (int c = 0; c < kMaxLetters; ++c) {
                n += occupied[cabin][r][c] ? 1 : 0;
            }
        }
        return n;
    }

    const Order* find_order_by_idcard(const char* id_card) const {
        for (int i = 0; i < orders.size(); ++i) {
            if (orders[i].holds_tickets() &&
                std::strcmp(orders[i].id_card, id_card) == 0) {
                return &orders[i];
            }
        }
        return nullptr;
    }

    int find_order_index(const char* id_card) const {
        for (int i = 0; i < orders.size(); ++i) {
            if (orders[i].holds_tickets() &&
                std::strcmp(orders[i].id_card, id_card) == 0) {
                return i;
            }
        }
        return -1;
    }
};

}  // namespace airline
