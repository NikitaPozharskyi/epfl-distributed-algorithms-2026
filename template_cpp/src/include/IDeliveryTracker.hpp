#pragma once
#include <cstdint>

class IDeliveryTracker
{
public:
    virtual ~IDeliveryTracker() = default;

    virtual bool hasDelivered(uint32_t senderId, uint32_t seqNum) = 0;
    virtual void markDelivered(uint32_t senderId, uint32_t seqNum) = 0;
};
