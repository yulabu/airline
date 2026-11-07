#include <cstring>

#include "domain/policies.h"

namespace airline {
namespace {

int count_free(const SeatLayout& layout, const FlightInstance& inst, int cabin) {
    const CabinLayout& cl = layout.cabin[cabin];
    int n = 0;
    for (int r = cl.row_start; r <= cl.row_end; ++r) {
        for (int i = 0; i < cl.seats_per_row; ++i) {
            if (inst.is_free(cabin, r, cl.letters[i])) ++n;
        }
    }
    return n;
}

}  // namespace

Status seat_alloc(const SeatLayout& layout, const FlightInstance& inst, int cabin,
                  int qty, const char* preferred,
                  char out[][kSeatCodeLen], int* assigned) {
    if (assigned) *assigned = 0;
    if (!out) return Status::INVALID_ARG;
    if (cabin < 0 || cabin >= CABIN_COUNT) return Status::INVALID_ARG;
    if (qty <= 0 || qty > kMaxSeatsPerOrder) return Status::INVALID_ARG;

    const CabinLayout& cl = layout.cabin[cabin];

    // 起点：默认从该舱位第一排第一个座位开始
    int start_row = cl.row_start;
    int start_col = 0;

    if (preferred && preferred[0] != '\0') {
        int row = 0;
        char letter = 0;
        if (!seat_code_parse(preferred, &row, &letter)) return Status::INVALID_ARG;
        if (seat_cabin_of(layout, row, letter) != cabin) return Status::CABIN_MISMATCH;
        if (!inst.is_free(cabin, row, letter)) return Status::SEAT_TAKEN;

        start_row = row;
        const char* pos = std::strchr(cl.letters, letter);
        start_col = static_cast<int>(pos - cl.letters);
    }

    // 全有或全无：座位不够就整单待定，避免"半张订单有座半张没座"
    if (count_free(layout, inst, cabin) < qty) {
        return Status::OK;
    }

    const int rows = cl.row_end - cl.row_start + 1;
    const int total = rows * cl.seats_per_row;
    const int start_idx = (start_row - cl.row_start) * cl.seats_per_row + start_col;

    int n = 0;
    for (int k = 0; k < total && n < qty; ++k) {
        int idx = (start_idx + k) % total;
        int row = cl.row_start + idx / cl.seats_per_row;
        int col = idx % cl.seats_per_row;
        char letter = cl.letters[col];
        if (!inst.is_free(cabin, row, letter)) continue;

        seat_code_make(row, letter, out[n]);
        ++n;
    }

    if (assigned) *assigned = n;
    return Status::OK;
}

}  // namespace airline
