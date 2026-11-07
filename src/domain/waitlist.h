#pragma once

#include "domain/types.h"

namespace airline {

// 候补登记。按舱位分队列，退票时只能撮合同舱位的候补。
struct WaitEntry {
    char name[kNameLen] = {0};
    char id_card[kIdCardLen] = {0};
    int cabin = CABIN_ECONOMY;
    int qty = 0;           // 所需票量
    long join_seq = 0;     // 登记序号，决定队列次序
    char join_time[kTimeLen] = {0};
};

}  // namespace airline
