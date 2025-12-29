#include "../include/PerfectLinks.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "IdGenerator.hpp"
#include "PackageProcessor.cpp"

PerfectLinks::PerfectLinks(Socket& _socket, uint32_t _nodeId, const std::string& path)
    : idGenerator(_nodeId)
      , nodeId(_nodeId)
      , socket(_socket)
      , logger(path)
      , stopReceiver(false)
{
}

PerfectLinks::~PerfectLinks()
{
    StopReceiver();
    StopResender();
    logger.FlushNow();
}

void PerfectLinks::SendMessageInChunks(const sockaddr_in& dest, std::vector<Packet>& batch, const uint32_t destNodeID)
{
    for (auto& pkt : batch)
    {
        AddPending(pkt, dest, destNodeID);
    }
    send_in_chunks(dest, batch, true);
}

void PerfectLinks::SendMessageInChunksNoWrite(const sockaddr_in& dest, std::vector<Packet>& batch,
                                              const uint32_t destNodeID)
{
    for (auto& pkt : batch)
    {
        AddPending(pkt, dest, destNodeID);
    }
    send_in_chunks(dest, batch, false);
}


void PerfectLinks::SendNoWrite(const sockaddr_in& dest, std::vector<Packet>& batch)
{
    send_in_chunks(dest, batch, false);
}

void PerfectLinks::send_in_chunks(const sockaddr_in& dest, std::vector<Packet>& batch, bool writeLog)
{
    const int maxBatch = 8;
    size_t offset = 0;
    while (offset < batch.size())
    {
        int take = static_cast<int>(std::min<size_t>(maxBatch, batch.size() - offset));

        size_t buf_size = 0;
        char* bytes = PackageProcessor::EncodeMany(batch.data() + offset, static_cast<size_t>(take), buf_size);
        socket.Send(bytes, reinterpret_cast<const sockaddr*>(&dest), buf_size);
        if (writeLog)
        {
            logger.LogSentRange(batch.data() + offset, static_cast<size_t>(take));
        }
        std::free(bytes);
        offset += static_cast<size_t>(take);
    }
}

std::vector<Packet> PerfectLinks::ReceivePackets(sockaddr_in& sender_addr) const
{
    size_t out = 0;
    char* bytes = socket.Receive(out, sender_addr);

    auto packets = PackageProcessor::DecodeMany(bytes, out);
    std::free(bytes);
    return packets;
}

std::pair<std::vector<Packet>, std::vector<Packet>> PerfectLinks::process(std::vector<Packet>& packets)
{
    std::vector<Packet> deliveredNow;
    deliveredNow.reserve(packets.size());

    std::vector<Packet> ackBatch;
    ackBatch.reserve(packets.size());

    for (auto& pkt : packets)
    {
        if (pkt.header.type == regular)
        {
            Packet ackPackage;
            ackPackage.header = Header(
                pkt.header.id,
                acknowledgment,
                nodeId
            );
            ackPackage.body = std::make_unique<Acknowledgment>();
            ackBatch.push_back(std::move(ackPackage));

            deliveredNow.push_back(std::move(pkt));
        }
        else if (pkt.header.type == acknowledgment)
        {
            ProcessPacket(pkt);
        }
    }

    return {deliveredNow, ackBatch};
}

void PerfectLinks::DeliverPackets(std::vector<Packet>& packets, sockaddr_in sender_addr)
{
    auto [deliveredNow, ackBatch] = process(packets);
    if (!deliveredNow.empty())
    {
        if (_fifoPacketProcessFunc)
        {
            _fifoPacketProcessFunc(deliveredNow);
        }
    }

    if (!ackBatch.empty())
    {
        SendNoWrite(sender_addr, ackBatch);
    }
}

void PerfectLinks::Receive()
{
    sockaddr_in sender_addr{};
    auto packets = ReceivePackets(sender_addr);
    DeliverPackets(packets, sender_addr);
}

uint64_t PerfectLinks::GetNextId()
{
    return idGenerator.next();
}

void PerfectLinks::AddPending(const Packet& pkt, const sockaddr_in& dest, uint32_t destNodeId)
{
    std::lock_guard lock(pendingMutex);

    PendingKey key{pkt.header.id, destNodeId};

    PendingEntry entry;
    entry.pkg = pkt;
    entry.dest = dest;
    entry.nextSend = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
    entry.backoff = std::chrono::milliseconds(50);

    pendingPackages.emplace(key, entry);
}

void PerfectLinks::ProcessPacket(const Packet& pkt)
{
    switch (pkt.header.type)
    {
    case regular:
        break;
    case acknowledgment:
        {
            const uint64_t msgId = pkt.header.id;
            const uint32_t srcNode = pkt.header.forwardedBy;

            const PendingKey key{msgId, srcNode};

            std::lock_guard lock(pendingMutex);
            pendingPackages.erase(key);
            break;
        }
    default:
        throw std::runtime_error("Unknown message type");
    }
}

void PerfectLinks::StartReceiver(UpperDeliverFn fn)
{
    _fifoPacketProcessFunc = std::move(fn);
    stopReceiver = false;
    receiverThread = std::thread(&PerfectLinks::ReceiverLoop, this);
}

void PerfectLinks::StopReceiver()
{
    stopReceiver = true;
    if (receiverThread.joinable())
        receiverThread.join();
}

void PerfectLinks::ReceiverLoop()
{
    while (!stopReceiver)
    {
        Receive();
    }
}

std::vector<PendingEntry> PerfectLinks::get_packets_to_send()
{
    std::vector<PendingEntry> toSend;

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(pendingMutex);
        for (auto & pendingPackage : pendingPackages)
        {
            auto& entry = pendingPackage.second;
            if (entry.nextSend <= now)
            {
                toSend.emplace_back(entry);
                entry.backoff = std::min(entry.backoff * 2, MAX_BACKOFF);
                entry.nextSend = now + entry.backoff;
            }
        }
    }

    return toSend;
}

void PerfectLinks::StartResender()
{
    stopResend = false;

    resendThread = std::thread([this]
    {
        while (!stopResend)
        {
            std::this_thread::sleep_for(RESENDER_TICK);

            std::vector<PendingEntry> toResend = get_packets_to_send();
            if (toResend.empty())
                continue;

            struct DestHash
            {
                size_t operator()(const sockaddr_in& s) const noexcept
                {
                    return std::hash<uint32_t>()(s.sin_addr.s_addr) ^
                        std::hash<uint16_t>()(s.sin_port);
                }
            };

            struct DestEq
            {
                bool operator()(const sockaddr_in& a, const sockaddr_in& b) const noexcept
                {
                    return a.sin_addr.s_addr == b.sin_addr.s_addr &&
                        a.sin_port == b.sin_port;
                }
            };

            std::unordered_map<sockaddr_in, std::vector<Packet>, DestHash, DestEq> batches;
            batches.reserve(toResend.size());

            for (auto& entry : toResend)
            {
                batches[entry.dest].push_back(entry.pkg);
            }

            for (auto& [dest, batch] : batches)
            {
                SendNoWrite(dest, batch);
            }
        }
    });
}


void PerfectLinks::StopResender()
{
    stopResend = true;
    if (resendThread.joinable())
        resendThread.join();
}
