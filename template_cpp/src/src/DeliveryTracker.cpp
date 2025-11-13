#include "DeliveryTracker.hpp"

#include <cstdint>
#include <mutex>

DeliveryTracker::DeliveryTracker(size_t maxSenders)
    : delivered(maxSenders)
{
}

bool DeliveryTracker::hasDelivered(uint32_t senderId, uint32_t seqNum)
{
    std::lock_guard lock(mtx);
    auto& v = delivered[senderId - 1];
    size_t idx = seqNum / 64;
    uint64_t mask = 1ULL << (seqNum % 64);
    return idx < v.size() && (v[idx] & mask);
}

void DeliveryTracker::markDelivered(uint32_t senderId, uint32_t seqNum)
{
    std::lock_guard lock(mtx);
    auto& v = delivered[senderId - 1];
    size_t idx = seqNum / 64;
    if (idx >= v.size())
        v.resize(idx + 1, 0);
    v[idx] |= 1ULL << (seqNum % 64);
}
