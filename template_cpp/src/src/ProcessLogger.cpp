#include "../include/ProcessLogger.hpp"
#include "IdGenerator.hpp"
#include <iostream>
#include <sstream>

ProcessLogger::ProcessLogger(const std::string& path)
{
    logFile.open(path, std::ios::out | std::ios::trunc);
    if (!logFile.is_open())
        throw std::runtime_error("Failed to open log file: " + path);

    flusherThread = std::thread(&ProcessLogger::FlusherLoop, this);
}

ProcessLogger::~ProcessLogger()
{
    stopFlusher = true;
    if (flusherThread.joinable())
        flusherThread.join();

    FlushInternal();
    if (logFile.is_open())
        logFile.close();
}

void ProcessLogger::LogSent(const std::vector<Package>& outs)
{
    std::lock_guard lock(bufferMutex);
    for (const auto& pkg : outs)
    {
        if (pkg.header.type != regular) continue;
        if (auto* msg = dynamic_cast<Regular*>(pkg.body.get()))
        {
            std::ostringstream oss;
            oss << "b " << msg->number << "\n";
            buffer.emplace_back(oss.str());
        }
    }
}

void ProcessLogger::LogSentRange(const Package* pkts, size_t count)
{
    if (!pkts || count == 0) return;
    std::lock_guard lock(bufferMutex);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& pkg = pkts[i];
        if (pkg.header.type != regular) continue;
        if (auto* msg = dynamic_cast<Regular*>(pkg.body.get()))
        {
            std::ostringstream oss;
            oss << "b " << msg->number << "\n";
            buffer.emplace_back(oss.str());
        }
    }
}

void ProcessLogger::LogReceived(const std::vector<Package>& ins)
{
    std::lock_guard lock(bufferMutex);
    for (const auto& pkg : ins)
    {
        if (pkg.header.type != regular) continue;
        if (auto* msg = dynamic_cast<Regular*>(pkg.body.get()))
        {
            std::ostringstream oss;
            oss << "d " << IdGenerator::getNodeId(pkg.header.id)
                << " " << msg->number << "\n";
            buffer.emplace_back(oss.str());
        }
    }
}

void ProcessLogger::FlusherLoop()
{
    using namespace std::chrono_literals;
    while (!stopFlusher)
    {
        std::this_thread::sleep_for(500ms);
        FlushInternal();
    }

    FlushInternal();
}

void ProcessLogger::FlushInternal()
{
    std::vector<std::string> local;
    {
        std::lock_guard lock(bufferMutex);
        if (buffer.empty()) return;
        local.swap(buffer);
    }

    for (const auto& line : local)
        logFile << line;
    logFile.flush();
}

void ProcessLogger::FlushNow()
{
    FlushInternal();
}
