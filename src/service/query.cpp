#include "service/airline_service.h"

#include <cstring>

#include "domain/policies.h"

namespace airline {

AirlineService::AirlineService(Date today) : today_(today) {}

Route* AirlineService::require_route(const char* flight_no, Status* err) {
    if (!flight_no || flight_no[0] == '\0') {
        if (err) *err = Status::INVALID_ARG;
        return nullptr;
    }
    Route* r = repo_.find_route(flight_no);
    if (!r) {
        if (err) *err = Status::NOT_FOUND;
        return nullptr;
    }
    if (err) *err = Status::OK;
    return r;
}

RouteView AirlineService::build_view(const Route& route, const Date& on_date) const {
    RouteView v;
    v.flight_no = route.flight_no;
    v.from = route.from;
    v.to = route.to;
    v.aircraft = route.aircraft;
    v.weekdays = route.weekdays;

    char wbuf[64];
    route.weekday_text(wbuf, sizeof(wbuf));
    v.weekday_text = wbuf;

    const FlightInstance* inst = repo_.find_instance(route.flight_no, on_date);
    for (int c = 0; c < CABIN_COUNT; ++c) {
        CabinView& cv = v.cabins[c];
        cv.cabin = c;
        cv.quota = route.cabins[c].quota;
        cv.overbook_max = route.cabins[c].overbook_max;
        cv.price = route.cabins[c].price;
        cv.sold = inst ? inst->sold[c] : 0;
        cv.avail = inst ? avail_display(route, *inst, c) : route.cabins[c].quota;
    }
    return v;
}

Result<std::vector<RouteView>> AirlineService::search_routes(const std::string& from,
                                                             const std::string& to,
                                                             int weekday) {
    std::lock_guard<std::mutex> lock(mu_);

    if (from.empty() && to.empty()) return Result<std::vector<RouteView>>::failure(Status::INVALID_ARG);
    if (weekday != 0 && (weekday < 1 || weekday > 7)) {
        return Result<std::vector<RouteView>>::failure(Status::INVALID_ARG);
    }

    std::vector<RouteView> out;
    for (int i = 0; i < repo_.routes().size(); ++i) {
        const Route& r = repo_.routes().at(i);
        if (!from.empty() && std::strcmp(r.from, from.c_str()) != 0) continue;
        if (!to.empty() && std::strcmp(r.to, to.c_str()) != 0) continue;
        if (weekday != 0 && !r.flies_on(weekday)) continue;

        Date next = r.next_date(today_);
        RouteView v = build_view(r, next);
        v.next_date = date_to_string(next);
        out.push_back(v);
    }
    return Result<std::vector<RouteView>>::success(std::move(out));
}

Result<std::vector<RouteView>> AirlineService::flight_instances(const std::string& flight_no,
                                                                int n) {
    std::lock_guard<std::mutex> lock(mu_);

    Status st = Status::OK;
    Route* route = require_route(flight_no, &st);
    if (!route) return Result<std::vector<RouteView>>::failure(st);

    if (n < 1 || n > 31) n = 7;

    std::vector<RouteView> out;
    Date cursor = today_;
    for (int k = 0; k < n; ++k) {
        Date d = route->next_date(cursor);
        RouteView v = build_view(*route, d);
        v.next_date = date_to_string(d);
        out.push_back(v);
        cursor = date_add_days(d, 1);
        if (date_diff_days(cursor, today_) > 60) break;  // 防御：不无限前推
    }
    return Result<std::vector<RouteView>>::success(std::move(out));
}

Result<SeatMapView> AirlineService::seat_map(const std::string& flight_no,
                                             const std::string& date, int cabin) {
    std::lock_guard<std::mutex> lock(mu_);

    if (cabin < 0 || cabin >= CABIN_COUNT) {
        return Result<SeatMapView>::failure(Status::INVALID_ARG);
    }

    Status st = Status::OK;
    Route* route = require_route(flight_no, &st);
    if (!route) return Result<SeatMapView>::failure(st);

    Date d;
    if (date.empty()) {
        d = route->next_date(today_);
    } else if (!date_from_string(date, &d)) {
        return Result<SeatMapView>::failure(Status::INVALID_ARG);
    }

    SeatLayout layout;
    if (!seat_layout_for(route->aircraft, &layout)) {
        return Result<SeatMapView>::failure(Status::INTERNAL);
    }

    // 未创建过实例 = 无人订票，全空闲；查询不产生状态
    const FlightInstance* inst = repo_.find_instance(route->flight_no, d);

    SeatMapView v;
    v.flight_no = route->flight_no;
    v.date = date_to_string(d);
    v.cabin = cabin;
    v.aircraft = route->aircraft;

    const CabinLayout& cl = layout.cabin[cabin];
    v.row_start = cl.row_start;
    v.row_end = cl.row_end;
    for (int i = 0; i < cl.seats_per_row; ++i) {
        v.letters.push_back(std::string(1, cl.letters[i]));
    }

    for (int r = cl.row_start; r <= cl.row_end; ++r) {
        for (int i = 0; i < cl.seats_per_row; ++i) {
            SeatCell cell;
            cell.row = r;
            cell.letter = cl.letters[i];
            cell.occupied = inst && !inst->is_free(cabin, r, cl.letters[i]);
            v.cells.push_back(cell);
        }
    }
    return Result<SeatMapView>::success(std::move(v));
}

Result<std::vector<RouteView>> AirlineService::recommend(const std::string& to,
                                                         const std::string& date,
                                                         const std::string& exclude_flight_no) {
    std::lock_guard<std::mutex> lock(mu_);

    if (to.empty()) return Result<std::vector<RouteView>>::failure(Status::INVALID_ARG);

    Date target;
    bool has_target = date_from_string(date, &target);
    if (!has_target) target = today_;

    std::vector<RouteView> out;
    for (int i = 0; i < repo_.routes().size(); ++i) {
        const Route& r = repo_.routes().at(i);
        if (std::strcmp(r.to, to.c_str()) != 0) continue;
        if (exclude_flight_no == r.flight_no) continue;

        // 目标日期起 3 天内该航线是否执飞（不推荐已过去的班次）
        Date best;
        bool found = false;
        for (int delta = 0; delta <= 3 && !found; ++delta) {
            Date d = date_add_days(target, delta);
            if (d.y < 1900 || !r.flies_on(date_weekday(d))) continue;
            const FlightInstance* inst = repo_.find_instance(r.flight_no, d);

            bool any_avail = false;
            for (int c = 0; c < CABIN_COUNT && !any_avail; ++c) {
                int left = inst ? avail_display(r, *inst, c) : r.cabins[c].quota;
                if (left > 0) any_avail = true;
            }
            if (any_avail) {
                best = d;
                found = true;
            }
        }
        if (!found) continue;

        RouteView v = build_view(r, best);
        v.next_date = date_to_string(best);
        out.push_back(v);
    }
    return Result<std::vector<RouteView>>::success(std::move(out));
}

}  // namespace airline
