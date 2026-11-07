#include "domain/seat_layout.h"

#include <cstring>

namespace airline {
namespace {

struct LayoutDef {
    const char* aircraft;
    // 每个舱位：起始排, 结束排, 座位字母
    int row_start[CABIN_COUNT];
    int row_end[CABIN_COUNT];
    const char* letters[CABIN_COUNT];
};

// 四类常见机型。头等/商务为 2-2 布局（A C D F），经济舱为 3-3 布局（A-F）。
const LayoutDef kLayouts[] = {
    {"B737-800", {1, 3, 7},   {2, 6, 32},  {"ACDF", "ACDF", "ABCDEF"}},
    {"A320",     {1, 3, 6},   {2, 5, 30},  {"ACDF", "ACDF", "ABCDEF"}},
    {"A330-300", {1, 4, 10},  {3, 9, 39},  {"ACDF", "ACDF", "ABCDEF"}},
    {"B787-9",   {1, 4, 11},  {3, 10, 38}, {"ACDF", "ACDF", "ABCDEF"}},
};

const int kLayoutCount = static_cast<int>(sizeof(kLayouts) / sizeof(kLayouts[0]));

}  // namespace

int SeatLayout::max_row() const {
    int m = 0;
    for (int c = 0; c < CABIN_COUNT; ++c) {
        if (cabin[c].row_end > m) m = cabin[c].row_end;
    }
    return m;
}

int SeatLayout::seats_of(int cabin) const {
    if (cabin < 0 || cabin >= CABIN_COUNT) return 0;
    const CabinLayout& cl = this->cabin[cabin];
    int rows = cl.row_end - cl.row_start + 1;
    if (rows <= 0) return 0;
    return rows * cl.seats_per_row;
}

int SeatLayout::total_seats() const {
    int sum = 0;
    for (int c = 0; c < CABIN_COUNT; ++c) sum += seats_of(c);
    return sum;
}

bool seat_layout_for(const char* aircraft, SeatLayout* out) {
    if (!aircraft || !out) return false;
    for (int i = 0; i < kLayoutCount; ++i) {
        if (std::strcmp(kLayouts[i].aircraft, aircraft) != 0) continue;

        const LayoutDef& def = kLayouts[i];
        for (int c = 0; c < CABIN_COUNT; ++c) {
            CabinLayout& cl = out->cabin[c];
            cl.row_start = def.row_start[c];
            cl.row_end = def.row_end[c];
            cl.letters[0] = '\0';
            std::strncpy(cl.letters, def.letters[c], kMaxLetters);
            cl.letters[kMaxLetters] = '\0';
            cl.seats_per_row = static_cast<int>(std::strlen(cl.letters));
        }
        return true;
    }
    return false;
}

int seat_layout_count() { return kLayoutCount; }

const char* seat_layout_aircraft(int index) {
    if (index < 0 || index >= kLayoutCount) return "";
    return kLayouts[index].aircraft;
}

bool seat_code_parse(const char* code, int* row, char* letter) {
    if (!code) return false;
    int r = 0;
    int i = 0;
    while (code[i] >= '0' && code[i] <= '9') {
        r = r * 10 + (code[i] - '0');
        ++i;
        if (i > 2) return false;  // 最多两位排号
    }
    if (i == 0) return false;     // 必须有排号
    char l = code[i];
    if (l >= 'a' && l <= 'z') l = static_cast<char>(l - 'a' + 'A');
    if (l < 'A' || l > 'F') return false;
    if (code[i + 1] != '\0') return false;

    if (r < 1 || r > kMaxRows) return false;
    if (row) *row = r;
    if (letter) *letter = l;
    return true;
}

void seat_code_make(int row, char letter, char out[kSeatCodeLen]) {
    if (row < 10) {
        out[0] = static_cast<char>('0' + row);
        out[1] = letter;
        out[2] = '\0';
    } else {
        out[0] = static_cast<char>('0' + row / 10);
        out[1] = static_cast<char>('0' + row % 10);
        out[2] = letter;
        out[3] = '\0';
    }
}

int seat_cabin_of(const SeatLayout& layout, int row, char letter) {
    for (int c = 0; c < CABIN_COUNT; ++c) {
        const CabinLayout& cl = layout.cabin[c];
        if (row < cl.row_start || row > cl.row_end) continue;
        if (std::strchr(cl.letters, letter) == nullptr) continue;
        return c;
    }
    return -1;
}

int seat_index_in_cabin(const SeatLayout& layout, int cabin, int row, char letter) {
    if (cabin < 0 || cabin >= CABIN_COUNT) return -1;
    const CabinLayout& cl = layout.cabin[cabin];
    if (row < cl.row_start || row > cl.row_end) return -1;

    const char* pos = std::strchr(cl.letters, letter);
    if (!pos) return -1;

    int col = static_cast<int>(pos - cl.letters);
    return (row - cl.row_start) * cl.seats_per_row + col;
}

}  // namespace airline
