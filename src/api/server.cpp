// HTTP 适配层：路由注册 + JSON 编解码 + 参数格式校验。
// 红线：这里不允许出现任何业务判断（不判断余票、不判断能否订票）。
#include "api/server.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "base/status.h"
#include "store/store.h"

namespace airline {
namespace api {
namespace {

using json = nlohmann::json;
using Http = httplib::Server;

// ── Status → HTTP 状态码映射（纯转换，不含业务） ──────
int http_code(Status s) {
    switch (s) {
        case Status::OK:               return 200;
        case Status::INVALID_ARG:      return 400;
        case Status::NOT_FOUND:        return 404;
        case Status::ORDER_NOT_FOUND:  return 404;
        case Status::WAITLIST_EMPTY:   return 404;
        case Status::FLIGHT_EXISTS:    return 409;
        case Status::QUOTA_BELOW_SOLD: return 409;
        case Status::NO_SEAT:          return 409;
        case Status::SEAT_TAKEN:       return 409;
        case Status::DUPLICATE_ORDER:  return 409;
        case Status::CABIN_MISMATCH:   return 409;
        case Status::FLIGHT_LOCKED:    return 423;
        case Status::NOT_ELIGIBLE:     return 409;
        case Status::INTERNAL:         return 500;
    }
    return 500;
}

json envelope_ok(json data) { return json{{"ok", true}, {"data", std::move(data)}}; }

json envelope_err(Status s) {
    return json{{"ok", false},
                {"code", status_code(s)},
                {"msg", status_message(s)}};
}

void reply(httplib::Response& res, Status s, json data) {
    res.status = http_code(s);
    res.set_content((s == Status::OK ? envelope_ok(std::move(data))
                                     : envelope_err(s))
                        .dump(),
                    "application/json");
}

void reply_result(httplib::Response& res, Status s) { reply(res, s, json::object()); }

// ── 参数格式校验（只查"长什么样"，不查"该不该"） ──────

bool get_str(const json& body, const char* key, std::string* out) {
    if (!body.is_object()) return false;
    auto it = body.find(key);
    if (it == body.end() || !it->is_string()) return false;
    *out = it->get<std::string>();
    return true;
}

bool get_int(const json& body, const char* key, int* out) {
    if (!body.is_object()) return false;
    auto it = body.find(key);
    if (it == body.end() || !it->is_number_integer()) return false;
    *out = it->get<int>();
    return true;
}

bool valid_idcard(const std::string& s) {
    if (s.size() != 18) return false;
    for (size_t i = 0; i < 17; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    char last = s[17];
    return (last >= '0' && last <= '9') || last == 'X' || last == 'x';
}

bool valid_flight_no(const std::string& s) {
    if (s.empty() || s.size() >= kFlightNoLen) return false;
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

json cabins_view(const RouteView& v) {
    json arr = json::array();
    for (int c = 0; c < CABIN_COUNT; ++c) {
        arr.push_back({{"cabin", v.cabins[c].cabin},
                       {"name", cabin_name(c)},
                       {"quota", v.cabins[c].quota},
                       {"sold", v.cabins[c].sold},
                       {"avail", v.cabins[c].avail},
                       {"overbook_max", v.cabins[c].overbook_max},
                       {"price", v.cabins[c].price}});
    }
    return arr;
}

json route_view(const RouteView& v) {
    return {{"flight_no", v.flight_no},
            {"from", v.from},
            {"to", v.to},
            {"aircraft", v.aircraft},
            {"weekdays", v.weekdays},
            {"weekday_text", v.weekday_text},
            {"next_date", v.next_date},
            {"cabins", cabins_view(v)}};
}

json route_list(const std::vector<RouteView>& list) {
    json arr = json::array();
    for (const auto& v : list) arr.push_back(route_view(v));
    return arr;
}

BookCmd parse_book_cmd(const json& body, bool* ok) {
    BookCmd cmd;
    *ok = get_str(body, "flight_no", &cmd.flight_no) &&
          valid_flight_no(cmd.flight_no) && get_str(body, "name", &cmd.name) &&
          get_str(body, "id_card", &cmd.id_card) && valid_idcard(cmd.id_card) &&
          get_int(body, "cabin", &cmd.cabin) && get_int(body, "qty", &cmd.qty);
    if (*ok) {
        get_str(body, "date", &cmd.date);   // 可空
        get_str(body, "seat", &cmd.seat);   // 可空
        if (!cmd.date.empty() && cmd.date.size() != 10) *ok = false;
    }
    return cmd;
}

void register_passenger(Http& srv, AirlineService& svc) {
    // ① 查询航线：起点或终点（至少一个）
    srv.Get("/api/search", [&](const httplib::Request& req, httplib::Response& res) {
        std::string from = req.get_param_value("from");
        std::string to = req.get_param_value("to");
        int weekday = 0;
        if (req.has_param("weekday")) weekday = std::atoi(req.get_param_value("weekday").c_str());

        auto r = svc.search_routes(from, to, weekday);
        reply(res, r.status, r.ok() ? route_list(r.value) : json::object());
    });

    // 某航班未来 n 个班次
    srv.Get(R"(/api/flights/([0-9A-Z]+)/instances)",
            [&](const httplib::Request& req, httplib::Response& res) {
                std::string no = req.matches[1];
                int n = 7;
                if (req.has_param("n")) n = std::atoi(req.get_param_value("n").c_str());

                auto r = svc.flight_instances(no, n);
                reply(res, r.status, r.ok() ? route_list(r.value) : json::object());
            });

    // ② 订票
    srv.Post("/api/orders", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            return reply_result(res, Status::INVALID_ARG);
        }

        bool ok = false;
        BookCmd cmd = parse_book_cmd(body, &ok);
        if (!ok) return reply_result(res, Status::INVALID_ARG);

        auto r = svc.book(cmd);
        if (!r.ok()) {
            // NO_SEAT 时携带 remaining / waitlist_size 上下文
            json data;
            data["remaining"] = r.value.remaining;
            data["waitlist_size"] = r.value.waitlist_size;
            data["flight_no"] = r.value.flight_no;
            data["date"] = r.value.date;
            data["cabin"] = r.value.cabin;
            return reply(res, r.status, std::move(data));
        }

        json data = {{"order_id", r.value.order_id},
                     {"flight_no", r.value.flight_no},
                     {"date", r.value.date},
                     {"cabin", r.value.cabin},
                     {"qty", r.value.qty},
                     {"seat_pending", r.value.seat_pending},
                     {"seats", r.value.seats}};
        store::flush(svc.repo(), "state.json");
        reply(res, Status::OK, std::move(data));
    });

    // ③ 退票
    srv.Post("/api/refund", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            return reply_result(res, Status::INVALID_ARG);
        }

        RefundCmd cmd;
        bool ok = get_str(body, "flight_no", &cmd.flight_no) &&
                  valid_flight_no(cmd.flight_no) &&
                  get_str(body, "id_card", &cmd.id_card) && valid_idcard(cmd.id_card);
        if (ok) get_str(body, "date", &cmd.date);
        if (ok && body.contains("qty") && body["qty"].is_number_integer()) {
            cmd.qty = body["qty"].get<int>();
        }
        if (!ok) return reply_result(res, Status::INVALID_ARG);

        auto r = svc.refund(cmd);
        if (!r.ok()) return reply_result(res, r.status);

        json promoted = json::array();
        for (const auto& p : r.value.promoted) {
            promoted.push_back({{"name", p.name},
                                {"id_card", p.id_card},
                                {"qty", p.qty},
                                {"cabin", p.cabin},
                                {"cabin_name", cabin_name(p.cabin)},
                                {"seats", p.seats},
                                {"seat_pending", p.seat_pending}});
        }
        json data = {{"order_id", r.value.order_id},
                     {"refunded", r.value.refunded},
                     {"promoted", promoted}};
        store::flush(svc.repo(), "state.json");
        reply(res, Status::OK, std::move(data));
    });

