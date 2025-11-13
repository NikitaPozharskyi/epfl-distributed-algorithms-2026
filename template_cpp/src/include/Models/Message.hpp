#pragma once

enum MessageType : uint16_t
{
    regular = 0,
    acknowledgment,
};

struct Message
{
    virtual ~Message() = default;
    virtual std::unique_ptr<Message> clone() const = 0;
};

struct Acknowledgment : Message
{
    Acknowledgment() = default;

    std::unique_ptr<Message> clone() const override
    {
        return std::make_unique<Acknowledgment>(*this);
    }
};

struct Regular : Message
{
    uint32_t number;
    std::unique_ptr<Message> clone() const override {
        return std::make_unique<Regular>(*this);
    }
};
