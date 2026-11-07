#pragma once

#include <utility>

#include "base/status.h"

namespace airline {

// 查询类用例的返回值：携带一个值，或者一个错误码。
// 命令类用例没有返回值，直接用 Status。
template <class T>
struct Result {
    Status status = Status::INTERNAL;
    T value{};

    bool ok() const { return status == Status::OK; }

    static Result success(T v) {
        Result r;
        r.status = Status::OK;
        r.value = std::move(v);
        return r;
    }

    // 失败但携带上下文（如 NO_SEAT 时的剩余可售量）
    static Result failure_with(Status s, T v) {
        Result r;
        r.status = s;
        r.value = std::move(v);
        return r;
    }

    static Result failure(Status s) {
        Result r;
        r.status = s;
        return r;
    }
};

}  // namespace airline