    // 登记候补（订票失败后的二次确认）
    srv.Post("/api/waitlist", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            return reply_result(res, Status::INVALID_ARG);
        }

        WaitCmd cmd;
        bool ok = get_str(body, "flight_no", &cmd.flight_no) &&
                  valid_flight_no(cmd.flight_no) && get_str(body, "name", &cmd.name) &&
                  get_str(body, "id_card", &cmd.id_card) && valid_idcard(cmd.id_card) &&
                  get_int(body, "cabin", &cmd.cabin) && get_int(body, "qty", &cmd.qty);
        if (ok) get_str(body, "date", &cmd.date);
        if (!ok) return reply_result(res, Status::INVALID_ARG);

        auto r = svc.join_waitlist(cmd);
        if (!r.ok()) return reply_result(res, r.status);

        json data = {{"flight_no", r.value.flight_no},
                     {"date", r.value.date},
                     {"cabin", r.value.cabin},
                     {"qty", r.value.qty},
                     {"position", r.value.position}};
        store::flush(svc.repo(), "state.json");
        reply(res, Status::OK, std::move(data));
    });

    // 扩展 1：推荐同目的地其他航线
    srv.Get("/api/recommend", [&](const httplib::Request& req, httplib::Response& res) {
        std::string to = req.get_param_value("to");
        std::string date = req.get_param_value("date");
        std::string exclude = req.get_param_value("exclude");

        auto r = svc.recommend(to, date, exclude);
        reply(res, r.status, r.ok() ? route_list(r.value) : json::object());
    });

    // 扩展 3：座位图
    srv.Get("/api/seats", [&](const httplib::Request& req, httplib::Response& res) {
        std::string no = req.get_param_value("flight_no");
        std::string date = req.get_param_value("date");
        int cabin = std::atoi(req.get_param_value("cabin").c_str());

        auto r = svc.seat_map(no, date, cabin);
        if (!r.ok()) return reply_result(res, r.status);

        json cells = json::array();
        for (const auto& c : r.value.cells) {
            cells.push_back({{"row", c.row},
                             {"letter", std::string(1, c.letter)},
                             {"code", std::to_string(c.row) + std::string(1, c.letter)},
                             {"occupied", c.occupied}});
        }
        json data = {{"flight_no", r.value.flight_no},
                     {"date", r.value.date},
                     {"cabin", r.value.cabin},
                     {"cabin_name", cabin_name(r.value.cabin)},
                     {"aircraft", r.value.aircraft},
                     {"row_start", r.value.row_start},
                     {"row_end", r.value.row_end},
                     {"letters", r.value.letters},
                     {"cells", cells}};
        reply(res, Status::OK, std::move(data));
    });
}

