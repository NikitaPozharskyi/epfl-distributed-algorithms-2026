#pragma once
#include <memory>
#include <vector>

enum MessageType : uint16_t
{
    regular = 0,
    acknowledgment,
    latticeAgreement,
    notAcknowledgment,
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


// Nack for Lattice Agreement
struct NotAcknowledgment : Message
{
    std::vector<uint32_t> values;

    NotAcknowledgment() = default;

    std::unique_ptr<Message> clone() const override
    {
        return std::make_unique<NotAcknowledgment>(*this);
    }
};

struct Regular : Message
{
    std::unique_ptr<Message> clone() const override
    {
        return std::make_unique<Regular>(*this);
    }
};

struct LatticeAgreementMessage : Message
{
    uint32_t proposalNumber;
    uint32_t roundNumber;
    std::vector<uint64_t> values;

    std::unique_ptr<Message> clone() const override
    {
        return std::make_unique<LatticeAgreementMessage>(*this);
    }
};
