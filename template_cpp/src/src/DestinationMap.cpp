#include "DestinationMap.hpp"

#include <iostream>
#include <ostream>

DestinationMap::DestinationMap()
{
}

DestinationMap::DestinationMap(const std::vector<Node>& nodes)
{
    _destinationMap.resize(nodes.size());
    for (auto& node : nodes)
    {
        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(node.port);
        dest.sin_addr.s_addr = node.ip;
        _destinationMap[node.id - 1] = dest;
    }
}

sockaddr_in DestinationMap::GetDestinationByIndex(const uint32_t Index) const
{
    return _destinationMap[Index];
}

sockaddr_in DestinationMap::GetDestinationByID(const uint32_t ID) const
{
    return _destinationMap[ID - 1];
}
