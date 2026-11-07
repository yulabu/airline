#include "domain/route.h"

#include <cstdio>
#include <cstring>

namespace airline {

int Route::capacity() const {
    int sum = 0;
    for (int c = 0; c < CABIN_COUNT; ++c) sum += cabins[c].quota;
    return sum;
}

bool Route::flies_on(int weekday) const {
    return weekday_mask_contains(weekdays, weekday);
}

Date Route::next_date(const Date& today) const {
    // 一周之内必然命中，最多看 7 天
    for (int delta = 0; delta < 7; ++delta) {
        Date d = date_add_days(today, delta);
        if (flies_on(date_weekday(d))) return d;
    }
    return today;
}

void Route::weekday_text(char* out, int out_len) const {
    if (!out || out_len <= 0) return;
    out[0] = '\0';

    bool first = true;
    for (int wd = 1; wd <= 7; ++wd) {
        if (!flies_on(wd)) continue;
        if (!first) {
            std::strncat(out, "/", static_cast<size_t>(out_len - std::strlen(out) - 1));
        }
        std::strncat(out, weekday_name(wd),
                     static_cast<size_t>(out_len - std::strlen(out) - 1));
        first = false;
    }
    if (first) {
        std::snprintf(out, static_cast<size_t>(out_len), "无");
    }
}

}  // namespace airline
