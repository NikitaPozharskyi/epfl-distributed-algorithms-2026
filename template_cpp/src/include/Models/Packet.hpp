#pragma once

#include <memory>
#include "Message.hpp"

struct Header
{
    uint64_t id;
    MessageType type;
    uint16_t length;
    uint32_t forwardedBy;

    Header() = default;

    Header(const uint64_t _id, const MessageType _type, const uint32_t forwardedBy)
        : id(_id), type(_type), length(0), forwardedBy(forwardedBy)
    {
    }
};

struct Packet
{
    Header header;
    std::unique_ptr<Message> body;

    Packet() = default;

    Packet(const Packet& other)
        : header(other.header)
    {
        if (other.body) body = other.body->clone();
    }

    Packet& operator=(const Packet& other)
    {
        if (this == &other) return *this;
        header = other.header;
        if (other.body) body = other.body->clone();
        else body.reset();
        return *this;
    }

    Packet(Packet&&) noexcept = default;
    Packet& operator=(Packet&&) noexcept = default;
};
