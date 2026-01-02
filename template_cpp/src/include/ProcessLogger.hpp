#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#include "Models/Packet.hpp"

class ProcessLogger
{
public:
    explicit ProcessLogger(const std::string& path);
    ~ProcessLogger();

    void LogSent(const std::vector<Packet>& outs);
    void LogLatticeAgreement(const std::vector<uint32_t>& values);
    void LogSentRange(const Packet* pkts, size_t count);
    void LogReceived(const std::vector<Packet>& ins);

    void FlushNow();

private:
    std::ofstream logFile;
    std::mutex bufferMutex;
    std::vector<std::string> buffer;
    std::atomic<bool> stopFlusher{false};
    std::thread flusherThread;

    void FlusherLoop();
    void FlushInternal();
};
