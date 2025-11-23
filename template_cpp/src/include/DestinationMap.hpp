#pragma once

#include <vector>
#include <netinet/in.h>

#include "Models/Node.hpp"

class DestinationMap
{
    std::vector<sockaddr_in> _destinationMap;

public:
    DestinationMap();
    DestinationMap(const std::vector<Node>& nodes);
    sockaddr_in GetDestinationByIndex(uint32_t Index) const;
    sockaddr_in GetDestinationByID(uint32_t ID) const;
};
