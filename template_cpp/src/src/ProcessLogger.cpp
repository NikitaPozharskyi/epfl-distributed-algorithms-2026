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

void ProcessLogger::LogSent(const std::vector<Packet>& outs)
{
    std::lock_guard lock(bufferMutex);
    for (const auto& pkg : outs)
    {
        if (pkg.header.type != regular) continue;
        std::ostringstream oss;
        oss << "b " << IdGenerator::getCounter(pkg.header.id) << "\n";
        buffer.emplace_back(oss.str());
    }
}

void ProcessLogger::LogLatticeAgreement(const std::vector<uint32_t>& values)
{
    std::lock_guard lock(bufferMutex);

    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i)
    {
        oss << values[i];
        if (i + 1 < values.size())
            oss << " ";
    }
    oss << "\n";

    buffer.emplace_back(oss.str());
}


void ProcessLogger::LogSentRange(const Packet* pkts, size_t count)
{
    if (!pkts || count == 0) return;
    std::lock_guard lock(bufferMutex);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& pkg = pkts[i];
        if (pkg.header.type != regular) continue;
        std::ostringstream oss;
        oss << "b " << IdGenerator::getCounter(pkg.header.id) << "\n";
        buffer.emplace_back(oss.str());
    }
}

void ProcessLogger::LogReceived(const std::vector<Packet>& ins)
{
    std::lock_guard lock(bufferMutex);
    for (const auto& pkg : ins)
    {
        if (pkg.header.type != regular) continue;
        std::ostringstream oss;
        oss << "d " << IdGenerator::getNodeId(pkg.header.id)
            << " " << IdGenerator::getCounter(pkg.header.id) << "\n";
        buffer.emplace_back(oss.str());
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
