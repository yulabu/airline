#pragma once

#include <string>

#include "repo/repository.h"

namespace airline {
namespace store {

// 状态持久化：整个仓库 ↔ state.json（先写临时文件再 rename，避免写坏）。
// 归属：store 层，依赖 json 第三方库；核心层（base/ds/domain/repo/service）不碰它。

// 从 path 装载。文件不存在返回 false（调用方应灌种子）。
bool load(Repository& repo, const std::string& path);

// 全量落盘。成功返回 true。
bool flush(Repository& repo, const std::string& path);

}  // namespace store
}  // namespace airline
