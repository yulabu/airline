#pragma once

#include "base/status.h"
#include "domain/instance.h"
#include "domain/route.h"
#include "domain/seat_layout.h"

namespace airline {

// ══════════════════════════════════════════════════════
//  余票。
//  注意两个数必须分开：
//    avail_display  —— 对外展示的"余票"，不显示负数
//    avail_sellable —— 还能卖多少（含超卖额度）
// ══════════════════════════════════════════════════════
int avail_display(const Route& route, const FlightInstance& inst, int cabin);
int avail_sellable(const Route& route, const FlightInstance& inst, int cabin);

// ══════════════════════════════════════════════════════
//  座位分配。纯函数，不修改 inst。
//  规则：全有或全无 —— 可用座位不足 qty 时 assigned = 0，
//        由调用方把整张订单标记为"超卖待定"。
// ══════════════════════════════════════════════════════
Status seat_alloc(const SeatLayout& layout, const FlightInstance& inst, int cabin,
                  int qty, const char* preferred,
                  char out[][kSeatCodeLen], int* assigned);

// ══════════════════════════════════════════════════════
//  候补撮合。
//  文档要求"首先询问排在第一的客户，若所退票额能满足则办理，否则依次询问"，
//  因此不是严格 FIFO，而是"首个能满足者优先"。
// ══════════════════════════════════════════════════════
bool waitlist_find(const LinkQueue<WaitEntry>& queue, int released_qty,
                   int* index, WaitEntry* out);

// ══════════════════════════════════════════════════════
//  配置校验。
// ══════════════════════════════════════════════════════
Status route_config_check(const Route& route);

// 管理员修改定额：new_quota 必须 >= 该舱位所有班次的最大已售量
Status quota_change_check(int new_quota, int sold_max, int physical_seats);

// 新建航线前检查航班号是否已被占用（由仓储层提供查询结果）
Status flight_no_check(const char* flight_no);

// ══════════════════════════════════════════════════════
//  登机调整。纯函数，只产出计划，不修改 inst。
// ══════════════════════════════════════════════════════
struct UpgradeMove {
    int order_index = -1;
    int to_cabin = CABIN_FIRST;
    char seats[kMaxSeatsPerOrder][kSeatCodeLen] = {{0}};
    int seat_count = 0;
};

struct CheckinPlan {
    SeqList<UpgradeMove> upgrades;
    SeqList<int> forfeits;  // 被强制退票的订单下标
    int boarded[CABIN_COUNT] = {0};
    int physical[CABIN_COUNT] = {0};
};

// 生成登机调整方案：
//   1. 经济舱超卖 → 商务舱有空位则升舱
//   2. 商务舱仍超卖 → 头等舱有空位则升舱
//   3. 仍有超卖 → 按订票序号倒序强制退票（最后订票者先退）
Status plan_checkin(const Route& route, const FlightInstance& inst, CheckinPlan* out);

}  // namespace airline
