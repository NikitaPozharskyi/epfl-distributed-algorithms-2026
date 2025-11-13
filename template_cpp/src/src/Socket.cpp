#include "Socket.hpp"

#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAXLINE 65536

Socket::Socket(in_addr_t ip, uint16_t port)
{
    if ((socket_file_description = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    {
        int buf = 8 * 1024 * 1024; // 8MB
        setsockopt(socket_file_description, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));
        setsockopt(socket_file_description, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
        int yes = 1;
        setsockopt(socket_file_description, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = ip;

    if (bind(socket_file_description,
             reinterpret_cast<const sockaddr*>(&server_address),
             sizeof(server_address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
}

Socket::~Socket()
{
    Stop();
}

char* Socket::Receive(size_t& out_size, sockaddr_in& sender_addr) const
{
    if (stopped.load())
        return nullptr;

    char* buffer = new char[MAXLINE];
    socklen_t len = sizeof(sender_addr);

    int n = static_cast<int>(recvfrom(socket_file_description, buffer, MAXLINE, 0,
        reinterpret_cast<struct sockaddr*>(&sender_addr), &len));

    if (n < 0)
    {
        if (!stopped.load())
        {
            perror("recvfrom failed");
        }
        delete[] buffer;
        return nullptr;
    }

    out_size = static_cast<size_t>(n);
    return buffer;
}

void Socket::Send(const char* bytes, const sockaddr* sendAddress, size_t length) const
{
    if (stopped.load())
        return;

    if (sendto(socket_file_description, bytes, length, 0, sendAddress, sizeof(*sendAddress)) < 0)
    {
        if (!stopped.load())
            perror("sendto failed");
    }
}

void Socket::Stop()
{
    if (stopped.exchange(true))
        return;

    if (socket_file_description >= 0)
    {
        shutdown(socket_file_description, SHUT_RDWR);
        close(socket_file_description);
        socket_file_description = -1;
    }
}
