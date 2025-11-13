#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>
#include <memory>
#include <vector>

#include "Models/Package.hpp"

class PackageProcessor
{
public:
    static char* EncodeMany(const Package* packages, size_t count, size_t& out_size)
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

            std::memcpy(buffer + offset, &h_copy, sizeof(Header));
            char* bodyPtr = buffer + offset + sizeof(Header);

            switch (package.header.type)
            {
            case regular:
                {
                    auto* r = dynamic_cast<Regular*>(package.body.get());
                    uint32_t net_number = htonl(r->number);
                    std::memcpy(bodyPtr, &net_number, sizeof(net_number));
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
    static char* EncodePackage(Package& package, size_t& out_size)
    {
        size_t body_size = GetMessageSize(package.header.type);
        out_size = sizeof(Header) + body_size;
        char* buffer = static_cast<char*>(std::malloc(out_size));
        if (!buffer) throw std::bad_alloc();

        Header h_copy = package.header;

        h_copy.id = htobe64(package.header.id);
        h_copy.type = static_cast<MessageType>(htons(static_cast<uint16_t>(package.header.type)));
        h_copy.length = htons(static_cast<uint16_t>(body_size));

        std::memcpy(buffer, &h_copy, sizeof(Header));
        char* bodyPtr = buffer + sizeof(Header);

        switch (package.header.type)
        {
        case regular:
            {
                auto* r = dynamic_cast<Regular*>(package.body.get());
                uint32_t net_number = htonl(r->number);
                std::memcpy(bodyPtr, &net_number, sizeof(net_number));
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

        return buffer;
    }

    static Package DecodePackage(const char* data)
    {
        Header net_hdr;
        std::memcpy(&net_hdr, data, sizeof(Header));

        Header hdr;
        hdr.id = be64toh(net_hdr.id);
        hdr.type = static_cast<MessageType>(ntohs(static_cast<uint16_t>(net_hdr.type)));
        hdr.length = ntohs(net_hdr.length);

        Package package;
        package.header = hdr;

        size_t expected = GetMessageSize(hdr.type);
        if (hdr.length != expected)
        {
            throw std::runtime_error("body length mismatch");
        }

        const char* bodyPtr = data + sizeof(Header);

        switch (hdr.type)
        {
        case regular:
            {
                auto r = std::make_unique<Regular>();
                uint32_t net_number;
                std::memcpy(&net_number, bodyPtr, sizeof(net_number));
                r->number = ntohl(net_number);
                package.body = std::move(r);
                break;
            }
        case acknowledgment:
            {
                package.body = std::make_unique<Acknowledgment>();
                break;
            }
        default:
            throw std::invalid_argument("Invalid message type");
        }

        return package;
    }

    static std::vector<Package> DecodeMany(const char* data, size_t size)
    {
        std::vector<Package> result;
        size_t offset = 0;
        while (offset + sizeof(Header) <= size)
        {
            Header net_hdr;
            std::memcpy(&net_hdr, data + offset, sizeof(Header));

            Header hdr;
            hdr.id = be64toh(net_hdr.id);
            hdr.type = static_cast<MessageType>(ntohs(static_cast<uint16_t>(net_hdr.type)));
            hdr.length = ntohs(net_hdr.length);

            size_t total_len = sizeof(Header) + hdr.length;
            if (offset + total_len > size)
            {
                break;
            }

            Package package;
            package.header = hdr;

            const char* bodyPtr = data + offset + sizeof(Header);
            switch (hdr.type)
            {
            case regular:
                {
                    auto r = std::make_unique<Regular>();
                    uint32_t net_number;
                    std::memcpy(&net_number, bodyPtr, sizeof(net_number));
                    r->number = ntohl(net_number);
                    package.body = std::move(r);
                    break;
                }
            case acknowledgment:
                {
                    package.body = std::make_unique<Acknowledgment>();
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
        case regular: return sizeof(Regular);
        case acknowledgment: return 0; // no body for ACK
        default: throw std::invalid_argument("Invalid message type");
        }
    }
};
