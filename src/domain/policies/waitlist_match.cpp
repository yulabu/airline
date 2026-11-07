#include "domain/policies.h"

namespace airline {

bool waitlist_find(const LinkQueue<WaitEntry>& queue, int released_qty,
                   int* index, WaitEntry* out) {
    if (released_qty <= 0) return false;

    bool found = false;
    int i = 0;
    queue.for_each([&](const WaitEntry& e) {
        if (!found && e.qty <= released_qty) {
            found = true;
            if (index) *index = i;
            if (out) *out = e;
        }
        ++i;
    });
    return found;
}

}  // namespace airline
