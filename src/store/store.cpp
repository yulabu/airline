#include "store/store.h"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

#include "base/date.h"

namespace airline {
namespace store {
namespace {

using json = nlohmann::json;

json cabin_to_json(const CabinConfig& c) {
    return json{{"quota", c.quota}, {"overbook_max", c.overbook_max}, {"price", c.price}};
}

void cabin_from_json(const json& j, CabinConfig* c) {
    c->quota = j.value("quota", 0);
    c->overbook_max = j.value("overbook_max", 0);
    c->price = j.value("price", 0);
}

json route_to_json(const Route& r) {
    json cabins = json::array();
    for (int c = 0; c < CABIN_COUNT; ++c) cabins.push_back(cabin_to_json(r.cabins[c]));
    return json{{"flight_no", r.flight_no},
                {"from", r.from},
                {"to", r.to},
                {"aircraft", r.aircraft},
                {"weekdays", r.weekdays},
                {"cabins", cabins}};
}

Route route_from_json(const json& j) {
    Route r;
    std::snprintf(r.flight_no, kFlightNoLen, "%s", j.value("flight_no", "").c_str());
    std::snprintf(r.from, kCityLen, "%s", j.value("from", "").c_str());
    std::snprintf(r.to, kCityLen, "%s", j.value("to", "").c_str());
    std::snprintf(r.aircraft, kAircraftLen, "%s", j.value("aircraft", "").c_str());
    r.weekdays = j.value("weekdays", 0);
    const json& cabins = j.at("cabins");
    for (int c = 0; c < CABIN_COUNT && c < static_cast<int>(cabins.size()); ++c) {
        cabin_from_json(cabins.at(c), &r.cabins[c]);
    }
    return r;
}

json order_to_json(const Order& o) {
    json seats = json::array();
    for (int k = 0; k < o.seat_count; ++k) seats.push_back(std::string(o.seats[k]));
    return json{{"order_id", o.order_id},
                {"name", o.name},
                {"id_card", o.id_card},
                {"cabin", o.cabin},
                {"qty", o.qty},
                {"refunded", o.refunded},
                {"seats", seats},
                {"seat_pending", o.seat_pending},
                {"price_at_booking", o.price_at_booking},
                {"book_seq", o.book_seq},
                {"book_time", o.book_time},
                {"status", o.status}};
}

Order order_from_json(const json& j) {
    Order o;
    std::snprintf(o.order_id, kOrderIdLen, "%s", j.value("order_id", "").c_str());
    std::snprintf(o.name, kNameLen, "%s", j.value("name", "").c_str());
    std::snprintf(o.id_card, kIdCardLen, "%s", j.value("id_card", "").c_str());
    o.cabin = j.value("cabin", 0);
    o.qty = j.value("qty", 0);
    o.refunded = j.value("refunded", 0);
    o.seat_pending = j.value("seat_pending", false);
    o.price_at_booking = j.value("price_at_booking", 0);
    o.book_seq = j.value("book_seq", 0L);
    std::snprintf(o.book_time, kTimeLen, "%s", j.value("book_time", "").c_str());
    o.status = j.value("status", 0);
    for (const auto& s : j.value("seats", json::array())) {
        if (o.seat_count >= kMaxSeatsPerOrder) break;
        std::snprintf(o.seats[o.seat_count], kSeatCodeLen, "%s",
                      std::string(s).c_str());
        ++o.seat_count;
    }
    return o;
}

json wait_to_json(const WaitEntry& e) {
    return json{{"name", e.name},
                {"id_card", e.id_card},
                {"cabin", e.cabin},
                {"qty", e.qty},
                {"join_seq", e.join_seq},
                {"join_time", e.join_time}};
}

WaitEntry wait_from_json(const json& j) {
    WaitEntry e;
    std::snprintf(e.name, kNameLen, "%s", j.value("name", "").c_str());
    std::snprintf(e.id_card, kIdCardLen, "%s", j.value("id_card", "").c_str());
    e.cabin = j.value("cabin", 0);
    e.qty = j.value("qty", 0);
    e.join_seq = j.value("join_seq", 0L);
    std::snprintf(e.join_time, kTimeLen, "%s", j.value("join_time", "").c_str());
    return e;
}

json instance_to_json(const FlightInstance& inst) {
    json orders = json::array();
    for (int i = 0; i < inst.orders.size(); ++i) {
        orders.push_back(order_to_json(inst.orders.at(i)));
    }
    json waits = json::array();
    for (int c = 0; c < CABIN_COUNT; ++c) {
        inst.wait_lists[c].for_each([&](const WaitEntry& e) {
            waits.push_back(wait_to_json(e));
        });
    }
    json sold = json::array();
    for (int c = 0; c < CABIN_COUNT; ++c) sold.push_back(inst.sold[c]);
    return json{{"flight_no", inst.flight_no},
                {"date", date_to_string(inst.date)},
                {"sold", sold},
                {"phase", inst.phase},
                {"orders", orders},
                {"waits", waits}};
}

// 座位占用位图不落盘：装载时从各订单的座位号重建（不变量：占用位 = 持票订单的座位并集）
void rebuild_occupied(FlightInstance* inst) {
    for (int i = 0; i < inst->orders.size(); ++i) {
        const Order& o = inst->orders.at(i);
        if (!o.holds_tickets()) continue;
        for (int k = 0; k < o.seat_count; ++k) {
            int row = 0;
            char letter = 0;
            if (seat_code_parse(o.seats[k], &row, &letter)) {
                inst->set_occupied(o.cabin, row, letter, true);
            }
        }
    }
}

}  // namespace

bool load(Repository& repo, const std::string& path) {
    std::ifstream in(path);
    if (!in.good()) return false;

    json root;
    try {
        in >> root;
    } catch (...) {
        return false;
    }
    if (!root.is_object()) return false;

    repo.clear();

    for (const auto& jr : root.value("routes", json::array())) {
        repo.add_route(route_from_json(jr));
    }

    for (const auto& ji : root.value("instances", json::array())) {
        Date d;
        if (!date_from_string(ji.value("date", ""), &d)) continue;
        FlightInstance* inst =
            repo.get_or_create_instance(ji.value("flight_no", "").c_str(), d);
        if (!inst) continue;

        const json& sold = ji.value("sold", json::array());
        for (int c = 0; c < CABIN_COUNT && c < static_cast<int>(sold.size()); ++c) {
            inst->sold[c] = sold.at(c).get<int>();
        }
        inst->phase = ji.value("phase", 0);

        for (const auto& jo : ji.value("orders", json::array())) {
            inst->orders.push_back(order_from_json(jo));
        }
        for (const auto& jw : ji.value("waits", json::array())) {
            WaitEntry e = wait_from_json(jw);
            if (e.cabin >= 0 && e.cabin < CABIN_COUNT) {
                inst->wait_lists[e.cabin].enqueue(e);
            }
        }
        rebuild_occupied(inst);
    }

    repo.set_order_seq(root.value("order_seq", 0L));
    repo.set_wait_seq(root.value("wait_seq", 0L));
    return true;
}

bool flush(Repository& repo, const std::string& path) {
    json root;
    root["order_seq"] = repo.order_seq();
    root["wait_seq"] = repo.wait_seq();

    json routes = json::array();
    for (int i = 0; i < repo.routes().size(); ++i) {
        routes.push_back(route_to_json(repo.routes().at(i)));
    }
    root["routes"] = routes;

    json instances = json::array();
    repo.for_each_instance([&](const FlightInstance& inst) {
        instances.push_back(instance_to_json(inst));
    });
    root["instances"] = instances;

    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out.good()) return false;
        out << root.dump(2) << std::endl;
        if (!out.good()) return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

}  // namespace store
}  // namespace airline
