// service 层集成测试：订票 → 退票 → 候补 → 登机 全链路。
// 只链 airline_core，不启动 HTTP。
#include <cstdio>
#include <cstring>
#include <string>

#include "base/date.h"
#include "service/airline_service.h"

using namespace airline;

extern int g_checks;
extern int g_failures;

#define TCHECK(cond)                                                       \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            std::printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

#define TCHECK_EQ(a, b)                                                         \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!((a) == (b))) {                                                    \
            std::printf("  [FAIL] %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b); \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

namespace {

BookCmd mk_book(const char* id, int cabin, int qty, const char* seat = "") {
    BookCmd c;
    c.flight_no = "CZ3101";
    c.name = "测试乘客";
    c.id_card = id;
    c.cabin = cabin;
    c.qty = qty;
    c.seat = seat;
    return c;
}

const char* kId1 = "44010119900101001A";
const char* kId2 = "44010119900101002B";
const char* kId3 = "44010119900101003C";
const char* kId4 = "44010119900101004D";

}  // namespace

void test_service_flow() {
    std::printf("[service: book / refund / waitlist]\n");

    AirlineService svc(Date{2026, 11, 9});  // 周一
    svc.seed_standard();
    TCHECK(svc.repo().route_count() >= 15);

    // ── 查询航线：按起点 ─────────────────────────────
    auto q = svc.search_routes("广州", "", 0);
    TCHECK(q.ok());
    TCHECK(!q.value.empty());
    bool has_cz3101 = false;
    for (const auto& v : q.value) {
        if (v.flight_no == "CZ3101") has_cz3101 = true;
    }
    TCHECK(has_cz3101);
    TCHECK_EQ(q.value[0].next_date, std::string("2026-11-09"));  // 周一执飞

    // 查询不产生实例
    TCHECK_EQ(svc.repo().instance_count(), 0);

    // 按终点查
    auto q2 = svc.search_routes("", "北京", 0);
    TCHECK(q2.ok());
    TCHECK(q2.value.size() >= 2);  // CZ3101 + MU5203

    // ── 订票：自动分配座位 ───────────────────────────
    auto b1 = svc.book(mk_book(kId1, CABIN_ECONOMY, 2));
    TCHECK(b1.ok());
    TCHECK_EQ(b1.value.seats.size(), 2u);
    TCHECK_EQ(b1.value.seats[0], std::string("7A"));
    TCHECK_EQ(b1.value.seats[1], std::string("7B"));

    // 同人同航班重复订票
    auto dup = svc.book(mk_book(kId1, CABIN_ECONOMY, 1));
    TCHECK(!dup.ok());
    TCHECK(dup.status == Status::DUPLICATE_ORDER);

    // 指定座位
    auto b2 = svc.book(mk_book(kId2, CABIN_ECONOMY, 1, "9C"));
    TCHECK(b2.ok());
    TCHECK_EQ(b2.value.seats[0], std::string("9C"));

    // 指定已被占的座位
    auto taken = svc.book(mk_book(kId3, CABIN_ECONOMY, 1, "9C"));
    TCHECK(!taken.ok());
    TCHECK(taken.status == Status::SEAT_TAKEN);

    // ── 候补：把经济舱卖超 ───────────────────────────
    // CZ3101 经济舱定额 150 + 超卖 6 = 156 可售
    AirlineService svc2(Date{2026, 11, 9});
    svc2.seed_standard();

    // 买满 156 张（每人 6 张 × 26 人）
    for (int i = 0; i < 26; ++i) {
        BookCmd c;
        c.flight_no = "CZ3101";
        char name[kNameLen];
        char id[kIdCardLen];
        std::snprintf(name, sizeof(name), "乘客%02d", i);
        std::snprintf(id, sizeof(id), "440101199001%05dI", i);
        c.name = name;
        c.id_card = id;
        c.cabin = CABIN_ECONOMY;
        c.qty = 6;
        auto r = svc2.book(c);
        if (!r.ok()) {
            TCHECK(false);
            break;
        }
    }
    // 第 157 张（超卖额度也用完）
    auto full = svc2.book(mk_book(kId4, CABIN_ECONOMY, 1));
    TCHECK(!full.ok());
    TCHECK(full.status == Status::NO_SEAT);
    TCHECK_EQ(full.value.remaining, 0);

    // 登记候补：甲要 3 张（队首），乙要 2 张
    WaitCmd w1;
    w1.flight_no = "CZ3101";
    w1.name = "候补甲";
    w1.id_card = "44010119900101111E";
    w1.cabin = CABIN_ECONOMY;
    w1.qty = 3;
    auto wr1 = svc2.join_waitlist(w1);
    TCHECK(wr1.ok());
    TCHECK_EQ(wr1.value.position, 1);

    WaitCmd w2;
    w2.flight_no = "CZ3101";
    w2.name = "候补乙";
    w2.id_card = "44010119900101222F";
    w2.cabin = CABIN_ECONOMY;
    w2.qty = 2;
    auto wr2 = svc2.join_waitlist(w2);
    TCHECK(wr2.ok());
    TCHECK_EQ(wr2.value.position, 2);

    // ── 退票：退 2 张 → 队首要 3 张，跳过，乙（2 张）被撮合 ──
    RefundCmd rf;
    rf.flight_no = "CZ3101";
    rf.id_card = "44010119900100000I";  // 第 0 位乘客
    rf.qty = 2;
    auto rr = svc2.refund(rf);
    TCHECK(rr.ok());
    TCHECK_EQ(rr.value.refunded, 2);
    TCHECK_EQ(rr.value.promoted.size(), 1u);
    TCHECK_EQ(rr.value.promoted[0].name, std::string("候补乙"));
    TCHECK_EQ(rr.value.promoted[0].qty, 2);

    // ── 再退 4 张 → 甲（3 张）被撮合，剩 1 张无人要 ──
    RefundCmd rf2;
    rf2.flight_no = "CZ3101";
    rf2.id_card = "44010119900100001I";
    rf2.qty = 4;
    auto rr2 = svc2.refund(rf2);
    TCHECK(rr2.ok());
    TCHECK_EQ(rr2.value.refunded, 4);
    TCHECK_EQ(rr2.value.promoted.size(), 1u);
    TCHECK_EQ(rr2.value.promoted[0].name, std::string("候补甲"));

    // ── 部分退票：6 张退 1 张，订单仍在 ──────────────
    RefundCmd rf3;
    rf3.flight_no = "CZ3101";
    rf3.id_card = "44010119900100002I";
    rf3.qty = 1;
    auto rr3 = svc2.refund(rf3);
    TCHECK(rr3.ok());
    TCHECK_EQ(rr3.value.refunded, 1);
    auto rec = svc2.records("CZ3101", "", "44010119900100002I");
    TCHECK(rec.ok());
    TCHECK_EQ(rec.value.size(), 1u);
    TCHECK_EQ(rec.value[0].qty, 5);
    TCHECK(rec.value[0].status == ORDER_PARTIAL_REFUND);
}

void test_checkin_flow() {
    std::printf("[service: checkin / oversell]\n");

    AirlineService svc(Date{2026, 11, 9});
    svc.seed_standard();

    // 构造超卖场景：经济舱 156 座卖 157 张（每人 1 张，157 人）
    // CZ3101 经济舱定额 150、超卖上限 6 → 最多 156，无法 157。
    // 改用管理员提高超卖上限到 10，再卖 157 张。
    RouteCmd uc;
    uc.from = "广州";
    uc.to = "北京";
    uc.aircraft = "B737-800";
    uc.weekdays = 0b0101011;
    uc.cabins[CABIN_FIRST] = {8, 0, 2600};
    uc.cabins[CABIN_BUSINESS] = {16, 2, 1800};
    uc.cabins[CABIN_ECONOMY] = {150, 10, 1200};
    auto up = svc.update_route("CZ3101", uc);
    TCHECK(up.ok());

    for (int i = 0; i < 157; ++i) {
        BookCmd c;
        c.flight_no = "CZ3101";
        char name[kNameLen];
        char id[kIdCardLen];
        std::snprintf(name, sizeof(name), "旅%03d", i);
        std::snprintf(id, sizeof(id), "440101199001%05dJ", i);
        c.name = name;
        c.id_card = id;
        c.cabin = CABIN_ECONOMY;
        c.qty = 1;
        auto r = svc.book(c);
        if (!r.ok()) {
            TCHECK(false);
            break;
        }
    }

    // 商务舱空着 → 登机时超出物理座位的人升舱，0 人被强制退票
    // 经济舱 156 个实体座位卖 157 人（定额 150 + 超卖 10）→ 需升舱 1 人
    auto ck = svc.checkin("CZ3101", "2026-11-09");
    TCHECK(ck.ok());
    TCHECK_EQ(ck.value.upgrades.size(), 1u);
    TCHECK_EQ(ck.value.forfeits.size(), 0u);
    TCHECK_EQ(ck.value.boarded[CABIN_ECONOMY], 156);
    TCHECK_EQ(ck.value.boarded[CABIN_BUSINESS], 1);
    TCHECK_EQ(ck.value.physical[CABIN_ECONOMY], 156);
    TCHECK_EQ(ck.value.physical[CABIN_BUSINESS], 16);

    // 锁定后不能再订票
    auto after = svc.book(mk_book(kId1, CABIN_ECONOMY, 1));
    TCHECK(!after.ok());
    TCHECK(after.status == Status::FLIGHT_LOCKED);

    // 记录里有升舱订单
    auto rec = svc.records("CZ3101", "2026-11-09", "");
    TCHECK(rec.ok());
    int upgraded = 0;
    for (const auto& row : rec.value) {
        if (row.status == ORDER_UPGRADED) ++upgraded;
    }
    TCHECK_EQ(upgraded, 1);

    // 重复登机 → FLIGHT_LOCKED
    auto again = svc.checkin("CZ3101", "2026-11-09");
    TCHECK(!again.ok());
    TCHECK(again.status == Status::FLIGHT_LOCKED);
}

void test_admin_flow() {
    std::printf("[service: admin]\n");

    AirlineService svc(Date{2026, 11, 9});
    svc.seed_standard();

    // 三个舱位都必须填合法值（price > 0），否则被 domain 校验拒绝
    auto mk_cmd = [](const char* no) {
        RouteCmd c;
        c.flight_no = no;
        c.from = "广州";
        c.to = "南京";
        c.aircraft = "A320";
        c.weekdays = 0x0F;
        c.cabins[CABIN_FIRST] = {8, 0, 2600};
        c.cabins[CABIN_BUSINESS] = {12, 0, 1600};
        c.cabins[CABIN_ECONOMY] = {140, 0, 800};
        return c;
    };

    // 新建航线：航班号重复
    RouteCmd dup = mk_cmd("CZ3101");
    dup.to = "北京";
    auto r1 = svc.create_route(dup);
    TCHECK(!r1.ok());
    TCHECK(r1.status == Status::FLIGHT_EXISTS);

    // 新建航线：定额超物理座位
    RouteCmd over = mk_cmd("XX1234");
    over.cabins[CABIN_ECONOMY] = {200, 0, 800};  // A320 经济舱只有 150 座
    auto r2 = svc.create_route(over);
    TCHECK(!r2.ok());
    TCHECK(r2.status == Status::INVALID_ARG);

    // 正常新建
    over.cabins[CABIN_ECONOMY] = {140, 0, 800};
    auto r3 = svc.create_route(over);
    TCHECK(r3.ok());

    // 修改航线：先卖票，再把定额改到已售以下
    BookCmd b;
    b.flight_no = "XX1234";
    b.name = "买家";
    b.id_card = "44010119900101999Z";
    b.cabin = CABIN_ECONOMY;
    b.qty = 6;
    TCHECK(svc.book(b).ok());

    RouteCmd shrink = over;
    shrink.cabins[CABIN_ECONOMY] = {5, 0, 800};  // 已售 6 → 不能改到 5
    auto r4 = svc.update_route("XX1234", shrink);
    TCHECK(!r4.ok());
    TCHECK(r4.status == Status::QUOTA_BELOW_SOLD);

    // 换机型：有已售票时不允许
    RouteCmd re_ac = over;
    re_ac.aircraft = "B737-800";
    auto r5 = svc.update_route("XX1234", re_ac);
    TCHECK(!r5.ok());
    TCHECK(r5.status == Status::NOT_ELIGIBLE);

    // 定额保持合法的修改成功
    RouteCmd ok_cmd = over;
    ok_cmd.cabins[CABIN_ECONOMY] = {146, 0, 820};
    auto r6 = svc.update_route("XX1234", ok_cmd);
    TCHECK(r6.ok());
    TCHECK_EQ(r6.value.cabins[CABIN_ECONOMY].price, 820);

    // 记录检索：按航班号
    auto rec = svc.records("CZ3101", "", "");
    TCHECK(rec.ok());
    TCHECK(rec.value.empty());  // 新服务没卖过 CZ3101
}

void test_waitlist_guard() {
    std::printf("[service: waitlist guard]\n");

    AirlineService svc(Date{2026, 11, 9});
    svc.seed_standard();

    // ① 有余票时不允许登记候补
    WaitCmd w;
    w.flight_no = "CZ3101";
    w.name = "多余候补";
    w.id_card = "440101199001060001";
    w.cabin = CABIN_ECONOMY;
    w.qty = 1;
    auto r0 = svc.join_waitlist(w);
    TCHECK(!r0.ok());
    TCHECK(r0.status == Status::NOT_ELIGIBLE);

    // 制造超卖：经济舱 156 可售（150 定额 + 6 超卖），26 人 × 6 张订满
    for (int i = 0; i < 26; ++i) {
        BookCmd c;
        c.flight_no = "CZ3101";
        char name[kNameLen];
        char id[kIdCardLen];
        std::snprintf(name, sizeof(name), "满座%02d", i);
        std::snprintf(id, sizeof(id), "440101199001%05dI", i);
        c.name = name;
        c.id_card = id;
        c.cabin = CABIN_ECONOMY;
        c.qty = 6;
        TCHECK(svc.book(c).ok());
    }

    // ② 超卖后可登记；同一身份证不允许重复登记
    w.id_card = "440101199001061001";
    w.qty = 2;
    TCHECK(svc.join_waitlist(w).ok());
    auto r2 = svc.join_waitlist(w);
    TCHECK(!r2.ok());
    TCHECK(r2.status == Status::NOT_ELIGIBLE);

    // ③ 已持有效订单的人不允许再登记
    w.id_card = "44010119900100000I";
    auto r3 = svc.join_waitlist(w);
    TCHECK(!r3.ok());
    TCHECK(r3.status == Status::DUPLICATE_ORDER);

    // ④ 失效登记清理：候补者后来通过正常渠道订到了票，
    //    其残留的候补登记必须在下一次撮合时被移除，绝不能被撮合出第二张订单
    //    途径：管理员把经济舱超卖上限 6 → 16，可售余量变 4，候补者直接订 2 张
    RouteCmd uc;
    uc.from = "广州";
    uc.to = "北京";
    uc.aircraft = "B737-800";
    uc.weekdays = 0b0101011;
    uc.cabins[CABIN_FIRST] = {8, 0, 2600};
    uc.cabins[CABIN_BUSINESS] = {16, 2, 1800};
    // 已售 156 超过原定额 150 → 新定额必须 ≥ 已售；156 恰为经济舱物理座位数
    uc.cabins[CABIN_ECONOMY] = {156, 16, 1200};
    TCHECK(svc.update_route("CZ3101", uc).ok());

    WaitCmd stale_wait;
    stale_wait.flight_no = "CZ3101";
    stale_wait.name = "先候后订";
    stale_wait.id_card = "440101199001062002";
    stale_wait.cabin = CABIN_ECONOMY;
    stale_wait.qty = 2;
    // ④-a 余票充足时登记 → 拒绝（NOT_ELIGIBLE）
    auto r4 = svc.join_waitlist(stale_wait);
    TCHECK(!r4.ok());
    TCHECK(r4.status == Status::NOT_ELIGIBLE);

    // ④-b 造一个"登记在先、订票在后"的残留：先退 4 张制造余票，
    //     此时"多余候补"(440101199001061001) 的登记会被撮合掉；
    //     但撮合时该人若无订单则会获票——为构造残留，改为直接考察：
    //     1) 登记人 A(…061001) 已在队列；2) A 通过订票拿到票是不可能的（一人一单仅限订单）。
    //     于是用另一条确定性路径：A 先订到票（余票 4），再登记候补 → DUPLICATE_ORDER。
    BookCmd a_book;
    a_book.flight_no = "CZ3101";
    a_book.name = "多余候补";
    a_book.id_card = "440101199001061001";
    a_book.cabin = CABIN_ECONOMY;
    a_book.qty = 2;
    // 余票 4 ≥ 2，可正常订票；但这单会让 A 同时保有"队列登记 + 订单"
    TCHECK(svc.book(a_book).ok());

    // ④-c 退 2 张 → 撮合前清理：A 的残留登记必须被移除，A 不能再获一单
    RefundCmd rf;
    rf.flight_no = "CZ3101";
    rf.id_card = "44010119900100001I";
    rf.qty = 2;
    auto rr = svc.refund(rf);
    TCHECK(rr.ok());
    int promoted_a = 0;
    for (const auto& p : rr.value.promoted) {
        if (p.id_card == "440101199001061001") ++promoted_a;
    }
    TCHECK_EQ(promoted_a, 0);  // A 已持订单，绝不被再次撮合

    // ④-d A 现在持有且仅持一张订单
    auto rec = svc.records("CZ3101", "", "440101199001061001");
    TCHECK(rec.ok());
    TCHECK_EQ(rec.value.size(), 1u);
    TCHECK(rec.value[0].status == ORDER_ACTIVE);
}

void test_recommend_flow() {
    std::printf("[service: recommend]\n");

    AirlineService svc(Date{2026, 11, 9});
    svc.seed_standard();

    // 北京为目的地：CZ3101（广州→北京，周一）、MU5203（西安→北京，周二四六）
    auto rec = svc.recommend("北京", "2026-11-09", "CZ3101");
    TCHECK(rec.ok());
    TCHECK(!rec.value.empty());
    bool has_mu5203 = false;
    for (const auto& v : rec.value) {
        if (v.flight_no == "MU5203") has_mu5203 = true;
        TCHECK(v.to == "北京");
        TCHECK(v.flight_no != "CZ3101");
    }
    TCHECK(has_mu5203);
}
