#pragma once
#include <netinet/in.h>

struct Node
{
    uint32_t id;
    in_addr_t ip;
    uint16_t port;

    sockaddr_in GetDestinationAddress()
    {
        sockaddr_in dest;
        dest.sin_port = htons(port);
        dest.sin_addr.s_addr = ip;
        return dest;
    }
};
