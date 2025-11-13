#pragma once

#include <vector>
#include <cstdint>
#include <mutex>

class DeliveryTracker {
    std::vector<std::vector<uint64_t>> delivered;
    std::mutex mtx;

public:

    DeliveryTracker(size_t maxSenders);
    bool hasDelivered(uint32_t senderId, uint32_t seqNum);
    void markDelivered(uint32_t senderId, uint32_t seqNum);
};
