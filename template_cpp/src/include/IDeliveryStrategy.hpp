#pragma once
#include <utility>
#include <vector>

#include "Models/Packet.hpp"
#include "Models/PendingEntry.hpp"

class IDeliveryStrategy
{
public:
    virtual ~IDeliveryStrategy() = default;

    virtual std::pair<
        std::vector<Packet>, // deliveredNow
        std::vector<Packet> // acks
    > process(std::vector<Packet>& packets) = 0;

    virtual std::vector<PendingEntry> get_packets_to_send() = 0;
};
