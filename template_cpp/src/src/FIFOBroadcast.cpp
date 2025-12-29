#include "FIFOBroadcast.hpp"

FIFOBroadcast::FIFOBroadcast(
    const std::vector<Node>& nodes,
    Socket& socket,
    const std::string& outputPath,
    const uint32_t nodeID
)
    : _tracker(static_cast<uint32_t>(nodes.size())),
      _nodes(nodes),
      _destinationMap(nodes),
      _links(socket, nodeID, outputPath)
{
}

void FIFOBroadcast::StopResender()
{
    _links.StopResender();
}

void FIFOBroadcast::StopReceiver()
{
    _links.StopReceiver();
}

void FIFOBroadcast::Broadcast(uint32_t amount)
{
    auto process = [this](std::vector<Packet>& pkts)
    {
        this->OnPacketsFromPL(pkts);
    };

    _links.StartReceiver(process);
    _links.StartResender();

    std::vector<Packet> packets;
    packets.reserve(amount);

    for (uint32_t i = 0; i < amount; ++i)
    {
        Packet pkt;
        pkt.header = Header(
            _links.GetNextId(),
            regular,
            _links.nodeId
        );
        pkt.body = std::make_unique<Regular>();

        packets.push_back(pkt);

        const uint32_t sender = IdGenerator::getNodeId(pkt.header.id);
        const uint32_t seq = IdGenerator::getCounter(pkt.header.id);

        _tracker.markSeen(sender, seq, _links.nodeId);
    }

    bool write = true;
    for (const auto& node : _nodes)
    {
        if (node.id == _links.nodeId)
        {
            continue;
        }

        auto dest = _destinationMap.GetDestinationByID(node.id);
        if (write)
        {
            _links.SendMessageInChunks(dest, packets, node.id);
            write = false;
        }

        _links.SendMessageInChunksNoWrite(dest, packets, node.id);
    }
}

void FIFOBroadcast::OnPacketsFromPL(std::vector<Packet>& packets)
{
    std::vector<Packet> packetsToRebroadcast;
    packetsToRebroadcast.reserve(packets.size());

    std::vector<Packet> toDeliver;
    toDeliver.reserve(packets.size());

    for (auto& pkt : packets)
    {
        const uint64_t msgId = pkt.header.id;
        const uint32_t sender = IdGenerator::getNodeId(msgId);
        uint32_t seq = IdGenerator::getCounter(msgId);
        const uint32_t fromProc = pkt.header.forwardedBy;

        if (_tracker.markSeenAndCheckFirst(sender, seq, fromProc, pkt))
        {
            pkt.header.forwardedBy = _links.nodeId;
            packetsToRebroadcast.push_back(pkt);
        }
    }

    for (uint32_t s = 1; s <= _nodes.size(); s++)
    {
        uint32_t next = _tracker.lastDeliveredOf(s) + 1;

        while (_tracker.readyToDeliver(s, next))
        {
            Packet& pkt = _tracker.getPacket(s, next);
            toDeliver.push_back(pkt);

            _tracker.markDelivered(s, next);
            _tracker.removePacket(s, next);
            next++;
        }
    }

    _links.logger.LogReceived(toDeliver);
    Rebroadcast(packetsToRebroadcast);
}

void FIFOBroadcast::Rebroadcast(std::vector<Packet> packets)
{
    if (packets.empty())
    {
        return;
    }

    for (auto& node : _nodes)
    {
        if (node.id == _links.nodeId)
            continue;

        auto dest = _destinationMap.GetDestinationByID(node.id);
        _links.SendMessageInChunksNoWrite(dest, packets, node.id);
    }
}
