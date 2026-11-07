#include <cstring>

#include "domain/policies.h"
#include "service/airline_service.h"

namespace airline {
namespace {

Status parse_cabin_qty(int cabin, int qty) {
    if (cabin < 0 || cabin >= CABIN_COUNT) return Status::INVALID_ARG;
    if (qty < 1 || qty > kMaxSeatsPerOrder) return Status::INVALID_ARG;
    return Status::OK;
}

Status parse_name_idcard(const std::string& name, const std::string& id_card) {
    if (name.empty() || name.size() >= static_cast<size_t>(kNameLen)) {
        return Status::INVALID_ARG;
    }
    if (id_card.size() != 18) return Status::INVALID_ARG;
    return Status::OK;
}

}  // namespace

Result<BookingOut> AirlineService::book(const BookCmd& cmd) {
    std::lock_guard<std::mutex> lock(mu_);

    // ── 第一段：全部校验（只读，可失败） ─────────────────
    Status st = parse_name_idcard(cmd.name, cmd.id_card);
    if (st != Status::OK) return Result<BookingOut>::failure(st);

    st = parse_cabin_qty(cmd.cabin, cmd.qty);
    if (st != Status::OK) return Result<BookingOut>::failure(st);

    Route* route = require_route(cmd.flight_no, &st);
    if (!route) return Result<BookingOut>::failure(st);

    Date d;
    if (cmd.date.empty()) {
        d = route->next_date(today_);
    } else if (!date_from_string(cmd.date, &d)) {
        return Result<BookingOut>::failure(Status::INVALID_ARG);
    }

    FlightInstance* inst = repo_.get_or_create_instance(route->flight_no, d);
    if (!inst) return Result<BookingOut>::failure(Status::INTERNAL);

    if (inst->phase == FLIGHT_LOCKED) {
        return Result<BookingOut>::failure(Status::FLIGHT_LOCKED);
    }

    // 一人一航班一单
    if (inst->find_order_by_idcard(cmd.id_card.c_str()) != nullptr) {
        return Result<BookingOut>::failure(Status::DUPLICATE_ORDER);
    }

    // 可售余量（含超卖额度）不足 → NO_SEAT，附上下文供前端转候补/推荐
    if (avail_sellable(*route, *inst, cmd.cabin) < cmd.qty) {
        BookingOut out;
        out.flight_no = route->flight_no;
        out.date = date_to_string(d);
        out.cabin = cmd.cabin;
        out.qty = cmd.qty;
        out.remaining = avail_sellable(*route, *inst, cmd.cabin);
        out.waitlist_size = inst->wait_lists[cmd.cabin].size();
        return Result<BookingOut>::failure_with(Status::NO_SEAT, std::move(out));
    }

    SeatLayout layout;
    if (!seat_layout_for(route->aircraft, &layout)) {
        return Result<BookingOut>::failure(Status::INTERNAL);
    }

    char seats[kMaxSeatsPerOrder][kSeatCodeLen];
    int assigned = 0;
    if (!cmd.seat.empty()) {
        st = seat_alloc(layout, *inst, cmd.cabin, cmd.qty, cmd.seat.c_str(), seats,
                        &assigned);
        if (st != Status::OK) return Result<BookingOut>::failure(st);
    } else {
        st = seat_alloc(layout, *inst, cmd.cabin, cmd.qty, "", seats, &assigned);
        if (st != Status::OK) return Result<BookingOut>::failure(st);
    }
    // assigned == 0：实体座位不够 → 整单超卖待定（登机时调整）

    // ── 第二段：变更（不可能失败） ───────────────────────
    Order o;
    long seq = repo_.next_order_seq();
    repo_.make_order_id(seq, o.order_id, kOrderIdLen);
    std::strncpy(o.name, cmd.name.c_str(), kNameLen - 1);
    std::strncpy(o.id_card, cmd.id_card.c_str(), kIdCardLen - 1);
    o.cabin = cmd.cabin;
    o.qty = cmd.qty;
    o.seat_count = assigned;
    o.seat_pending = (assigned == 0);
    o.price_at_booking = route->cabins[cmd.cabin].price;
    o.book_seq = seq;
    std::strncpy(o.book_time, datetime_now_string().c_str(), kTimeLen - 1);
    o.status = ORDER_ACTIVE;

    for (int k = 0; k < assigned; ++k) {
        std::strncpy(o.seats[k], seats[k], kSeatCodeLen);
        int row = 0;
        char letter = 0;
        seat_code_parse(seats[k], &row, &letter);
        inst->set_occupied(cmd.cabin, row, letter, true);
    }

    if (inst->orders.push_back(o) != Status::OK) {
        return Result<BookingOut>::failure(Status::INTERNAL);
    }
    inst->sold[cmd.cabin] += cmd.qty;

    // ── 输出 ─────────────────────────────────────────────
    BookingOut out;
    out.order_id = o.order_id;
    out.flight_no = route->flight_no;
    out.date = date_to_string(d);
    out.cabin = cmd.cabin;
    out.qty = cmd.qty;
    out.seat_pending = o.seat_pending;
    for (int k = 0; k < assigned; ++k) out.seats.push_back(std::string(o.seats[k]));
    return Result<BookingOut>::success(std::move(out));
}

Result<RefundOut> AirlineService::refund(const RefundCmd& cmd) {
    std::lock_guard<std::mutex> lock(mu_);

    Route* route = require_route(cmd.flight_no, nullptr);
    if (!route) return Result<RefundOut>::failure(Status::NOT_FOUND);

    Date d;
    if (cmd.date.empty()) {
        d = route->next_date(today_);
    } else if (!date_from_string(cmd.date, &d)) {
        return Result<RefundOut>::failure(Status::INVALID_ARG);
    }

    FlightInstance* inst = repo_.find_instance(route->flight_no, d);
    if (!inst) return Result<RefundOut>::failure(Status::ORDER_NOT_FOUND);

    if (inst->phase == FLIGHT_LOCKED) {
        return Result<RefundOut>::failure(Status::FLIGHT_LOCKED);
    }

    if (cmd.id_card.size() != 18) {
        return Result<RefundOut>::failure(Status::INVALID_ARG);
    }

    int idx = inst->find_order_index(cmd.id_card.c_str());
    if (idx < 0) return Result<RefundOut>::failure(Status::ORDER_NOT_FOUND);

    Order& o = inst->orders.at(idx);
    int n = (cmd.qty <= 0 || cmd.qty >= o.qty) ? o.qty : cmd.qty;

    SeatLayout layout;
    if (!seat_layout_for(route->aircraft, &layout)) {
        return Result<RefundOut>::failure(Status::INTERNAL);
    }

    // ── 变更：释放尾部 n 张的座位，收缩订单 ──────────────
    for (int k = 0; k < n; ++k) {
        int pos = o.seat_count - 1;
        if (pos < 0) break;
        int row = 0;
        char letter = 0;
        if (seat_code_parse(o.seats[pos], &row, &letter)) {
            inst->set_occupied(o.cabin, row, letter, false);
        }
        o.seats[pos][0] = '\0';
        --o.seat_count;
    }

    o.qty -= n;
    o.refunded += n;
    if (o.qty == 0) {
        o.status = ORDER_REFUNDED;
    } else if (o.qty > 0) {
        o.status = ORDER_PARTIAL_REFUND;
    }
    inst->sold[o.cabin] -= n;

    RefundOut out;
    out.order_id = o.order_id;
    out.refunded = n;

    // ── 候补撮合：同舱位队列，"首个能满足者优先" ─────────
    LinkQueue<WaitEntry>& queue = inst->wait_lists[o.cabin];
    int remaining = n;
    while (remaining > 0) {
        int widx = -1;
        WaitEntry e;
        if (!waitlist_find(queue, remaining, &widx, &e)) break;

        if (queue.remove_at(widx, &e) != Status::OK) break;

        // 为候补者建单：有实体座位就分配，没有就 pending（超卖）
        char wseats[kMaxSeatsPerOrder][kSeatCodeLen];
        int wassigned = 0;
        seat_alloc(layout, *inst, e.cabin, e.qty, "", wseats, &wassigned);

        Order w;
        long seq = repo_.next_order_seq();
        repo_.make_order_id(seq, w.order_id, kOrderIdLen);
        std::strncpy(w.name, e.name, kNameLen - 1);
        std::strncpy(w.id_card, e.id_card, kIdCardLen - 1);
        w.cabin = e.cabin;
        w.qty = e.qty;
        w.seat_count = wassigned;
        w.seat_pending = (wassigned == 0);
        w.price_at_booking = route->cabins[e.cabin].price;
        w.book_seq = seq;
        std::strncpy(w.book_time, datetime_now_string().c_str(), kTimeLen - 1);
        w.status = ORDER_ACTIVE;

        for (int k = 0; k < wassigned; ++k) {
            std::strncpy(w.seats[k], wseats[k], kSeatCodeLen);
            int row = 0;
            char letter = 0;
            seat_code_parse(wseats[k], &row, &letter);
            inst->set_occupied(e.cabin, row, letter, true);
        }

        PromotedRow pr;
        pr.name = e.name;
        pr.id_card = e.id_card;
        pr.qty = e.qty;
        pr.cabin = e.cabin;
        pr.seat_pending = w.seat_pending;
        for (int k = 0; k < wassigned; ++k) pr.seats.push_back(std::string(w.seats[k]));
        out.promoted.push_back(pr);

        if (inst->orders.push_back(w) != Status::OK) break;
        inst->sold[e.cabin] += e.qty;
        remaining -= e.qty;
    }

    return Result<RefundOut>::success(std::move(out));
}

Result<WaitOut> AirlineService::join_waitlist(const WaitCmd& cmd) {
    std::lock_guard<std::mutex> lock(mu_);

    Status st = parse_name_idcard(cmd.name, cmd.id_card);
    if (st != Status::OK) return Result<WaitOut>::failure(st);

    st = parse_cabin_qty(cmd.cabin, cmd.qty);
    if (st != Status::OK) return Result<WaitOut>::failure(st);

    Route* route = require_route(cmd.flight_no, &st);
    if (!route) return Result<WaitOut>::failure(st);

    Date d;
    if (cmd.date.empty()) {
        d = route->next_date(today_);
    } else if (!date_from_string(cmd.date, &d)) {
        return Result<WaitOut>::failure(Status::INVALID_ARG);
    }

    FlightInstance* inst = repo_.get_or_create_instance(route->flight_no, d);
    if (!inst) return Result<WaitOut>::failure(Status::INTERNAL);

    if (inst->phase == FLIGHT_LOCKED) {
        return Result<WaitOut>::failure(Status::FLIGHT_LOCKED);
    }

    WaitEntry e;
    std::strncpy(e.name, cmd.name.c_str(), kNameLen - 1);
    std::strncpy(e.id_card, cmd.id_card.c_str(), kIdCardLen - 1);
    e.cabin = cmd.cabin;
    e.qty = cmd.qty;
    e.join_seq = repo_.next_wait_seq();
    std::strncpy(e.join_time, datetime_now_string().c_str(), kTimeLen - 1);

    LinkQueue<WaitEntry>& queue = inst->wait_lists[cmd.cabin];
    if (queue.enqueue(e) != Status::OK) {
        return Result<WaitOut>::failure(Status::INTERNAL);
    }

    WaitOut out;
    out.flight_no = route->flight_no;
    out.date = date_to_string(d);
    out.cabin = cmd.cabin;
    out.qty = cmd.qty;
    out.position = queue.size();
    return Result<WaitOut>::success(std::move(out));
}

}  // namespace airline
