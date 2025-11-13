#include <chrono>
#include <iostream>
#include <thread>

#include "parser.hpp"
#include <signal.h>
#include "PackageProcessor.cpp"

#include "PerfectLinks.hpp"
#include "Socket.hpp"

struct Config
{
    int messageCount;
    unsigned long processId;
};

Config ParseConfigFile(const std::string& path);

std::unique_ptr<PerfectLinks> g_link;

static void stop(int)
{
    // reset signal handlers to default
    std::cerr << "[SIGTERM] Node  stopping.\n";
    std::cout << "[SIGTERM] Node  stopping.\n";

    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);

    // immediately stop network packet processing
    std::cout << "Immediately stopping network packet processing.\n";

    if (g_link)
    {
        // think of the order.
        g_link->logger.FlushNow();
        g_link->socket.Stop();
        g_link->StopAckReceiver();
        g_link->StopResender();
    }

    // write/flush output file if necessary
    std::cout << "Writing output.\n";

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

    std::cout << std::endl;

    std::cout << "My PID: " << getpid() << "\n";
    std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
        << getpid() << "` to stop processing packets\n\n";

    std::cout << "My ID: " << parser.id() << "\n\n";

    std::cout << "List of resolved hosts is:\n";
    std::cout << "==========================\n";
    auto hosts = parser.hosts();

    for (auto& host : hosts)
    {
        std::cout << host.id << "\n";
        std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
        std::cout << "Machine-readable IP: " << host.ip << "\n";
        std::cout << "Human-readabale Port: " << host.portReadable() << "\n";
        std::cout << "Machine-readabale Port: " << host.port << "\n";
        std::cout << "\n";
    }


    unsigned long my_index = parser.id() - 1;
    const auto me = hosts[my_index];
    std::cout << "My IP: " << me.ipReadable() << "\n";
    std::cout << "My Port: " << me.portReadable() << "\n";
    auto socket = Socket(me.ip, me.port);

    try
    {
        auto config = ParseConfigFile(parser.configPath());
        if (config.processId == parser.id())
        {
            g_link = std::make_unique<PerfectLinks>(socket, static_cast<uint32_t>(parser.id()), parser.outputPath(),
                                                    false);
            g_link->Receive();
        }
        else
        {
            g_link = std::make_unique<PerfectLinks>(socket, static_cast<uint32_t>(parser.id()), parser.outputPath(),
                                                    true);
            g_link->Send(hosts[config.processId - 1].ip, hosts[config.processId - 1].port, config.messageCount);
        }
    }
    catch (int exceptionCode)
    {
        std::cout << "Exception: " << exceptionCode << "\n";
    }

    std::cout << "\n";

    std::cout << "Path to output:\n";
    std::cout << "===============\n";
    std::cout << parser.outputPath() << "\n\n";

    std::cout << "Path to config:\n";
    std::cout << "===============\n";
    std::cout << parser.configPath() << "\n\n";

    std::cout << "Doing some initialization...\n\n";

    std::cout << "Broadcasting and delivering messages...\n\n";

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
    if (!(file >> cfg.messageCount >> cfg.processId))
    {
        throw std::runtime_error("Invalid file format: expected two integers separated by space");
    }

    return cfg;
}
