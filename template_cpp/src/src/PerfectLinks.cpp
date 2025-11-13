#include "../include/PerfectLinks.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>

#include "IdGenerator.hpp"
#include "PackageProcessor.cpp"

PerfectLinks::PerfectLinks(Socket& _socket, uint32_t _nodeId, const std::string& path, bool startAck)
    : nodeId(_nodeId)
      , idGenerator(_nodeId)
      , socket(_socket)
      , logger(path)
      , delivery_tracker(128)
      , stopAckReceiver(false)
      , startAckReceiver(startAck)
{
}

PerfectLinks::~PerfectLinks()
{
    StopAckReceiver();
    StopResender();
    logger.FlushNow();
}

void PerfectLinks::send_in_chunks(const sockaddr_in& dest, std::vector<Package>& batch, bool writeLog)
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
        offset += take;
    }
}

void PerfectLinks::Receive()
{
    while (true)
    {
        size_t out = 0;
        sockaddr_in sender_addr{};
        char* bytes = socket.Receive(out, sender_addr);

        if (bytes == nullptr)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        auto packets = PackageProcessor::DecodeMany(bytes, out);

        std::vector<Package> deliveredNow;
        deliveredNow.reserve(packets.size());

        std::vector<Package> ackBatch;
        ackBatch.reserve(packets.size());

        for (auto& pkt : packets)
        {
            if (pkt.header.type == regular)
            {
                Package ackPackage;
                ackPackage.header = Header(
                    pkt.header.id,
                    acknowledgment
                );
                ackPackage.body = std::make_unique<Acknowledgment>();
                ackBatch.push_back(std::move(ackPackage));

                {
                    uint32_t id = IdGenerator::getNodeId(pkt.header.id);
                    uint32_t counter = IdGenerator::getCounter(pkt.header.id);
                    if (delivery_tracker.hasDelivered(id, counter))
                    {
                        continue;
                    }

                    delivery_tracker.markDelivered(id, counter);
                }

                deliveredNow.push_back(std::move(pkt));
            }
            else if (pkt.header.type == acknowledgment)
            {
                ProcessPacket(pkt);
            }
        }

        if (!deliveredNow.empty())
        {
            logger.LogReceived(deliveredNow);
        }

        if (!ackBatch.empty())
        {
            send_in_chunks(sender_addr, ackBatch);
        }

        delete[] bytes;
    }
}

void PerfectLinks::Send(in_addr_t ip, uint16_t port, int packageCounter)
{
    if (startAckReceiver)
    {
        StartAckReceiver();
        StartResender();
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = ip;

    std::cout << "Sending to: " << inet_ntoa(dest.sin_addr) << ":" << port << std::endl;


    std::vector<Package> packages;
    packages.reserve(packageCounter);

    for (int k = 0; k < packageCounter; ++k)
    {
        Package pkt;
        pkt.header = Header(
            idGenerator.next(),
            regular
        );

        pkt.body = std::make_unique<Regular>();
        if (auto* regularMsg = dynamic_cast<Regular*>(pkt.body.get()))
        {
            regularMsg->number = IdGenerator::getCounter(pkt.header.id);
        }

        auto id = pkt.header.id;
        {
            std::lock_guard lock(pendingMutex);
            PendingEntry entry;
            entry.pkg = pkt;
            entry.dest = dest;
            entry.nextSend = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
            entry.backoff = std::chrono::milliseconds(50);
            entry.retries = 0;
            pendingPackages.emplace(id, std::move(entry));
        }

        packages.push_back(std::move(pkt));
    }

    send_in_chunks(dest, packages, true);
}

void PerfectLinks::ProcessPacket(Package& package)
{
    switch (package.header.type)
    {
    case regular:
        break;
    case acknowledgment:
        {
            std::lock_guard lock(pendingMutex);
            uint64_t acked_id = package.header.id;
            pendingPackages.erase(acked_id);
            break;
        }
    default:
        throw std::runtime_error("Unknown message type");
    }
}

void PerfectLinks::StartAckReceiver()
{
    stopAckReceiver = false;
    ackReceiverThread = std::thread(&PerfectLinks::AckReceiverLoop, this);
}

void PerfectLinks::StopAckReceiver()
{
    stopAckReceiver = true;
    if (ackReceiverThread.joinable())
        ackReceiverThread.join();
}

void PerfectLinks::AckReceiverLoop()
{
    while (!stopAckReceiver)
    {
        Receive();
    }
}

void PerfectLinks::StartResender()
{
    stopResend = false;
    resendThread = std::thread([this]()
    {
        while (!stopResend)
        {
            std::this_thread::sleep_for(RESENDER_TICK);

            std::vector<std::pair<uint64_t, PendingEntry>> toSend;
            const auto now = std::chrono::steady_clock::now();
            {
                std::lock_guard lock(pendingMutex);
                for (auto it = pendingPackages.begin(); it != pendingPackages.end(); ++it)
                {
                    auto& id = it->first;
                    auto& entry = it->second;
                    if (entry.nextSend <= now)
                    {
                        toSend.emplace_back(id, entry);
                        entry.retries++;
                        entry.backoff = std::min(entry.backoff * 2, MAX_BACKOFF);
                        entry.nextSend = now + entry.backoff;
                    }
                }
            }

            for (auto& pair : toSend)
            {
                auto& entry = pair.second;
                size_t size;
                char* bytes = PackageProcessor::EncodePackage(entry.pkg, size);
                socket.Send(bytes, reinterpret_cast<const sockaddr*>(&entry.dest), size);
                std::free(bytes);
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