void register_admin(Http& srv, AirlineService& svc) {
    // 管理员 ① 新建航线
    srv.Post("/api/admin/routes", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            return reply_result(res, Status::INVALID_ARG);
        }

        RouteCmd cmd;
        bool ok = get_str(body, "flight_no", &cmd.flight_no) &&
                  valid_flight_no(cmd.flight_no) && get_str(body, "from", &cmd.from) &&
                  get_str(body, "to", &cmd.to) &&
                  get_str(body, "aircraft", &cmd.aircraft) &&
                  get_int(body, "weekdays", &cmd.weekdays);
        if (ok && body.contains("cabins") && body["cabins"].is_array() &&
            body["cabins"].size() == CABIN_COUNT) {
            for (int c = 0; c < CABIN_COUNT; ++c) {
                const json& jc = body["cabins"].at(c);
                if (!jc.is_object() || !jc.contains("quota") || !jc.contains("price")) {
                    ok = false;
                    break;
                }
                cmd.cabins[c].quota = jc["quota"].get<int>();
                cmd.cabins[c].price = jc["price"].get<int>();
                cmd.cabins[c].overbook_max =
                    jc.value("overbook_max", 0);
            }
        } else {
            ok = false;
        }
        if (!ok) return reply_result(res, Status::INVALID_ARG);

        auto r = svc.create_route(cmd);
        if (!r.ok()) return reply_result(res, r.status);
        store::flush(svc.repo(), "state.json");
        reply(res, Status::OK, route_view(r.value));
    });

    // 管理员 ② 修改航线
    srv.Put(R"(/api/admin/routes/([0-9A-Z]+))",
            [&](const httplib::Request& req, httplib::Response& res) {
                json body;
                try {
                    body = json::parse(req.body);
                } catch (...) {
                    return reply_result(res, Status::INVALID_ARG);
                }

                RouteCmd cmd;
                bool ok = get_str(body, "from", &cmd.from) &&
                          get_str(body, "to", &cmd.to) &&
                          get_str(body, "aircraft", &cmd.aircraft) &&
                          get_int(body, "weekdays", &cmd.weekdays);
                if (ok && body.contains("cabins") && body["cabins"].is_array() &&
                    body["cabins"].size() == CABIN_COUNT) {
                    for (int c = 0; c < CABIN_COUNT; ++c) {
                        const json& jc = body["cabins"].at(c);
                        cmd.cabins[c].quota = jc.value("quota", 0);
                        cmd.cabins[c].price = jc.value("price", 0);
                        cmd.cabins[c].overbook_max = jc.value("overbook_max", 0);
                    }
                } else {
                    ok = false;
                }
                if (!ok) return reply_result(res, Status::INVALID_ARG);

                auto r = svc.update_route(req.matches[1], cmd);
                if (!r.ok()) return reply_result(res, r.status);
                store::flush(svc.repo(), "state.json");
                reply(res, Status::OK, route_view(r.value));
            });

    // 管理员 ③ 查看订票记录
    srv.Get("/api/admin/records", [&](const httplib::Request& req, httplib::Response& res) {
        std::string no = req.get_param_value("flight_no");
        std::string date = req.get_param_value("date");
        std::string idcard = req.get_param_value("id_card");

        auto r = svc.records(no, date, idcard);
        if (!r.ok()) return reply_result(res, r.status);

        json rows = json::array();
        for (const auto& row : r.value) {
            rows.push_back({{"flight_no", row.flight_no},
                            {"date", row.date},
                            {"order_id", row.order_id},
                            {"name", row.name},
                            {"id_card", row.id_card},
                            {"cabin", row.cabin},
                            {"cabin_name", cabin_name(row.cabin)},
                            {"qty", row.qty},
                            {"price", row.price},
                            {"seats", row.seats},
                            {"seat_pending", row.seat_pending},
                            {"book_seq", row.book_seq},
                            {"book_time", row.book_time},
                            {"status", row.status},
                            {"status_name", order_status_name(row.status)}});
        }
        reply(res, Status::OK, rows);
    });

    // 扩展 2：模拟登机（超卖调整）
    srv.Post("/api/admin/checkin", [&](const httplib::Request& req, httplib::Response& res) {
        json body;
        try {
            body = json::parse(req.body);
        } catch (...) {
            return reply_result(res, Status::INVALID_ARG);
        }

        std::string flight_no, date;
        if (!get_str(body, "flight_no", &flight_no) || !valid_flight_no(flight_no)) {
            return reply_result(res, Status::INVALID_ARG);
        }
        get_str(body, "date", &date);

        auto r = svc.checkin(flight_no, date);
        if (!r.ok()) return reply_result(res, r.status);

        json upgrades = json::array();
        for (const auto& u : r.value.upgrades) {
            upgrades.push_back({{"order_id", u.order_id},
                                {"name", u.name},
                                {"from_cabin", u.from_cabin},
                                {"from_name", cabin_name(u.from_cabin)},
                                {"to_cabin", u.to_cabin},
                                {"to_name", cabin_name(u.to_cabin)},
                                {"qty", u.qty},
                                {"seats", u.seats}});
        }
        json forfeits = json::array();
        for (const auto& f : r.value.forfeits) {
            forfeits.push_back({{"order_id", f.order_id},
                                {"name", f.name},
                                {"cabin", f.cabin},
                                {"cabin_name", cabin_name(f.cabin)},
                                {"qty", f.qty},
                                {"book_seq", f.book_seq}});
        }
        json boarded = json::array();
        for (int c = 0; c < CABIN_COUNT; ++c) {
            boarded.push_back({{"cabin", c},
                               {"name", cabin_name(c)},
                               {"boarded", r.value.boarded[c]},
                               {"physical", r.value.physical[c]}});
        }
        json data = {{"flight_no", r.value.flight_no},
                     {"date", r.value.date},
                     {"upgrades", upgrades},
                     {"forfeits", forfeits},
                     {"boarded", boarded}};
        store::flush(svc.repo(), "state.json");
        reply(res, Status::OK, std::move(data));
    });
}

}  // namespace

int serve(AirlineService& svc, const std::string& web_dir,
          const std::string& state_path, int port) {
    Http srv;

    srv.set_mount_point("/", web_dir.c_str());

    srv.Get("/api/meta", [&](const httplib::Request& req, httplib::Response& res) {
        json aircraft = json::array();
        for (int i = 0; i < seat_layout_count(); ++i) {
            aircraft.push_back(seat_layout_aircraft(i));
        }
        json cabins = json::array();
        for (int c = 0; c < CABIN_COUNT; ++c) {
            cabins.push_back({{"cabin", c}, {"name", cabin_name(c)}});
        }
        reply(res, Status::OK,
              {{"today", date_to_string(svc.today())},
               {"aircraft", aircraft},
               {"cabins", cabins},
               {"max_seats_per_order", kMaxSeatsPerOrder}});
    });

    register_passenger(srv, svc);
    register_admin(srv, svc);

    (void)state_path;  // 落盘路径统一为工作目录下的 state.json

    srv.listen("0.0.0.0", port);
    return 0;
}

}  // namespace api
}  // namespace airline
