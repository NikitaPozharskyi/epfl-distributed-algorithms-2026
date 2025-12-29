#pragma once

#include "Socket.hpp"
#include "Models/Packet.hpp"
#include "IdGenerator.hpp"

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <functional>

#include "IDeliveryStrategy.hpp"
#include "ProcessLogger.hpp"
#include "Models/PendingEntry.hpp"

using UpperDeliverFn = std::function<void(std::vector<Packet>&)>;

struct PendingKey
{
    uint64_t id;
    uint32_t destNodeId;
};

struct PendingKeyHash
{
    size_t operator()(const PendingKey& k) const noexcept
    {
        return std::hash<uint64_t>()(k.id)
            ^ (std::hash<uint32_t>()(k.destNodeId) << 1);
    }
};

struct PendingKeyEq
{
    bool operator()(const PendingKey& a, const PendingKey& b) const noexcept
    {
        return a.id == b.id &&
            a.destNodeId == b.destNodeId;
    }
};

class PerfectLinks : public IDeliveryStrategy
{
    IdGenerator idGenerator;

public:
    uint32_t nodeId;
    Socket& socket;
    ProcessLogger logger;

private:
    std::thread receiverThread;
    std::atomic<bool> stopReceiver;

    std::thread resendThread;
    std::atomic<bool> stopResend = false;
    static constexpr std::chrono::milliseconds RESENDER_TICK{10};

    UpperDeliverFn _fifoPacketProcessFunc;
    std::mutex pendingMutex;
    std::unordered_map<PendingKey, PendingEntry, PendingKeyHash, PendingKeyEq> pendingPackages;

    void send_in_chunks(const sockaddr_in& dest, std::vector<Packet>& batch, bool writeLog = false);

    void DeliverPackets(std::vector<Packet>& packets, sockaddr_in sender_addr);
    void ProcessPacket(const Packet& pkt);
    void ReceiverLoop();

public:
    static constexpr std::chrono::milliseconds MAX_BACKOFF{1000};
    uint64_t GetNextId();

    // IDeliveryStrategy
    std::vector<PendingEntry> get_packets_to_send() override;
    std::pair<std::vector<Packet>, std::vector<Packet>> process(std::vector<Packet>& packets) override;

    explicit PerfectLinks(Socket& _socket, uint32_t _nodeId, const std::string& path);
    ~PerfectLinks();

    void AddPending(const Packet& pkt, const sockaddr_in& dest, uint32_t destNodeId);
    void Receive();

    void SendNoWrite(const sockaddr_in& dest, std::vector<Packet>& batch);

    void SendMessageInChunksNoWrite(const sockaddr_in& dest, std::vector<Packet>& batch, const uint32_t destNodeID);
    void SendMessageInChunks(const sockaddr_in& dest, std::vector<Packet>& batch, uint32_t destNodeID);

    std::vector<Packet> ReceivePackets(sockaddr_in& sender_addr) const;

    void StartReceiver(UpperDeliverFn fn);
    void StopReceiver();

    void StartResender();
    void StopResender();
};
