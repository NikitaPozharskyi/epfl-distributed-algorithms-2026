#include "IdGenerator.hpp"
#include <chrono>

IdGenerator::IdGenerator(uint32_t _nodeId)
    : nodeId(_nodeId)
{
}

uint32_t IdGenerator::getNodeId(uint64_t id)
{
    return static_cast<uint32_t>(id >> 32);
}

uint32_t IdGenerator::getCounter(uint64_t id)
{
    return static_cast<uint32_t>(id & 0xFFFFFFFF);
}

uint64_t IdGenerator::next()
{
    const uint32_t count = counter.fetch_add(1, std::memory_order_relaxed);
    return static_cast<uint64_t>(nodeId) << 32 | count;
}

uint64_t IdGenerator::make(const uint32_t id, const uint32_t ctr)
{
    return (static_cast<uint64_t>(id) << 32) | ctr;
}
