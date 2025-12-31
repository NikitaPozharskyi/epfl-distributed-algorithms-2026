#include "LatticeAgreement.hpp"

#include <unordered_set>


//helpers
bool isSubset(const std::unordered_set<uint32_t>& A,
              const std::unordered_set<uint32_t>& P)
{
    for (auto v : A)
        if (P.count(v) == 0)
            return false;
    return true;
}

void mergeInto(std::unordered_set<uint32_t>& A,
               const std::unordered_set<uint32_t>& P)
{
    for (auto v : P)
        A.insert(v);
}

std::unordered_set<uint32_t> ToSet(const std::vector<uint32_t>& values)
{
    std::unordered_set<uint32_t> out;
    out.reserve(values.size());
    out.insert(values.begin(), values.end());
    return out;
}

std::vector<uint32_t> ToVector(const std::unordered_set<uint32_t>& values)
{
    std::vector<uint32_t> out(values.begin(), values.end());
    return out;
}

Packet BuildAckPacket(const uint64_t id, const uint32_t nodeId)
{
    Packet ackPkt;
    ackPkt.header = Header(
        id,
        acknowledgment,
        nodeId
    );

    ackPkt.body = std::make_unique<Acknowledgment>();

    return ackPkt;
}

Packet BuildNackPacket(const uint64_t id, const uint32_t nodeId, const std::vector<uint32_t>& accepted)
{
    Packet nackPkt;
    nackPkt.header = Header(
        id,
        notAcknowledgment,
        nodeId
    );

    auto body = std::make_unique<NotAcknowledgment>();
    body->values = accepted;
    nackPkt.body = std::move(body);

    return nackPkt;
}


// acceptor
Packet LatticeAgreement::Accept(Proposal proposal, Packet pkt)
{
    auto& acceptedValues = _acceptedValues[proposal.roundNumber];
    auto proposedSet = ToSet(proposal.proposalSet);

    if (isSubset(acceptedValues, proposedSet))
    {
        acceptedValues = proposedSet;
        return BuildAckPacket(pkt.header.id, _links.nodeId);
    }

    mergeInto(acceptedValues, proposedSet);

    auto acceptedValueVector = ToVector(acceptedValues);
    return BuildNackPacket(pkt.header.id, _links.nodeId, acceptedValueVector);
}

void LatticeAgreement::Propose(Proposal)
{
}