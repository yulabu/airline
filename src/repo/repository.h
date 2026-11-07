#pragma once

#include <cstddef>
#include <string>

#include "base/date.h"
#include "base/status.h"
#include "domain/instance.h"
#include "domain/route.h"
#include "ds/hash_index.h"
#include "ds/seq_list.h"

namespace airline {

// 航班实例的复合键：(航班号, 日期)
struct FlightKey {
    char flight_no[kFlightNoLen] = {0};
    Date date;
};

struct FlightKeyHash {
    std::size_t operator()(const FlightKey& k) const;
};

bool operator==(const FlightKey& a, const FlightKey& b);

// 仓储层：系统状态的唯一持有者。
// 只负责"存在哪里"，不做任何业务判断，也不知道 JSON / HTTP 的存在。
class Repository {
public:
    Repository();
    ~Repository();

    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;

    // ── 航线 ──────────────────────────────────────────
    // 只检查航班号是否重复，配置合法性由 domain 的 route_config_check 负责
    Status add_route(const Route& r);
    Route* find_route(const char* flight_no);
    const Route* find_route(const char* flight_no) const;

    SeqList<Route>& routes() { return routes_; }
    const SeqList<Route>& routes() const { return routes_; }
    int route_count() const { return routes_.size(); }

    // 该航线所有已创建班次中，指定舱位的最大已售量
    int max_sold(const char* flight_no, int cabin) const;

    // ── 航班实例 ──────────────────────────────────────
    FlightInstance* find_instance(const char* flight_no, const Date& d);
    const FlightInstance* find_instance(const char* flight_no, const Date& d) const;

    // 惰性创建：不存在时才新建
    FlightInstance* get_or_create_instance(const char* flight_no, const Date& d);

    int instance_count() const { return instances_.size(); }

    template <class F>
    void for_each_instance(F f) {
        for (int i = 0; i < instances_.size(); ++i) f(*instances_[i]);
    }

    template <class F>
    void for_each_instance(F f) const {
        for (int i = 0; i < instances_.size(); ++i) f(*instances_[i]);
    }

    // ── 序号生成 ──────────────────────────────────────
    long next_order_seq() { return ++order_seq_; }
    long next_wait_seq() { return ++wait_seq_; }

    long order_seq() const { return order_seq_; }
    long wait_seq() const { return wait_seq_; }
    void set_order_seq(long v) { order_seq_ = v; }
    void set_wait_seq(long v) { wait_seq_ = v; }

    // "O000001"
    void make_order_id(long seq, char* out, int len) const;

    // ── 生命周期 ──────────────────────────────────────
    void clear();

private:
    SeqList<Route> routes_;
    SeqList<FlightInstance*> instances_;

    // 哈希表存的是下标而不是指针：
    // SeqList 扩容会搬家，指针会失效，下标不会。
    HashIndex<std::string, int> route_index_;
    HashIndex<FlightKey, int, FlightKeyHash> instance_index_;

    long order_seq_ = 0;
    long wait_seq_ = 0;
};

}  // namespace airline
