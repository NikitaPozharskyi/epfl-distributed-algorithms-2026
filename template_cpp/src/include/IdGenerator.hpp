#pragma once
#include <atomic>
#include <cstdint>

class IdGenerator
{
    std::atomic<uint32_t> counter{1};
    std::uint32_t nodeId;

public:
    IdGenerator(uint32_t _nodeId);
    uint64_t next();
    static uint32_t getNodeId(uint64_t id);
    static uint32_t getCounter(uint64_t id);
    static uint64_t make(uint32_t id, uint32_t ctr);
};
