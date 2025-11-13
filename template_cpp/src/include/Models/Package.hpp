#pragma once

#include <memory>
#include "Message.hpp"

struct Header
{
    uint64_t id;
    MessageType type;
    uint16_t length;

    Header() = default;

    Header(uint64_t _id, MessageType _type)
        : id(_id), type(_type), length(0)
    {
    }
};

struct Package
{
    Header header;
    std::unique_ptr<Message> body;

    // Default constructor OK
    Package() = default;

    // Deep-copy constructor
    Package(const Package& other)
        : header(other.header)
    {
        if (other.body) body = other.body->clone();
    }

    // Deep-copy assignment
    Package& operator=(const Package& other)
    {
        if (this == &other) return *this;
        header = other.header;
        if (other.body) body = other.body->clone();
        else body.reset();
        return *this;
    }

    // Move operations: keep defaults (efficient)
    Package(Package&&) noexcept = default;
    Package& operator=(Package&&) noexcept = default;
};
