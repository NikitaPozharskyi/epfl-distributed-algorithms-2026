#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>
#include <limits>
#include <memory>
#include <vector>

#include "Models/Packet.hpp"

class PacketEncoder
{
public:
    static char* EncodeMany(const Packet* packages, size_t count, size_t& out_size)
    {
        std::vector<size_t> body_sizes(count, 0);
        out_size = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const auto& p = packages[i];
            body_sizes[i] = GetMessageSize(p);
            out_size += sizeof(Header) + body_sizes[i];
        }

        char* buffer = static_cast<char*>(std::malloc(out_size));
        if (!buffer) throw std::bad_alloc();

        size_t offset = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const auto& package = packages[i];
            const size_t body_size = body_sizes[i];
            Header h_copy = package.header;

            h_copy.id = htobe64(package.header.id);
            h_copy.type = static_cast<MessageType>(htons(static_cast<uint16_t>(package.header.type)));
            h_copy.length = htons(static_cast<uint16_t>(body_size));
            h_copy.forwardedBy = htonl(package.header.forwardedBy);

            std::memcpy(buffer + offset, &h_copy, sizeof(Header));

            char* body_ptr = buffer + offset + sizeof(Header);

            switch (package.header.type)
            {
            case regular:
                {
                    break;
                }
            case acknowledgment:
                {
                    // ACK has no body; acknowledged id is carried in header.id
                    break;
                }
            case notAcknowledgment:
                {
                    const auto* msg = dynamic_cast<const NotAcknowledgment*>(package.body.get());
                    if (!msg)
                    {
                        std::free(buffer);
                        throw std::invalid_argument("Missing body for notAcknowledgment message");
                    }

                    const uint32_t value_count = htonl(static_cast<uint32_t>(msg->values.size()));
                    std::memcpy(body_ptr, &value_count, sizeof(uint32_t));

                    char* values_ptr = body_ptr + sizeof(uint32_t);
                    for (size_t j = 0; j < msg->values.size(); ++j)
                    {
                        const uint64_t net_val = htobe64(msg->values[j]);
                        std::memcpy(values_ptr + j * sizeof(uint64_t), &net_val, sizeof(uint64_t));
                    }
                    break;
                }
                
            case latticeAgreement:
                {
                    const auto* msg = dynamic_cast<const LatticeAgreementMessage*>(package.body.get());
                    if (!msg)
                    {
                        std::free(buffer);
                        throw std::invalid_argument("Missing body for latticeAgreement message");
                    }

                    const uint32_t proposal = htonl(msg->proposalNumber);
                    const uint32_t round = htonl(msg->roundNumber);
                    const uint32_t value_count = htonl(static_cast<uint32_t>(msg->values.size()));

                    std::memcpy(body_ptr, &proposal, sizeof(uint32_t));
                    std::memcpy(body_ptr + sizeof(uint32_t), &round, sizeof(uint32_t));
                    std::memcpy(body_ptr + 2 * sizeof(uint32_t), &value_count, sizeof(uint32_t));

                    char* values_ptr = body_ptr + 3 * sizeof(uint32_t);
                    for (size_t j = 0; j < msg->values.size(); ++j)
                    {
                        const uint64_t net_val = htobe64(msg->values[j]);
                        std::memcpy(values_ptr + j * sizeof(uint64_t), &net_val, sizeof(uint64_t));
                    }
                    break;
                }
            default:
                std::free(buffer);
                throw std::invalid_argument("Invalid message type");
            }

