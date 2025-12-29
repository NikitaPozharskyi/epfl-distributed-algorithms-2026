#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "parser.hpp"
#include <signal.h>

#include "FIFOBroadcast.hpp"
#include "PackageProcessor.cpp"

#include "PerfectLinks.hpp"
#include "Socket.hpp"

struct Config
{
    uint32_t messageCount;
};

Config ParseConfigFile(const std::string& path);

std::unique_ptr<PerfectLinks> g_link;

static void stop(int)
{
    // reset signal handlers to default
    std::cerr << "[SIGTERM] Node  stopping.\n";

    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    if (g_link)
    {
        // think of the order.
        g_link->logger.FlushNow();
        g_link->socket.Stop();
        g_link->StopReceiver();
        g_link->StopResender();
    }

    // exit directly from signal handler
    exit(0);
}

int main(int argc, char** argv)
{
    signal(SIGTERM, stop);
    signal(SIGINT, stop);

    // `true` means that a config file is required.
    // Call with `false` if no config file is necessary.
    bool requireConfig = true;

    Parser parser(argc, argv);
    parser.parse();

    auto hosts = parser.hosts();

    unsigned long my_index = parser.id() - 1;
    const auto me = hosts[my_index];
    auto socket = Socket(me.ip, me.port);

    std::unique_ptr<FIFOBroadcast> fifoBroadcast;

    try
    {
        auto config = ParseConfigFile(parser.configPath());
        std::vector<Node> nodes;
        nodes.reserve(hosts.size());
        for (auto& host : hosts)
        {
            Node n = {static_cast<uint32_t>(host.id), host.ip, host.port};
            nodes.push_back(n);
        }

        fifoBroadcast = std::make_unique<FIFOBroadcast>(
            nodes,
            socket,
            parser.outputPath(),
            static_cast<uint32_t>(parser.id()));

        fifoBroadcast->Broadcast(config.messageCount);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return 1;
    }
    catch (int exceptionCode)
    {
        return exceptionCode;
    }

    // After a process finishes broadcasting,
    // it waits forever for the delivery of messages.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }

    return 0;
}

Config ParseConfigFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    Config cfg{};
    if (!(file >> cfg.messageCount))
    {
        throw std::runtime_error("Invalid file format: expected integer");
    }

    return cfg;
}
