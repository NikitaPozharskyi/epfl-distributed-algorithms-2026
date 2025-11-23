#pragma once
#include <chrono>
#include <netinet/in.h>

#include "Packet.hpp"

struct PendingEntry
{
    Packet pkg;
    sockaddr_in dest;
    std::chrono::steady_clock::time_point nextSend;
    std::chrono::milliseconds backoff{50};
};
