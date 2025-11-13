#pragma once

#include "Socket.hpp"
#include "Models/Package.hpp"
#include "IdGenerator.hpp"

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <atomic>
#include <unordered_set>
#include <chrono>

#include "DeliveryTracker.hpp"
#include "ProcessLogger.hpp"

class PerfectLinks
{
    struct PendingEntry
    {
        Package pkg;
        sockaddr_in dest;
        std::chrono::steady_clock::time_point nextSend;
        std::chrono::milliseconds backoff{50};
        uint32_t retries{0};
    };

    uint32_t nodeId;
    IdGenerator idGenerator;

public:
    Socket& socket;
    ProcessLogger logger;

private:
    DeliveryTracker delivery_tracker;

    std::thread ackReceiverThread;
    std::atomic<bool> stopAckReceiver;
    bool startAckReceiver = false;

    std::thread resendThread;
    std::atomic<bool> stopResend = false;
    static constexpr std::chrono::milliseconds RESENDER_TICK{10};
    static constexpr std::chrono::milliseconds MAX_BACKOFF{1000};

    std::mutex pendingMutex;
    std::unordered_map<uint64_t, PendingEntry> pendingPackages;

    void send_in_chunks(const sockaddr_in& dest, std::vector<Package>& batch, bool writeLog = false);
    void ProcessPacket(Package& package);
    void AckReceiverLoop();
    void StartResender();

public:
    explicit PerfectLinks(Socket& _socket, uint32_t _nodeId, const std::string& path, bool startAck = false);
    ~PerfectLinks();

    void Send(in_addr_t ip, uint16_t port, int packageNum);
    void Receive();

    void StartAckReceiver();
    void StopAckReceiver();
    void StopResender();
};
