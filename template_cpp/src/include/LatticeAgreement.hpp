#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DestinationMap.hpp"
#include "PerfectLinks.hpp"
#include "Models/Node.hpp"

struct Proposal
{
    std::vector<uint32_t> values;
    uint32_t roundNumber;
    uint32_t proposalNumber;
};

struct Config
{
    uint32_t rounds;
    uint32_t vs;
    uint32_t ds;

    std::vector<std::unordered_set<uint32_t>> proposals;
};

struct ProposerState {
    uint32_t proposalNumber;
    uint32_t ackCount;
    uint32_t nackCount;
    bool canBeDecided;

    std::unordered_set<uint32_t> proposedValue;
};

class LatticeAgreement
{
private:
    PerfectLinks& _links;
    Config& _cfg;
    const std::vector<Node>& _nodes;
    DestinationMap _destinationMap;

    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> _acceptedValues;
    std::unordered_map<uint32_t, ProposerState> _proposerStates;
    std::unordered_map<uint32_t, std::vector<uint32_t>> _decidedValues;
    std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> _messageToProposal;

    uint32_t flushedRound = 0;


public:
    explicit LatticeAgreement(PerfectLinks& links, Config& cfg, const std::vector<Node>& nodes);
    ~LatticeAgreement() = default;

    // Acceptor
    void SelfAccept(const std::vector<uint32_t>& values, uint32_t roundNumber);
    Packet Accept(const std::vector<uint32_t>& values, uint32_t roundNumber, uint64_t originalPacketId);

    // Proposer
    void TryDecide(uint32_t roundNumber);
    void StartRound(uint32_t round, const std::vector<uint32_t>& values);
    void SendBatched(const std::vector<std::pair<uint32_t, const std::vector<uint32_t>*>>& items);
    void SendInitialProposals(const std::vector<std::vector<uint32_t>>& proposals);
    void SendProposalForRound(uint32_t round, const std::unordered_set<uint32_t>& set);
    void ProcessAck(uint32_t roundNumber, uint32_t proposalNumber);
    void ProcessNack(const Proposal& proposal);
    void MaybeRetry(uint32_t roundNumber);
    void RegisterProposalMessage(uint64_t messageId, uint32_t round, uint32_t proposalNumber);
    bool HandleAck(uint64_t messageId);
    bool HandleNack(uint64_t messageId, const std::vector<uint32_t>& values);

    void TryFlush();
    void Run();

    void StopReceiver() const;
    void StopResender() const;
};
