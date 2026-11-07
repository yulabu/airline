#include <cstring>

#include "domain/policies.h"
#include "service/airline_service.h"

namespace airline {
namespace {

void fill_route_from_cmd(Route* r, const RouteCmd& cmd) {
    std::memset(r->flight_no, 0, kFlightNoLen);
    std::snprintf(r->flight_no, kFlightNoLen, "%s", cmd.flight_no.c_str());
    std::snprintf(r->from, kCityLen, "%s", cmd.from.c_str());
    std::snprintf(r->to, kCityLen, "%s", cmd.to.c_str());
    std::snprintf(r->aircraft, kAircraftLen, "%s", cmd.aircraft.c_str());
    r->weekdays = cmd.weekdays;
    for (int c = 0; c < CABIN_COUNT; ++c) r->cabins[c] = cmd.cabins[c];
}

}  // namespace

Result<RouteView> AirlineService::create_route(const RouteCmd& cmd) {
    std::lock_guard<std::mutex> lock(mu_);

    if (cmd.flight_no.empty()) return Result<RouteView>::failure(Status::INVALID_ARG);

    Route r;
    fill_route_from_cmd(&r, cmd);

    // 配置合法性（domain 纯函数）
    Status st = route_config_check(r);
    if (st != Status::OK) return Result<RouteView>::failure(st);

    st = repo_.add_route(r);
    if (st != Status::OK) return Result<RouteView>::failure(st);

    Date next = r.next_date(today_);
    RouteView v = build_view(r, next);
    v.next_date = date_to_string(next);
    return Result<RouteView>::success(std::move(v));
}

Result<RouteView> AirlineService::update_route(const std::string& flight_no,
                                               const RouteCmd& cmd) {
    std::lock_guard<std::mutex> lock(mu_);

    Route* route = require_route(flight_no, nullptr);
    if (!route) return Result<RouteView>::failure(Status::NOT_FOUND);

    // ── 校验段 ───────────────────────────────────────────
    SeatLayout new_layout;
    if (!seat_layout_for(cmd.aircraft.c_str(), &new_layout)) {
        return Result<RouteView>::failure(Status::INVALID_ARG);
    }

    // 换机型：该航线必须没有任何已售票（座位物理布局会变）
    if (std::strcmp(route->aircraft, cmd.aircraft.c_str()) != 0) {
        bool any_sold = false;
        repo_.for_each_instance([&](const FlightInstance& inst) {
            if (std::strcmp(inst.flight_no, flight_no.c_str()) != 0) return;
            for (int c = 0; c < CABIN_COUNT; ++c) {
                if (inst.sold[c] > 0) any_sold = true;
            }
        });
        if (any_sold) return Result<RouteView>::failure(Status::NOT_ELIGIBLE);
    }

    if (!weekday_mask_valid(cmd.weekdays)) {
        return Result<RouteView>::failure(Status::INVALID_ARG);
    }

    for (int c = 0; c < CABIN_COUNT; ++c) {
        int sold_max = repo_.max_sold(flight_no.c_str(), c);
        Status st = quota_change_check(cmd.cabins[c].quota, sold_max,
                                       new_layout.seats_of(c));
        if (st != Status::OK) return Result<RouteView>::failure(st);
        if (cmd.cabins[c].overbook_max < 0) {
            return Result<RouteView>::failure(Status::INVALID_ARG);
        }
        if (cmd.cabins[c].price <= 0) {
            return Result<RouteView>::failure(Status::INVALID_ARG);
        }
    }

    // ── 变更段 ───────────────────────────────────────────
    RouteCmd fixed = cmd;
    fixed.flight_no = flight_no;  // 航班号不可改
    fill_route_from_cmd(route, fixed);

    Status st = route_config_check(*route);
    if (st != Status::OK) return Result<RouteView>::failure(st);

    Date next = route->next_date(today_);
    RouteView v = build_view(*route, next);
    v.next_date = date_to_string(next);
    return Result<RouteView>::success(std::move(v));
}

Result<std::vector<RecordRow>> AirlineService::records(const std::string& flight_no,
                                                       const std::string& date,
                                                       const std::string& id_card) {
    std::lock_guard<std::mutex> lock(mu_);

    Date d;
    bool has_date = date.empty() ? false : date_from_string(date, &d);
    if (!date.empty() && !has_date) {
        return Result<std::vector<RecordRow>>::failure(Status::INVALID_ARG);
    }

    std::vector<RecordRow> out;
    repo_.for_each_instance([&](const FlightInstance& inst) {
        if (!flight_no.empty() &&
            std::strcmp(inst.flight_no, flight_no.c_str()) != 0) return;
        if (has_date && !date_equal(inst.date, d)) return;

        for (int i = 0; i < inst.orders.size(); ++i) {
            const Order& o = inst.orders.at(i);
            if (!id_card.empty() && id_card != o.id_card) continue;

            RecordRow row;
            row.flight_no = inst.flight_no;
            row.date = date_to_string(inst.date);
            row.order_id = o.order_id;
            row.name = o.name;
            row.id_card = o.id_card;
            row.cabin = o.cabin;
            row.qty = o.qty;
            row.price = o.price_at_booking;
            row.seat_pending = o.seat_pending;
            row.book_seq = o.book_seq;
            row.book_time = o.book_time;
            row.status = o.status;
            for (int k = 0; k < o.seat_count; ++k) {
                row.seats.push_back(std::string(o.seats[k]));
            }
            out.push_back(row);
        }
    });
    return Result<std::vector<RecordRow>>::success(std::move(out));
}

Result<CheckinOut> AirlineService::checkin(const std::string& flight_no,
                                           const std::string& date) {
    std::lock_guard<std::mutex> lock(mu_);

    Route* route = require_route(flight_no, nullptr);
    if (!route) return Result<CheckinOut>::failure(Status::NOT_FOUND);

    Date d;
    if (date.empty()) {
        d = route->next_date(today_);
    } else if (!date_from_string(date, &d)) {
        return Result<CheckinOut>::failure(Status::INVALID_ARG);
    }

    FlightInstance* inst = repo_.find_instance(route->flight_no, d);
    if (!inst) return Result<CheckinOut>::failure(Status::NOT_FOUND);

    if (inst->phase == FLIGHT_LOCKED) {
        return Result<CheckinOut>::failure(Status::FLIGHT_LOCKED);
    }

    // ── 生成调整方案（domain 纯函数，不改状态） ──────────
    CheckinPlan plan;
    Status st = plan_checkin(*route, *inst, &plan);
    if (st != Status::OK) return Result<CheckinOut>::failure(st);

    CheckinOut out;
    out.flight_no = route->flight_no;
    out.date = date_to_string(d);
    for (int c = 0; c < CABIN_COUNT; ++c) {
        out.physical[c] = plan.physical[c];
        out.boarded[c] = plan.boarded[c];
    }

    // ── 变更段 1：执行升舱 ───────────────────────────────
    for (int i = 0; i < plan.upgrades.size(); ++i) {
        const UpgradeMove& mv = plan.upgrades.at(i);
        Order& o = inst->orders.at(mv.order_index);
        int from = o.cabin;

        UpgradeRow row;
        row.order_id = o.order_id;
        row.name = o.name;
        row.from_cabin = from;
        row.to_cabin = mv.to_cabin;
        row.qty = o.qty;

        o.cabin = mv.to_cabin;
        o.seat_count = mv.seat_count;
        o.seat_pending = false;
        o.status = ORDER_UPGRADED;
        for (int k = 0; k < mv.seat_count; ++k) {
            std::strncpy(o.seats[k], mv.seats[k], kSeatCodeLen);
            row.seats.push_back(std::string(mv.seats[k]));
            int r = 0;
            char letter = 0;
            seat_code_parse(mv.seats[k], &r, &letter);
            inst->set_occupied(mv.to_cabin, r, letter, true);
        }

        inst->sold[from] -= o.qty;
        inst->sold[mv.to_cabin] += o.qty;
        out.upgrades.push_back(row);
    }

    // ── 变更段 2：执行强制退票（最后订票者先退） ──────────
    for (int i = 0; i < plan.forfeits.size(); ++i) {
        Order& o = inst->orders.at(plan.forfeits.at(i));
        o.status = ORDER_FORCE_REFUNDED;
        inst->sold[o.cabin] -= o.qty;

        ForfeitRow row;
        row.order_id = o.order_id;
        row.name = o.name;
        row.cabin = o.cabin;
        row.qty = o.qty;
        row.book_seq = o.book_seq;
        out.forfeits.push_back(row);
    }

    // ── 锁定航班 ─────────────────────────────────────────
    inst->phase = FLIGHT_LOCKED;
    return Result<CheckinOut>::success(std::move(out));
}

}  // namespace airline
