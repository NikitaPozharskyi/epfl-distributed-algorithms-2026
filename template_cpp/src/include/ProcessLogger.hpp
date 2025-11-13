#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#include "Models/Package.hpp"

class ProcessLogger
{
public:
    explicit ProcessLogger(const std::string& path);
    ~ProcessLogger();

    void LogSent(const std::vector<Package>& outs);
    void LogSentRange(const Package* pkts, size_t count);
    void LogReceived(const std::vector<Package>& ins);

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
