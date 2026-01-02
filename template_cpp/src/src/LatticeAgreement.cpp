#include "LatticeAgreement.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_set>


namespace
{
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
        std::sort(out.begin(), out.end());
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
} // namespace

LatticeAgreement::LatticeAgreement(PerfectLinks& links, Config& cfg, const std::vector<Node>& nodes)
    : _links(links), _cfg(cfg), _nodes(nodes), _destinationMap(nodes)
{
}

void LatticeAgreement::SelfAccept(const std::vector<uint32_t>& values, const uint32_t roundNumber)
{
    auto& acceptedValues = _acceptedValues[roundNumber];
    mergeInto(acceptedValues, ToSet(values));
}

// acceptor
Packet LatticeAgreement::Accept(const std::vector<uint32_t>& values, const uint32_t roundNumber,
                                const uint64_t originalPacketId)
{
    auto& acceptedValues = _acceptedValues[roundNumber];
    const auto proposedSet = ToSet(values);

    if (isSubset(acceptedValues, proposedSet))
    {
        mergeInto(acceptedValues, proposedSet);
        return BuildAckPacket(originalPacketId, _links.nodeId);
    }

    mergeInto(acceptedValues, proposedSet);

    const auto acceptedValueVector = ToVector(acceptedValues);
    return BuildNackPacket(originalPacketId, _links.nodeId, acceptedValueVector);
}

void LatticeAgreement::StartRound(const uint32_t round, const std::vector<uint32_t>& values)
{
    auto& state = _proposerStates[round];
    const auto proposalsSet = ToSet(values);

    state.proposalNumber = 1;
    state.proposedValue = proposalsSet;
    state.ackCount = 1; // self-ack
    state.nackCount = 0;
    state.canBeDecided = false;

    _acceptedValues[round] = proposalsSet;
    TryDecide(round);
}

void LatticeAgreement::SendBatched(const std::vector<std::pair<uint32_t, const std::vector<uint32_t>*>>& items)
{
    constexpr std::size_t BATCH_SIZE = 8;

    if (_nodes.empty() || items.empty())
        return;

    const std::size_t total = items.size();

    for (std::size_t base = 0; base < total; base += BATCH_SIZE)
    {
        const std::size_t end = std::min(base + BATCH_SIZE, total);

        for (const auto& node : _nodes)
        {
            if (node.id == _links.nodeId)
                continue;

            std::vector<Packet> packets;
            packets.reserve(end - base);

            for (std::size_t i = base; i < end; ++i)
            {
                const auto& [round, valuesPtr] = items[i];

                Packet pkt;
                const uint64_t messageId = _links.GetNextId();
                pkt.header = Header(
                    messageId,
                    latticeAgreement,
                    _links.nodeId
                );

                auto body = std::make_unique<LatticeAgreementMessage>();
                body->roundNumber = round;
                body->proposalNumber = _proposerStates[round].proposalNumber;
                body->values = *valuesPtr;
                pkt.body = std::move(body);

                RegisterProposalMessage(messageId, round, _proposerStates[round].proposalNumber);

                packets.push_back(std::move(pkt));
            }

            auto dest = _destinationMap.GetDestinationByID(node.id);
            _links.SendMessageInChunksNoWrite(dest, packets, node.id);
        }
    }
}

void LatticeAgreement::SendInitialProposals(const std::vector<std::vector<uint32_t>>& proposals)
{
    std::vector<std::pair<uint32_t, const std::vector<uint32_t>*>> items;
    items.reserve(proposals.size());

    for (uint32_t r = 0; r < proposals.size(); ++r)
    {
        items.emplace_back(r, &proposals[r]);
    }

    SendBatched(items);
}

void LatticeAgreement::SendProposalForRound(uint32_t round, const std::unordered_set<uint32_t>& set)
{
    auto vec = ToVector(set);
    std::vector<std::pair<uint32_t, const std::vector<uint32_t>*>> items;
    items.emplace_back(round, &vec);
    SendBatched(items);
}

void LatticeAgreement::ProcessAck(const uint32_t roundNumber,
                                  const uint32_t proposalNumber)
{
    auto& state = _proposerStates[roundNumber];

    if (state.proposalNumber != proposalNumber)
        return;

    state.ackCount++;

    TryDecide(roundNumber);
    MaybeRetry(roundNumber);
}


