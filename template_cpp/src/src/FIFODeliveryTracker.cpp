#include "FIFODeliveryTracker.hpp"

#include <mutex>


FIFODeliveryTracker::FIFODeliveryTracker(uint32_t numProcesses)
    : numProcesses(numProcesses),
      lastDelivered(numProcesses, 0),
      receivedFrom(numProcesses),
      storedPackets(numProcesses)
{
}

uint32_t FIFODeliveryTracker::lastDeliveredOf(const uint32_t senderId) const
{
    return lastDelivered[senderId - 1];
}

bool FIFODeliveryTracker::markSeenAndCheckFirst(
    const uint32_t senderId,
    const uint32_t seqNum,
    const uint32_t fromProcess,
    const Packet& pkt)
{
    std::lock_guard lock(mtx);

    auto& mp = receivedFrom[senderId - 1];
    auto [it, inserted] = mp.try_emplace(seqNum, Bitmask{});

    if (inserted)
    {
        storedPackets[senderId - 1][seqNum] = pkt;
    }

    auto& bm = it->second;

    uint32_t idx = fromProcess - 1;
    if (idx < 64)
        bm.lo |= (1ULL << idx);
    else
        bm.hi |= (1ULL << (idx - 64));

    return inserted;
}

Packet& FIFODeliveryTracker::getPacket(uint32_t sender, uint32_t seq)
{
    return storedPackets[sender - 1][seq];
}

void FIFODeliveryTracker::removePacket(uint32_t sender, uint32_t seq)
{
    storedPackets[sender - 1].erase(seq);
}

void FIFODeliveryTracker::markSeen(uint32_t senderId, uint32_t seqNum, uint32_t fromProcess)
{
    std::lock_guard lock(mtx);

    auto& [lo, hi] = receivedFrom[senderId - 1][seqNum];

    if (const uint32_t idx = fromProcess - 1; idx < 64)
        lo |= (1ULL << idx);
    else
        hi |= (1ULL << (idx - 64));
}

int FIFODeliveryTracker::countSeen(uint32_t senderId, uint32_t seqNum)
{
    const auto& [lo, hi] = receivedFrom[senderId - 1][seqNum];

    return __builtin_popcountll(lo) +
        __builtin_popcountll(hi);
}

bool FIFODeliveryTracker::hasMajority(uint32_t senderId, uint32_t seqNum)
{
    const uint32_t seen = static_cast<uint32_t>(countSeen(senderId, seqNum));
    return seen > numProcesses / 2;
}

bool FIFODeliveryTracker::canBeDelivered(uint32_t senderId, uint32_t seqNum) const
{
    return lastDelivered[senderId - 1] + 1 == seqNum;
}

bool FIFODeliveryTracker::readyToDeliver(uint32_t senderId, uint32_t seqNum)
{
    std::lock_guard lock(mtx);
    return hasMajority(senderId, seqNum)
        && canBeDelivered(senderId, seqNum);
}

void FIFODeliveryTracker::markDelivered(uint32_t senderId, uint32_t seqNum)
{
    std::lock_guard lock(mtx);

    lastDelivered[senderId - 1] = seqNum;

    receivedFrom[senderId - 1].erase(seqNum);
}
