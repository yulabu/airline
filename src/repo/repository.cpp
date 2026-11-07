#include "repo/repository.h"

#include <cstdio>
#include <cstring>
#include <new>

namespace airline {

bool operator==(const FlightKey& a, const FlightKey& b) {
    return std::strcmp(a.flight_no, b.flight_no) == 0 && date_equal(a.date, b.date);
}

std::size_t FlightKeyHash::operator()(const FlightKey& k) const {
    std::size_t h = 1469598103934665603ull;
    for (const char* p = k.flight_no; *p != '\0'; ++p) {
        h ^= static_cast<unsigned char>(*p);
        h *= 1099511628211ull;
    }
    h ^= static_cast<std::size_t>(k.date.y * 10000 + k.date.m * 100 + k.date.d);
    h *= 1099511628211ull;
    return h;
}

Repository::Repository() : instance_index_(128) {}

Repository::~Repository() {
    clear();
}

Status Repository::add_route(const Route& r) {
    std::string key(r.flight_no);
    if (route_index_.get(key) != nullptr) return Status::FLIGHT_EXISTS;

    int idx = routes_.size();
    if (routes_.push_back(r) != Status::OK) return Status::INTERNAL;

    if (route_index_.put(key, idx) != Status::OK) {
        routes_.remove_at(idx);
        return Status::INTERNAL;
    }
    return Status::OK;
}

Route* Repository::find_route(const char* flight_no) {
    if (!flight_no) return nullptr;
    int* idx = route_index_.get(std::string(flight_no));
    if (!idx) return nullptr;
    if (*idx < 0 || *idx >= routes_.size()) return nullptr;
    return &routes_.at(*idx);
}

const Route* Repository::find_route(const char* flight_no) const {
    if (!flight_no) return nullptr;
    const int* idx = route_index_.get(std::string(flight_no));
    if (!idx) return nullptr;
    if (*idx < 0 || *idx >= routes_.size()) return nullptr;
    return &routes_.at(*idx);
}

int Repository::max_sold(const char* flight_no, int cabin) const {
    if (!flight_no || cabin < 0 || cabin >= CABIN_COUNT) return 0;
    int best = 0;
    for (int i = 0; i < instances_.size(); ++i) {
        const FlightInstance* inst = instances_.at(i);
        if (std::strcmp(inst->flight_no, flight_no) != 0) continue;
        if (inst->sold[cabin] > best) best = inst->sold[cabin];
    }
    return best;
}

FlightInstance* Repository::find_instance(const char* flight_no, const Date& d) {
    if (!flight_no) return nullptr;
    FlightKey key;
    std::strncpy(key.flight_no, flight_no, kFlightNoLen - 1);
    key.date = d;

    int* idx = instance_index_.get(key);
    if (!idx) return nullptr;
    if (*idx < 0 || *idx >= instances_.size()) return nullptr;
    return instances_.at(*idx);
}

const FlightInstance* Repository::find_instance(const char* flight_no, const Date& d) const {
    if (!flight_no) return nullptr;
    FlightKey key;
    std::strncpy(key.flight_no, flight_no, kFlightNoLen - 1);
    key.date = d;

    const int* idx = instance_index_.get(key);
    if (!idx) return nullptr;
    if (*idx < 0 || *idx >= instances_.size()) return nullptr;
    return instances_.at(*idx);
}

FlightInstance* Repository::get_or_create_instance(const char* flight_no, const Date& d) {
    if (!flight_no || flight_no[0] == '\0') return nullptr;
    if (FlightInstance* existing = find_instance(flight_no, d)) return existing;

    FlightInstance* inst = new (std::nothrow) FlightInstance();
    if (!inst) return nullptr;

    std::strncpy(inst->flight_no, flight_no, kFlightNoLen - 1);
    inst->date = d;

    int idx = instances_.size();
    if (instances_.push_back(inst) != Status::OK) {
        delete inst;
        return nullptr;
    }

    FlightKey key;
    std::strncpy(key.flight_no, flight_no, kFlightNoLen - 1);
    key.date = d;

    if (instance_index_.put(key, idx) != Status::OK) {
        instances_.remove_at(idx);
        delete inst;
        return nullptr;
    }
    return inst;
}

void Repository::make_order_id(long seq, char* out, int len) const {
    if (!out || len <= 0) return;
    std::snprintf(out, static_cast<size_t>(len), "O%06ld", seq);
}

void Repository::clear() {
    for (int i = 0; i < instances_.size(); ++i) {
        delete instances_.at(i);
    }
    instances_.clear();
    routes_.clear();
    route_index_.clear();
    instance_index_.clear();
    order_seq_ = 0;
    wait_seq_ = 0;
}

}  // namespace airline
