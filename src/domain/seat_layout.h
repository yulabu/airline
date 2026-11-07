#pragma once

#include "domain/types.h"

namespace airline {

// 一个舱位在机舱中的物理占位：起始排、结束排（含）、每排座位数、座位字母
struct CabinLayout {
    int row_start = 0;
    int row_end = 0;
    int seats_per_row = 0;
    char letters[kMaxLetters + 1] = {0};
};

struct SeatLayout {
    CabinLayout cabin[CABIN_COUNT];

    int max_row() const;
    int seats_of(int cabin) const;   // 该舱位物理座位数
    int total_seats() const;
};

// 按机型查布局；机型不存在返回 false
bool seat_layout_for(const char* aircraft, SeatLayout* out);

// 机型清单，供种子数据与前端下拉框使用
int seat_layout_count();
const char* seat_layout_aircraft(int index);

// 座位号工具："12A" ↔ (12, 'A')
bool seat_code_parse(const char* code, int* row, char* letter);
void seat_code_make(int row, char letter, char out[kSeatCodeLen]);

// 座位属于哪个舱位；不属于任何舱位返回 -1
int seat_cabin_of(const SeatLayout& layout, int row, char letter);

// 座位在该舱位内的序号（从 0 开始），用于占用位图下标；非法返回 -1
int seat_index_in_cabin(const SeatLayout& layout, int cabin, int row, char letter);

}  // namespace airline
