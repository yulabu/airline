#include "domain/policies.h"

namespace airline {

int avail_display(const Route& route, const FlightInstance& inst, int cabin) {
    if (cabin < 0 || cabin >= CABIN_COUNT) return 0;
    int left = route.cabins[cabin].quota - inst.sold[cabin];
    return left > 0 ? left : 0;
}

int avail_sellable(const Route& route, const FlightInstance& inst, int cabin) {
    if (cabin < 0 || cabin >= CABIN_COUNT) return 0;
    int limit = route.cabins[cabin].quota + route.cabins[cabin].overbook_max;
    int left = limit - inst.sold[cabin];
    return left > 0 ? left : 0;
}

}  // namespace airline
