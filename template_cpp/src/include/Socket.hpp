#pragma once

#include <netinet/in.h>
#include <atomic>

class Socket
{
    sockaddr_in server_address;
    int socket_file_description;
    mutable std::atomic<bool> stopped{false};

public:
    explicit Socket(in_addr_t ip, uint16_t port);
    ~Socket();
    void Stop();

    char* Receive(size_t& out_size, sockaddr_in& sender_addr) const;
    void Send(const char* bytes, const sockaddr* sendAddress, size_t length) const;
};
