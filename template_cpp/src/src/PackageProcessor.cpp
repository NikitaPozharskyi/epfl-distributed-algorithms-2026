#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>
#include <memory>
#include <vector>

#include "Models/Packet.hpp"

class PackageProcessor
{
public:
    static char* EncodeMany(const Packet* packages, size_t count, size_t& out_size)
    {
        out_size = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const auto& p = packages[i];
            out_size += sizeof(Header) + GetMessageSize(p.header.type);
        }

        char* buffer = static_cast<char*>(std::malloc(out_size));
        if (!buffer) throw std::bad_alloc();

        size_t offset = 0;
        for (size_t i = 0; i < count; ++i)
        {
            const auto& package = packages[i];
            size_t body_size = GetMessageSize(package.header.type);
            Header h_copy = package.header;

            h_copy.id = htobe64(package.header.id);
            h_copy.type = static_cast<MessageType>(htons(static_cast<uint16_t>(package.header.type)));
            h_copy.length = htons(static_cast<uint16_t>(body_size));
            h_copy.forwardedBy = htonl(package.header.forwardedBy);

            std::memcpy(buffer + offset, &h_copy, sizeof(Header));

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
            default:
                throw std::invalid_argument("Invalid message type");
            }

            result.emplace_back(std::move(package));
            offset += total_len;
        }
        return result;
    }


    static size_t GetMessageSize(MessageType type)
    {
        switch (type)
        {
        case regular: return 0; // no body for Regular;
        case acknowledgment: return 0; // no body for ack;
        default: throw std::invalid_argument("Invalid message type");
        }
    }
};
