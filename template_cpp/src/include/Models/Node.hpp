#pragma once
#include <netinet/in.h>

struct Node
{
    uint32_t id;
    in_addr_t ip;
    uint16_t port;
};
