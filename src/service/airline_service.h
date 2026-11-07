#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "base/date.h"
#include "base/result.h"
#include "repo/repository.h"
#include "service/dto.h"

namespace airline {

// 应用服务：唯一的用例入口，唯一允许修改系统状态的地方。
// 每个公开方法 = 一个用例，内部遵循 validate → mutate 两段式。
// 持久化不在本层：api 层在变更类用例成功后调用 store::flush。
class AirlineService {
public:
    explicit AirlineService(Date today = date_today());

    void set_today(const Date& d) { today_ = d; }
    Date today() const { return today_; }

    // 仅供启动装载与持久化层使用；业务代码不得绕过用例直接改仓库
    Repository& repo() { return repo_; }
    std::mutex& mutex() { return mu_; }

    // 内置种子：18 条航线（≥15），覆盖 4 种机型、每日/隔日班次
    void seed_standard();

    // ── 乘客用例 ──────────────────────────────────────
    Result<std::vector<RouteView>> search_routes(const std::string& from,
                                                 const std::string& to, int weekday);
    Result<std::vector<RouteView>> flight_instances(const std::string& flight_no, int n);
    Result<BookingOut> book(const BookCmd& cmd);
    Result<RefundOut> refund(const RefundCmd& cmd);
    Result<WaitOut> join_waitlist(const WaitCmd& cmd);
    // 订票失败时推荐同一目的地的其他航线（扩展 1）
    Result<std::vector<RouteView>> recommend(const std::string& to,
                                             const std::string& date,
                                             const std::string& exclude_flight_no);
    Result<SeatMapView> seat_map(const std::string& flight_no, const std::string& date,
                                 int cabin);

    // ── 管理员用例 ────────────────────────────────────
    Result<RouteView> create_route(const RouteCmd& cmd);
    Result<RouteView> update_route(const std::string& flight_no, const RouteCmd& cmd);
    Result<std::vector<RecordRow>> records(const std::string& flight_no,
                                           const std::string& date,
                                           const std::string& id_card);
    // 模拟登机：超卖调整 + 强制退票（扩展 2）
    Result<CheckinOut> checkin(const std::string& flight_no, const std::string& date);

private:
    // 校验辅助（只读，可失败）
    Route* require_route(const char* flight_no, Status* err);
    Route* require_route(const std::string& flight_no, Status* err) {
        return require_route(flight_no.c_str(), err);
    }

    // 视图组装
    RouteView build_view(const Route& route, const Date& on_date) const;

    Date today_;
    Repository repo_;
    std::mutex mu_;
};

}  // namespace airline
