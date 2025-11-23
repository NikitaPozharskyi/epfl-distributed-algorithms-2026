#pragma once

#include <vector>

#include "DestinationMap.hpp"
#include "FIFODeliveryTracker.hpp"
#include "PerfectLinks.hpp"
#include "Models/Node.hpp"
#include "Models/Packet.hpp"

class FIFOBroadcast
{
    FIFODeliveryTracker _tracker;
    std::vector<Node> _nodes;
    DestinationMap _destinationMap;
    PerfectLinks _links;
    std::vector<PendingEntry> _sent;

    std::mutex pendingMutex;

    void OnPacketsFromPL(std::vector<Packet>& packets);

public:
    explicit FIFOBroadcast(const std::vector<Node>& nodes,
                           Socket& socket,
                           const std::string& outputPath,
                           uint32_t nodeID);

    void Broadcast(uint32_t amount);
    void Rebroadcast(std::vector<Packet> packets);

    void StopResender();
    void StopReceiver();
};
