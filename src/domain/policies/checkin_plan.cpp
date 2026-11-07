#include <cstring>

#include "domain/policies.h"

namespace airline {
namespace {

struct Cand {
    long seq;
    int index;
};

// 订票序号倒序：最后订票的排最前，用于"最后订票者先被处理"
// 订单本来就是按序号追加的，这里用插入排序，接近有序时接近 O(n)
void sort_desc(Cand* a, int n) {
    for (int i = 1; i < n; ++i) {
        Cand key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j].seq < key.seq) {
            a[j + 1] = a[j];
            --j;
        }
        a[j + 1] = key;
    }
}

}  // namespace

Status plan_checkin(const Route& route, const FlightInstance& inst, CheckinPlan* out) {
    if (!out) return Status::INVALID_ARG;

    SeatLayout layout;
    if (!seat_layout_for(route.aircraft, &layout)) return Status::INTERNAL;

    const int n = inst.orders.size();

    for (int c = 0; c < CABIN_COUNT; ++c) {
        out->physical[c] = layout.seats_of(c);
        out->boarded[c] = 0;
    }

    // ── 1. 统计各舱位要登机的人数 ─────────────────────────
    for (int i = 0; i < n; ++i) {
        const Order& o = inst.orders[i];
        if (!o.holds_tickets()) continue;
        if (o.cabin < 0 || o.cabin >= CABIN_COUNT) continue;
        out->boarded[o.cabin] += o.qty;
    }

    // ── 2. 计划内的座位分配用本地占用副本，不回写 inst ────
    unsigned char used[CABIN_COUNT][kMaxRows][kMaxLetters];
    std::memcpy(used, inst.occupied, sizeof(used));

    auto free_in = [&](int cabin) {
        const CabinLayout& cl = layout.cabin[cabin];
        int cnt = 0;
        for (int r = cl.row_start; r <= cl.row_end; ++r) {
            for (int i = 0; i < cl.seats_per_row; ++i) {
                if (!used[cabin][r - 1][cl.letters[i] - 'A']) ++cnt;
            }
        }
        return cnt;
    };

    auto alloc_in = [&](int cabin, int qty, char seats[][kSeatCodeLen]) {
        const CabinLayout& cl = layout.cabin[cabin];
        int got = 0;
        for (int r = cl.row_start; r <= cl.row_end && got < qty; ++r) {
            for (int i = 0; i < cl.seats_per_row && got < qty; ++i) {
                char letter = cl.letters[i];
                if (used[cabin][r - 1][letter - 'A']) continue;
                seat_code_make(r, letter, seats[got]);
                used[cabin][r - 1][letter - 'A'] = 1;
                ++got;
            }
        }
        return got;
    };

    // ── 3. 收集各舱位的"超卖待定"订单，按订票序号倒序 ─────
    SeqList<Cand> cands[CABIN_COUNT];
    SeqList<char> upgraded;
    for (int i = 0; i < n; ++i) upgraded.push_back(0);

    for (int i = 0; i < n; ++i) {
        const Order& o = inst.orders[i];
        if (!o.holds_tickets() || !o.seat_pending) continue;
        if (o.cabin < 0 || o.cabin >= CABIN_COUNT) continue;
        cands[o.cabin].push_back(Cand{o.book_seq, i});
    }
    for (int c = 0; c < CABIN_COUNT; ++c) {
        sort_desc(cands[c].data(), cands[c].size());
    }

    int cursor[CABIN_COUNT] = {0};
    bool alloc_failed = false;

    // 把 from 舱的待定订单整单升到 to 舱，最多转移 want 人
    auto upgrade_from = [&](int from, int to, int want) -> int {
        int moved = 0;
        while (moved < want && cursor[from] < cands[from].size()) {
            const Cand& cd = cands[from].at(cursor[from]);
            const Order& o = inst.orders[cd.index];

            // 整单转移，装不下就跳过这一单、试下一个（可能张数更少）
            if (o.qty > want - moved) {
                ++cursor[from];
                continue;
            }
            if (free_in(to) < o.qty) break;

            UpgradeMove mv;
            mv.order_index = cd.index;
            mv.to_cabin = to;
            mv.seat_count = alloc_in(to, o.qty, mv.seats);
            if (mv.seat_count < o.qty) break;

            if (out->upgrades.push_back(mv) != Status::OK) {
                alloc_failed = true;
                return moved;
            }
            upgraded.at(cd.index) = 1;
            moved += o.qty;
            ++cursor[from];
        }
        return moved;
    };

    // ── 4. 先清上方舱位：商务舱超卖 → 头等舱 ──────────────
    //     先把商务舱腾到不超卖，才能腾出座位接经济舱的人
    int biz_excess = out->boarded[CABIN_BUSINESS] - out->physical[CABIN_BUSINESS];
    if (biz_excess > 0) {
        int first_room = out->physical[CABIN_FIRST] - out->boarded[CABIN_FIRST];
        if (first_room > 0) {
            int moved = upgrade_from(CABIN_BUSINESS, CABIN_FIRST,
                                     biz_excess < first_room ? biz_excess : first_room);
            out->boarded[CABIN_BUSINESS] -= moved;
            out->boarded[CABIN_FIRST] += moved;
        }
    }

    // ── 5. 再接下方：经济舱超卖 → 商务舱 ─────────────────
    int eco_excess = out->boarded[CABIN_ECONOMY] - out->physical[CABIN_ECONOMY];
    if (eco_excess > 0) {
        int biz_room = out->physical[CABIN_BUSINESS] - out->boarded[CABIN_BUSINESS];
        if (biz_room > 0) {
            int moved = upgrade_from(CABIN_ECONOMY, CABIN_BUSINESS,
                                     eco_excess < biz_room ? eco_excess : biz_room);
            out->boarded[CABIN_ECONOMY] -= moved;
            out->boarded[CABIN_BUSINESS] += moved;
        }
    }

    // ── 6. 仍无座的订单必须强制退票，按订票序号倒序 ────────
    if (alloc_failed) return Status::INTERNAL;

    for (int c = 0; c < CABIN_COUNT; ++c) {
        for (int k = 0; k < cands[c].size(); ++k) {
            int idx = cands[c].at(k).index;
            if (upgraded.at(idx)) continue;
            if (out->forfeits.push_back(idx) != Status::OK) return Status::INTERNAL;
        }
    }

    return Status::OK;
}

}  // namespace airline
