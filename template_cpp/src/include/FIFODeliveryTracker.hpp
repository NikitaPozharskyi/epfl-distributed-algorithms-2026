#pragma once

#include <vector>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "Models/Bitmask.hpp"
#include "Models/Packet.hpp"

class FIFODeliveryTracker
{
    uint32_t numProcesses;

    std::vector<uint32_t> lastDelivered;

    std::vector<std::unordered_map<uint32_t, Bitmask>> receivedFrom;
    std::vector<std::unordered_map<uint32_t, Packet>> storedPackets;

    std::mutex mtx;

public:
    FIFODeliveryTracker(uint32_t numProcesses);
    void removePacket(uint32_t sender, uint32_t seq);
    Packet& getPacket(uint32_t sender, uint32_t seq);
    bool markSeenAndCheckFirst(uint32_t senderId, uint32_t seqNum, uint32_t fromProcess, const Packet& pkt);
    void markSeen(uint32_t senderId, uint32_t seqNum, uint32_t fromProcess);
    int countSeen(uint32_t senderId, uint32_t seqNum);
    bool hasMajority(uint32_t senderId, uint32_t seqNum);
    bool canBeDelivered(uint32_t senderId, uint32_t seqNum) const;
    bool readyToDeliver(uint32_t senderId, uint32_t seqNum);
    void markDelivered(uint32_t senderId, uint32_t seqNum);
    uint32_t lastDeliveredOf(uint32_t senderId) const;
};