            offset += sizeof(Header) + body_size;
        }

        return buffer;
    }

    static std::vector<Packet> DecodeMany(const char* data, size_t size)
    {
        std::vector<Packet> result;
        size_t offset = 0;
        while (offset + sizeof(Header) <= size)
        {
            Header net_hdr;
            std::memcpy(&net_hdr, data + offset, sizeof(Header));

            Header hdr;
            hdr.id = be64toh(net_hdr.id);
            hdr.type = static_cast<MessageType>(ntohs(static_cast<uint16_t>(net_hdr.type)));
            hdr.length = ntohs(net_hdr.length);
            hdr.forwardedBy = ntohl(net_hdr.forwardedBy);

            size_t total_len = sizeof(Header) + hdr.length;
            if (offset + total_len > size)
            {
                break;
            }

            Packet package;
            package.header = hdr;

            const char* body_ptr = data + offset + sizeof(Header);

            switch (hdr.type)
            {
            case regular:
                {
                    break;
                }
            case acknowledgment:
                {
                    // ACK has no body; acknowledged id is carried in header.id
                    break;
                }
            case notAcknowledgment:
                {
                    constexpr size_t header_fields_size = sizeof(uint32_t);
                    if (hdr.length < header_fields_size)
                    {
                        throw std::invalid_argument("NotAcknowledgment message too small");
                    }

                    uint32_t value_count_net = 0;
                    std::memcpy(&value_count_net, body_ptr, sizeof(uint32_t));
                    const uint32_t value_count = ntohl(value_count_net);

                    const size_t expected_length = header_fields_size + static_cast<size_t>(value_count) * sizeof(uint64_t);
                    if (expected_length != hdr.length)
                    {
                        throw std::invalid_argument("NotAcknowledgment message length mismatch");
                    }

                    auto msg = std::make_unique<NotAcknowledgment>();
                    msg->values.resize(value_count);

                    const char* values_ptr = body_ptr + header_fields_size;
                    for (size_t j = 0; j < value_count; ++j)
                    {
                        uint64_t net_val = 0;
                        std::memcpy(&net_val, values_ptr + j * sizeof(uint64_t), sizeof(uint64_t));
                        msg->values[j] = be64toh(net_val);
                    }

                    package.body = std::move(msg);
                    break;
                }
            case latticeAgreement:
                {
                    constexpr size_t header_fields_size = sizeof(uint32_t) * 3;
                    if (hdr.length < header_fields_size)
                    {
                        throw std::invalid_argument("LatticeAgreement message too small");
                    }

                    uint32_t proposal_net = 0;
                    uint32_t round_net = 0;
                    uint32_t value_count_net = 0;

                    std::memcpy(&proposal_net, body_ptr, sizeof(uint32_t));
                    std::memcpy(&round_net, body_ptr + sizeof(uint32_t), sizeof(uint32_t));
                    std::memcpy(&value_count_net, body_ptr + 2 * sizeof(uint32_t), sizeof(uint32_t));

                    const uint32_t value_count = ntohl(value_count_net);
                    const size_t expected_length = header_fields_size + static_cast<size_t>(value_count) * sizeof(uint64_t);
                    if (expected_length != hdr.length)
                    {
                        throw std::invalid_argument("LatticeAgreement message length mismatch");
                    }

                    auto msg = std::make_unique<LatticeAgreementMessage>();
                    msg->proposalNumber = ntohl(proposal_net);
                    msg->roundNumber = ntohl(round_net);
                    msg->values.resize(value_count);

                    const char* values_ptr = body_ptr + header_fields_size;
                    for (size_t j = 0; j < value_count; ++j)
                    {
                        uint64_t net_val = 0;
                        std::memcpy(&net_val, values_ptr + j * sizeof(uint64_t), sizeof(uint64_t));
                        msg->values[j] = be64toh(net_val);
                    }

                    package.body = std::move(msg);
                    break;
                }
            default:
                throw std::invalid_argument("Invalid message type");
            }

            result.emplace_back(std::move(package));
            offset += total_len;
        }
        return result;
    }


    static size_t GetMessageSize(const Packet& package)
    {
        switch (package.header.type)
        {
        case regular: return 0; // no body for Regular;
        case acknowledgment: return 0; // no body for ack;
        case notAcknowledgment:
            {
                const auto* msg = dynamic_cast<const NotAcknowledgment*>(package.body.get());
                if (!msg) throw std::invalid_argument("Missing body for notAcknowledgment message");
                if (msg->values.size() > std::numeric_limits<uint32_t>::max())
                {
                    throw std::invalid_argument("Too many values in notAcknowledgment message");
                }

                const size_t body_size = sizeof(uint32_t) + sizeof(uint64_t) * msg->values.size();
                if (body_size > std::numeric_limits<uint16_t>::max())
                {
                    throw std::invalid_argument("notAcknowledgment message too large to encode");
                }
                return body_size;
            }
        case latticeAgreement:
            {
                const auto* msg = dynamic_cast<const LatticeAgreementMessage*>(package.body.get());
                if (!msg) throw std::invalid_argument("Missing body for latticeAgreement message");
                if (msg->values.size() > std::numeric_limits<uint32_t>::max())
                {
                    throw std::invalid_argument("Too many values in latticeAgreement message");
                }

                const size_t body_size = sizeof(uint32_t) * 3 + sizeof(uint64_t) * msg->values.size();
                if (body_size > std::numeric_limits<uint16_t>::max())
                {
                    throw std::invalid_argument("latticeAgreement message too large to encode");
                }
                return body_size;
            }
        default: throw std::invalid_argument("Invalid message type");
        }
    }
};
