// 核心层单元测试。只链接 airline_core，不依赖 HTTP / JSON 任何第三方库。
#include <cstdio>
#include <cstring>
#include <string>

#include "base/date.h"
#include "domain/instance.h"
#include "domain/policies.h"
#include "domain/route.h"
#include "domain/seat_layout.h"
#include "ds/hash_index.h"
#include "ds/link_queue.h"
#include "ds/seq_list.h"

using namespace airline;

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            std::printf("  [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!((a) == (b))) {                                                    \
            std::printf("  [FAIL] %s:%d  %s == %s\n", __FILE__, __LINE__, #a, #b); \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

// ─────────────────────────── SeqList ───────────────────────────

static void test_seq_list() {
    std::printf("[SeqList]\n");

    SeqList<int> list;
    CHECK(list.empty());
    CHECK_EQ(list.size(), 0);

    for (int i = 0; i < 100; ++i) CHECK(list.push_back(i) == Status::OK);
    CHECK_EQ(list.size(), 100);
    CHECK(list.capacity() >= 100);  // 触发过扩容

    CHECK_EQ(list.at(0), 0);
    CHECK_EQ(list.at(99), 99);

    CHECK(list.insert_at(0, -1) == Status::OK);
    CHECK_EQ(list.at(0), -1);
    CHECK_EQ(list.at(1), 0);
    CHECK_EQ(list.size(), 101);

    CHECK(list.insert_at(101, 100) == Status::OK);  // 尾部插入
    CHECK_EQ(list.at(101), 100);

    CHECK(list.insert_at(999, 0) == Status::INVALID_ARG);
    CHECK(list.remove_at(-1) == Status::INVALID_ARG);
    CHECK(list.remove_at(999) == Status::INVALID_ARG);

    CHECK(list.remove_at(0) == Status::OK);
    CHECK_EQ(list.at(0), 0);
    CHECK_EQ(list.size(), 101);

    int probe = 50;
    CHECK_EQ(list.find_index(probe), 50);
    int missing = 12345;
    CHECK_EQ(list.find_index(missing), -1);

    // 拷贝后互不影响
    SeqList<int> copy(list);
    CHECK_EQ(copy.size(), list.size());
    CHECK(copy.remove_at(0) == Status::OK);
    CHECK_EQ(copy.size(), list.size() - 1);

    // 移动后源对象为空
    SeqList<int> moved(std::move(copy));
    CHECK_EQ(moved.size(), 100);
    CHECK(copy.empty());

    // 清空不释放容量
    int cap_before = list.capacity();
    list.clear();
    CHECK(list.empty());
    CHECK_EQ(list.capacity(), cap_before);
}

// ─────────────────────────── LinkQueue ───────────────────────────

static void test_link_queue() {
    std::printf("[LinkQueue]\n");

    LinkQueue<int> q;
    CHECK(q.empty());

    for (int i = 0; i < 10; ++i) CHECK(q.enqueue(i) == Status::OK);
    CHECK_EQ(q.size(), 10);
    CHECK(q.front() != nullptr);
    CHECK_EQ(*q.front(), 0);

    int v = -1;
    CHECK(q.dequeue(&v) == Status::OK);
    CHECK_EQ(v, 0);
    CHECK_EQ(*q.front(), 1);
    CHECK_EQ(q.size(), 9);

    // 跳过队首，取走下标 2 的元素（模拟候补撮合"张数不够就跳过"）
    v = -1;
    CHECK(q.remove_at(2, &v) == Status::OK);
    CHECK_EQ(v, 3);
    CHECK_EQ(q.size(), 8);
    CHECK(q.remove_at(99, &v) == Status::INVALID_ARG);

    // 删除后剩余元素必须保持原入队顺序
    const int expected[] = {1, 2, 4, 5, 6, 7, 8, 9};
    int k = 0;
    q.for_each([&](const int& x) {
        CHECK_EQ(x, expected[k]);
        ++k;
    });
    CHECK_EQ(k, 8);

    LinkQueue<int> copy(q);
    CHECK_EQ(copy.size(), q.size());

    q.clear();
    CHECK(q.empty());
    CHECK(q.front() == nullptr);
    CHECK(q.dequeue(&v) == Status::WAITLIST_EMPTY);

    // 清空后可继续使用
    CHECK(q.enqueue(42) == Status::OK);
    CHECK_EQ(*q.front(), 42);
}

// ─────────────────────────── HashIndex ───────────────────────────

static void test_hash_index() {
    std::printf("[HashIndex]\n");

    HashIndex<std::string, int> idx;
    CHECK_EQ(idx.size(), 0);

    CHECK(idx.put("CZ3101", 1) == Status::OK);
    CHECK(idx.put("CA1301", 2) == Status::OK);
    CHECK(idx.put("MU5101", 3) == Status::OK);
    CHECK_EQ(idx.size(), 3);

    int* p = idx.get("CA1301");
    CHECK(p != nullptr);
    if (p) CHECK_EQ(*p, 2);

    CHECK(idx.get("XX0000") == nullptr);
    CHECK_EQ(idx.bucket_count(), 64);

    // 覆盖写
    CHECK(idx.put("CA1301", 20) == Status::OK);
    CHECK_EQ(idx.size(), 3);
    CHECK_EQ(*idx.get("CA1301"), 20);

    CHECK(idx.remove("MU5101"));
    CHECK(!idx.remove("MU5101"));
    CHECK_EQ(idx.size(), 2);

    // 大量键触发扩容
    HashIndex<std::string, int> big;
    for (int i = 0; i < 1000; ++i) {
        CHECK(big.put("K" + std::to_string(i), i) == Status::OK);
    }
    CHECK_EQ(big.size(), 1000);
    CHECK(big.bucket_count() > 64);

    int found = 0;
    for (int i = 0; i < 1000; i += 97) {
        int* q = big.get("K" + std::to_string(i));
        if (q && *q == i) ++found;
    }
    CHECK_EQ(found, 11);  // 0,97,...,970

    CHECK(big.max_bucket_length() >= 1);

    big.clear();
    CHECK_EQ(big.size(), 0);
}

// ─────────────────────────── Date ───────────────────────────

static void test_date() {
    std::printf("[Date]\n");

    Date d{2026, 11, 6};
    CHECK(date_valid(d));

    CHECK(!date_valid(Date{2026, 2, 30}));
    CHECK(!date_valid(Date{2026, 13, 1}));
    CHECK(!date_valid(Date{2026, 0, 1}));
    CHECK(date_valid(Date{2024, 2, 29}));   // 闰年
    CHECK(!date_valid(Date{2026, 2, 29}));  // 平年

    // 已知基准：2000-01-01 是周六
    CHECK_EQ(date_weekday(Date{2000, 1, 1}), 6);
    // 2024-01-01 是周一
    CHECK_EQ(date_weekday(Date{2024, 1, 1}), 1);

    Date next = date_add_days(Date{2026, 12, 31}, 1);
    CHECK_EQ(next.y, 2027);
    CHECK_EQ(next.m, 1);
    CHECK_EQ(next.d, 1);

    Date prev = date_add_days(Date{2026, 3, 1}, -1);
    CHECK_EQ(prev.m, 2);
    CHECK_EQ(prev.d, 28);

    CHECK_EQ(date_diff_days(Date{2026, 11, 10}, Date{2026, 11, 6}), 4);
    CHECK_EQ(date_diff_days(Date{2027, 1, 1}, Date{2026, 12, 31}), 1);

    // 周六(6) 的下一个周一是 +2 天
    Date monday = date_next_weekday(Date{2026, 11, 7}, 1);
    CHECK_EQ(date_weekday(Date{2026, 11, 7}), 6);
    CHECK_EQ(monday.d, 9);
    CHECK_EQ(date_weekday(monday), 1);

    // 当天就是目标星期几时应返回当天
    Date same = date_next_weekday(Date{2026, 11, 6}, date_weekday(Date{2026, 11, 6}));
    CHECK(date_equal(same, Date{2026, 11, 6}));

    Date parsed;
    CHECK(date_from_string("2026-11-06", &parsed));
    CHECK(date_equal(parsed, Date{2026, 11, 6}));
    CHECK(!date_from_string("2026-11-31", &parsed));
    CHECK(!date_from_string("2026/11/06", &parsed));
    CHECK(!date_from_string("26-11-06", &parsed));

    CHECK_EQ(date_to_string(Date{2026, 1, 5}), std::string("2026-01-05"));

    CHECK_EQ(std::string(weekday_name(1)), std::string("周一"));
    CHECK_EQ(std::string(weekday_name(7)), std::string("周日"));
}

// ─────────────────────────── Domain: 座位布局 ───────────────────────────

static Route make_route(const char* flight_no = "CZ3101") {
    Route r;
    std::strncpy(r.flight_no, flight_no, kFlightNoLen - 1);
    std::strncpy(r.from, "广州", kCityLen - 1);
    std::strncpy(r.to, "北京", kCityLen - 1);
    std::strncpy(r.aircraft, "B737-800", kAircraftLen - 1);
    r.weekdays = 0x01;  // 周一

    SeatLayout layout;
    seat_layout_for(r.aircraft, &layout);
    for (int c = 0; c < CABIN_COUNT; ++c) {
        r.cabins[c].quota = layout.seats_of(c);
        r.cabins[c].overbook_max = 0;
        r.cabins[c].price = 1000 + c * 500;
    }
    return r;
}

static void test_seat_layout() {
    std::printf("[SeatLayout]\n");

    SeatLayout layout;
    CHECK(seat_layout_for("B737-800", &layout));
    CHECK(!seat_layout_for("UNKNOWN", &layout));

    CHECK_EQ(layout.seats_of(CABIN_FIRST), 8);      // 2 排 × 4 座
    CHECK_EQ(layout.seats_of(CABIN_BUSINESS), 16);  // 4 排 × 4 座
    CHECK_EQ(layout.seats_of(CABIN_ECONOMY), 156);  // 26 排 × 6 座
    CHECK_EQ(layout.total_seats(), 180);
    CHECK_EQ(layout.max_row(), 32);

    int row = 0;
    char letter = 0;
    CHECK(seat_code_parse("12A", &row, &letter));
    CHECK_EQ(row, 12);
    CHECK_EQ(letter, 'A');

    CHECK(seat_code_parse("7f", &row, &letter));  // 小写也接受
    CHECK_EQ(row, 7);
    CHECK_EQ(letter, 'F');

    CHECK(!seat_code_parse("A12", &row, &letter));
    CHECK(!seat_code_parse("0A", &row, &letter));
    CHECK(!seat_code_parse("41A", &row, &letter));
    CHECK(!seat_code_parse("12G", &row, &letter));
    CHECK(!seat_code_parse("", &row, &letter));

    char code[kSeatCodeLen];
    seat_code_make(7, 'C', code);
    CHECK_EQ(std::string(code), std::string("7C"));
    seat_code_make(12, 'F', code);
    CHECK_EQ(std::string(code), std::string("12F"));

    CHECK_EQ(seat_cabin_of(layout, 1, 'A'), static_cast<int>(CABIN_FIRST));
    CHECK_EQ(seat_cabin_of(layout, 3, 'A'), static_cast<int>(CABIN_BUSINESS));
    CHECK_EQ(seat_cabin_of(layout, 7, 'A'), static_cast<int>(CABIN_ECONOMY));
    CHECK_EQ(seat_cabin_of(layout, 1, 'B'), -1);  // 头等舱没有 B 座

    CHECK_EQ(seat_index_in_cabin(layout, CABIN_ECONOMY, 7, 'A'), 0);
    CHECK_EQ(seat_index_in_cabin(layout, CABIN_ECONOMY, 7, 'F'), 5);
    CHECK_EQ(seat_index_in_cabin(layout, CABIN_ECONOMY, 8, 'A'), 6);
}

// ─────────────────────────── Domain: 余票与座位分配 ───────────────────────────

static void test_avail_and_alloc() {
    std::printf("[avail / seat_alloc]\n");

    Route route = make_route();
    FlightInstance inst;
    SeatLayout layout;
    seat_layout_for(route.aircraft, &layout);

    CHECK_EQ(avail_display(route, inst, CABIN_ECONOMY), 156);
    CHECK_EQ(avail_sellable(route, inst, CABIN_ECONOMY), 156);

    route.cabins[CABIN_ECONOMY].overbook_max = 5;
    CHECK_EQ(avail_display(route, inst, CABIN_ECONOMY), 156);
    CHECK_EQ(avail_sellable(route, inst, CABIN_ECONOMY), 161);

    inst.sold[CABIN_ECONOMY] = 150;
    CHECK_EQ(avail_display(route, inst, CABIN_ECONOMY), 6);
    CHECK_EQ(avail_sellable(route, inst, CABIN_ECONOMY), 11);

    inst.sold[CABIN_ECONOMY] = 160;
    CHECK_EQ(avail_display(route, inst, CABIN_ECONOMY), 0);   // 不显示负数
    CHECK_EQ(avail_sellable(route, inst, CABIN_ECONOMY), 1);

    // 无偏好：从经济舱第一排第一个座位开始
    FlightInstance fresh;
    char seats[kMaxSeatsPerOrder][kSeatCodeLen];
    int assigned = 0;
    CHECK(seat_alloc(layout, fresh, CABIN_ECONOMY, 2, "", seats, &assigned) == Status::OK);
    CHECK_EQ(assigned, 2);
    CHECK_EQ(std::string(seats[0]), std::string("7A"));
    CHECK_EQ(std::string(seats[1]), std::string("7B"));

    // 指定座位
    CHECK(seat_alloc(layout, fresh, CABIN_ECONOMY, 2, "9C", seats, &assigned) == Status::OK);
    CHECK_EQ(assigned, 2);
    CHECK_EQ(std::string(seats[0]), std::string("9C"));
    CHECK_EQ(std::string(seats[1]), std::string("9D"));

    // 指定座位已被占
    fresh.set_occupied(CABIN_ECONOMY, 9, 'C', true);
    CHECK(seat_alloc(layout, fresh, CABIN_ECONOMY, 1, "9C", seats, &assigned) ==
          Status::SEAT_TAKEN);

    // 座位不属于该舱位
    CHECK(seat_alloc(layout, fresh, CABIN_ECONOMY, 1, "1A", seats, &assigned) ==
          Status::CABIN_MISMATCH);

    // 全有或全无：只剩 1 个空座却要 2 张 → assigned = 0
    FlightInstance full;
    const CabinLayout& eco = layout.cabin[CABIN_ECONOMY];
    int left = layout.seats_of(CABIN_ECONOMY) - 1;
    for (int r = eco.row_start; r <= eco.row_end && left > 0; ++r) {
        for (int i = 0; i < eco.seats_per_row && left > 0; ++i) {
            full.set_occupied(CABIN_ECONOMY, r, eco.letters[i], true);
            --left;
        }
    }
    CHECK(seat_alloc(layout, full, CABIN_ECONOMY, 2, "", seats, &assigned) == Status::OK);
    CHECK_EQ(assigned, 0);
    CHECK(seat_alloc(layout, full, CABIN_ECONOMY, 1, "", seats, &assigned) == Status::OK);
    CHECK_EQ(assigned, 1);
    CHECK_EQ(std::string(seats[0]), std::string("32F"));  // 最后一个空座
}

// ─────────────────────────── Domain: 候补撮合 ───────────────────────────

static void test_waitlist() {
    std::printf("[waitlist_find]\n");

    LinkQueue<WaitEntry> q;

    WaitEntry a;
    std::strncpy(a.name, "甲", kNameLen - 1);
    a.qty = 3;
    a.cabin = CABIN_ECONOMY;
    a.join_seq = 1;

    WaitEntry b;
    std::strncpy(b.name, "乙", kNameLen - 1);
    b.qty = 2;
    b.cabin = CABIN_ECONOMY;
    b.join_seq = 2;

    WaitEntry c;
    std::strncpy(c.name, "丙", kNameLen - 1);
    c.qty = 1;
    c.cabin = CABIN_ECONOMY;
    c.join_seq = 3;

    q.enqueue(a);
    q.enqueue(b);
    q.enqueue(c);

    int idx = -1;
    WaitEntry out;

    // 退 2 张：队首要 3 张，跳过，命中第二个
    CHECK(waitlist_find(q, 2, &idx, &out));
    CHECK_EQ(idx, 1);
    CHECK_EQ(std::string(out.name), std::string("乙"));

    // 退 1 张：队首、第二个都不够，命中第三个
    CHECK(waitlist_find(q, 1, &idx, &out));
    CHECK_EQ(idx, 2);

    // 退 3 张：队首就够
    CHECK(waitlist_find(q, 3, &idx, &out));
    CHECK_EQ(idx, 0);

    // 没有能满足的
    CHECK(!waitlist_find(q, 0, &idx, &out));
    LinkQueue<WaitEntry> empty;
    CHECK(!waitlist_find(empty, 5, &idx, &out));
}

// ─────────────────────────── Domain: 配置校验 ───────────────────────────

static void test_config_check() {
    std::printf("[config_check]\n");

    Route route = make_route();
    CHECK(route_config_check(route) == Status::OK);
    CHECK_EQ(route.capacity(), 180);

    Route bad = route;
    bad.cabins[CABIN_ECONOMY].quota = 157;  // 超过物理座位
    CHECK(route_config_check(bad) == Status::INVALID_ARG);

    bad = route;
    std::strncpy(bad.from, "北京", kCityLen - 1);  // 起点=终点
    CHECK(route_config_check(bad) == Status::INVALID_ARG);

    bad = route;
    bad.weekdays = 0;
    CHECK(route_config_check(bad) == Status::INVALID_ARG);

    bad = route;
    bad.cabins[CABIN_FIRST].price = 0;
    CHECK(route_config_check(bad) == Status::INVALID_ARG);

    bad = route;
    std::strncpy(bad.aircraft, "BOEING-X", kAircraftLen - 1);
    CHECK(route_config_check(bad) == Status::INVALID_ARG);

    // 修改定额：不能低于已售
    CHECK(quota_change_check(40, 40, 156) == Status::OK);
    CHECK(quota_change_check(39, 40, 156) == Status::QUOTA_BELOW_SOLD);
    CHECK(quota_change_check(157, 0, 156) == Status::INVALID_ARG);
    CHECK(quota_change_check(-1, 0, 156) == Status::INVALID_ARG);
}

// ─────────────────────────── Domain: 登机调整 ───────────────────────────

static void fill_cabin_seats(const SeatLayout& layout, FlightInstance* inst, int cabin,
                             long* seq, int limit = 100000) {
    const CabinLayout& cl = layout.cabin[cabin];
    int made = 0;
    for (int r = cl.row_start; r <= cl.row_end; ++r) {
        for (int i = 0; i < cl.seats_per_row; ++i) {
            if (made >= limit) return;
            char letter = cl.letters[i];
            Order o;
            o.qty = 1;
            o.cabin = cabin;
            o.book_seq = ++(*seq);
            o.status = ORDER_ACTIVE;
            o.seat_count = 1;
            seat_code_make(r, letter, o.seats[0]);
            inst->set_occupied(cabin, r, letter, true);
            inst->orders.push_back(o);
            ++made;
        }
    }
}

static void add_pending(FlightInstance* inst, int cabin, int qty, long* seq,
                        const char* name) {
    Order o;
    o.qty = qty;
    o.cabin = cabin;
    o.book_seq = ++(*seq);
    o.status = ORDER_ACTIVE;
    o.seat_pending = true;
    o.seat_count = 0;
    std::strncpy(o.name, name, kNameLen - 1);
    inst->orders.push_back(o);
}

static void test_checkin_plan() {
    std::printf("[plan_checkin]\n");

    Route route = make_route();
    SeatLayout layout;
    seat_layout_for(route.aircraft, &layout);

    // 场景一：经济舱满座 + 超卖 3 张，商务舱全空 → 3 单全部升舱
    {
        FlightInstance inst;
        long seq = 0;
        fill_cabin_seats(layout, &inst, CABIN_ECONOMY, &seq);
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "超卖甲");
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "超卖乙");
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "超卖丙");

        CheckinPlan plan;
        CHECK(plan_checkin(route, inst, &plan) == Status::OK);
        CHECK_EQ(plan.physical[CABIN_ECONOMY], 156);
        // boarded 记录的是升舱调整后的最终分布
        CHECK_EQ(plan.boarded[CABIN_ECONOMY], 156);
        CHECK_EQ(plan.boarded[CABIN_BUSINESS], 3);
        CHECK_EQ(plan.upgrades.size(), 3);
        CHECK_EQ(plan.forfeits.size(), 0);

        for (int i = 0; i < plan.upgrades.size(); ++i) {
            CHECK_EQ(plan.upgrades[i].to_cabin, static_cast<int>(CABIN_BUSINESS));
            CHECK_EQ(plan.upgrades[i].seat_count, 1);
            int row = 0;
            char letter = 0;
            CHECK(seat_code_parse(plan.upgrades[i].seats[0], &row, &letter));
            CHECK_EQ(seat_cabin_of(layout, row, letter), static_cast<int>(CABIN_BUSINESS));
        }

        // 后订票的先升舱（序号倒序）
        CHECK(plan.upgrades[0].order_index > plan.upgrades[1].order_index);
    }

    // 场景二：商务舱也满，头等舱也满 → 只能强制退票，最后订票的先退
    {
        FlightInstance inst;
        long seq = 0;
        fill_cabin_seats(layout, &inst, CABIN_ECONOMY, &seq);
        fill_cabin_seats(layout, &inst, CABIN_BUSINESS, &seq);
        fill_cabin_seats(layout, &inst, CABIN_FIRST, &seq);
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "最早");
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "中间");
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "最晚");

        int last_index = inst.orders.size() - 1;

        CheckinPlan plan;
        CHECK(plan_checkin(route, inst, &plan) == Status::OK);
        CHECK_EQ(plan.upgrades.size(), 0);
        CHECK_EQ(plan.forfeits.size(), 3);
        // 最后订票的排在最前
        CHECK_EQ(plan.forfeits[0], last_index);
    }

    // 场景三：商务舱满座 + 超卖 2 张，头等舱有空位 → 2 单升头等
    {
        FlightInstance inst;
        long seq = 0;
        fill_cabin_seats(layout, &inst, CABIN_BUSINESS, &seq);
        add_pending(&inst, CABIN_BUSINESS, 1, &seq, "商务超卖甲");
        add_pending(&inst, CABIN_BUSINESS, 1, &seq, "商务超卖乙");

        CheckinPlan plan;
        CHECK(plan_checkin(route, inst, &plan) == Status::OK);
        CHECK_EQ(plan.upgrades.size(), 2);
        CHECK_EQ(plan.forfeits.size(), 0);
        CHECK_EQ(plan.boarded[CABIN_FIRST], 2);
        CHECK_EQ(plan.boarded[CABIN_BUSINESS], layout.seats_of(CABIN_BUSINESS));
    }

    // 场景四：商务舱只卖了 10 张，经济舱超卖 3 张 → 3 单升商务
    {
        FlightInstance inst;
        long seq = 0;
        fill_cabin_seats(layout, &inst, CABIN_ECONOMY, &seq);
        fill_cabin_seats(layout, &inst, CABIN_BUSINESS, &seq, 10);
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "经济超卖甲");
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "经济超卖乙");
        add_pending(&inst, CABIN_ECONOMY, 1, &seq, "经济超卖丙");

        CheckinPlan plan;
        CHECK(plan_checkin(route, inst, &plan) == Status::OK);
        CHECK_EQ(plan.upgrades.size(), 3);
        CHECK_EQ(plan.forfeits.size(), 0);
        CHECK_EQ(plan.boarded[CABIN_BUSINESS], 13);
        CHECK_EQ(plan.boarded[CABIN_ECONOMY], layout.seats_of(CABIN_ECONOMY));
        CHECK_EQ(plan.boarded[CABIN_FIRST], 0);
    }
}

// test_service.cpp 里的 g_checks / g_failures 声明在 test_main.cpp 定义
extern int g_checks;
extern int g_failures;

void test_service_flow();
void test_checkin_flow();
void test_admin_flow();
void test_waitlist_guard();
void test_recommend_flow();

// ─────────────────────────── main ───────────────────────────

int main() {
    std::printf("=== airline core tests ===\n");
    test_seq_list();
    test_link_queue();
    test_hash_index();
    test_date();
    test_seat_layout();
    test_avail_and_alloc();
    test_waitlist();
    test_config_check();
    test_checkin_plan();
    test_service_flow();
    test_checkin_flow();
    test_admin_flow();
    test_waitlist_guard();
    test_recommend_flow();

    std::printf("---\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