void LatticeAgreement::TryDecide(const uint32_t roundNumber)
{
    auto& state = _proposerStates[roundNumber];
    if (state.canBeDecided)
    {
        return;
    }

    const uint32_t quorum = static_cast<uint32_t>(_nodes.size() / 2 + 1);

    if (state.ackCount >= quorum && state.nackCount == 0)
    {
        auto decided = ToVector(state.proposedValue);
        _decidedValues.emplace(roundNumber, std::move(decided));
        state.canBeDecided = true;
        TryFlush();
    }
}

void LatticeAgreement::MaybeRetry(const uint32_t roundNumber)
{
    auto& state = _proposerStates[roundNumber];
    if (state.canBeDecided)
    {
        return;
    }

    const uint32_t quorum = static_cast<uint32_t>(_nodes.size() / 2 + 1);

    if (state.nackCount > 0 &&
        state.ackCount + state.nackCount >= quorum)
    {
        state.proposalNumber++;
        state.ackCount = 1; // self-ack for new proposal
        state.nackCount = 0;

        SendProposalForRound(roundNumber, state.proposedValue);
    }
}

void LatticeAgreement::TryFlush()
{
    while (true)
    {
        auto it = _decidedValues.find(flushedRound);
        if (it == _decidedValues.end())
            break;
        _links.logger.LogLatticeAgreement(it->second);
        _decidedValues.erase(it);
        flushedRound++;
    }
}

void LatticeAgreement::RegisterProposalMessage(const uint64_t messageId, const uint32_t round,
                                               const uint32_t proposalNumber)
{
    _messageToProposal[messageId] = {round, proposalNumber};
}

bool LatticeAgreement::HandleAck(const uint64_t messageId)
{
    auto it = _messageToProposal.find(messageId);
    if (it == _messageToProposal.end())
        return false;

    const auto [round, proposalNumber] = it->second;
    ProcessAck(round, proposalNumber);
    _messageToProposal.erase(it);
    return true;
}

bool LatticeAgreement::HandleNack(const uint64_t messageId, const std::vector<uint32_t>& values)
{
    auto it = _messageToProposal.find(messageId);
    if (it == _messageToProposal.end())
        return false;

    const auto [round, proposalNumber] = it->second;
    Proposal proposal{values, round, proposalNumber};
    ProcessNack(proposal);
    _messageToProposal.erase(it);
    return true;
}

void LatticeAgreement::ProcessNack(const Proposal& proposal)
{
    auto& state = _proposerStates[proposal.roundNumber];

    if (state.canBeDecided)
        return;

    if (state.proposalNumber != proposal.proposalNumber)
        return;

    state.nackCount++;
    mergeInto(state.proposedValue, ToSet(proposal.values));

    MaybeRetry(proposal.roundNumber);
}

void LatticeAgreement::StopReceiver() const
{
    _links.StopReceiver();
}

void LatticeAgreement::StopResender() const
{
    _links.StopResender();
}

void LatticeAgreement::Run()
{
    _links.StartResender();
    _links.StartReceiver([this](std::vector<Packet>& packets, sockaddr_in& sender_addr)
    {
        std::vector<Packet> out;
        out.reserve(packets.size());
        for (auto& pkt : packets)
        {
            switch (pkt.header.type)
            {
            case latticeAgreement:
                {
                    auto* msg = dynamic_cast<LatticeAgreementMessage*>(pkt.body.get());
                    if (!msg) break;
                    Packet resp = Accept(msg->values, msg->roundNumber, pkt.header.id);
                    out.push_back(std::move(resp));
                    break;
                }
            case acknowledgment:
                HandleAck(pkt.header.id);
                break;
            case notAcknowledgment:
                {
                    auto* nack = dynamic_cast<NotAcknowledgment*>(pkt.body.get());
                    if (!nack) break;
                    HandleNack(pkt.header.id, nack->values);
                    break;
                }
            default:
                break;
            }
        }

        _links.SendNoWrite(sender_addr, out);
    });

    std::vector<std::vector<uint32_t>> proposals;
    proposals.reserve(_cfg.proposals.size());
    for (uint32_t r = 0; r < _cfg.proposals.size(); ++r)
    {
        std::vector<uint32_t> vals(_cfg.proposals[r].begin(), _cfg.proposals[r].end());
        std::sort(vals.begin(), vals.end());
        StartRound(r, vals);
        proposals.push_back(std::move(vals));
    }

    SendInitialProposals(proposals);
}
