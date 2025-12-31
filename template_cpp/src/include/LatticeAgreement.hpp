#pragma once
#include <unordered_set>

#include "PerfectLinks.hpp"
#include "Models/Node.hpp"


struct Proposal;
struct ProposerState;
struct Config;

class LatticeAgreement
{
private:
    PerfectLinks& _links;
    Config& _cfg;
    // map represents accepted value mapped to round number (shot)
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> _acceptedValues;
    std::vector<Node> _nodes;

public:
    explicit LatticeAgreement(PerfectLinks& links, Config& cfg);
    ~LatticeAgreement();

    // Acceptor
    Packet Accept(Proposal proposal, Packet pkt);

    // Proposer
    void Propose(const std::vector<Proposal>& proposals) const;
    void ProcessAck(Proposal proposal);
    void ProcessNack(Proposal proposal);

    void Run();

    void StartReceiver();
    void StartResender();
};

struct Proposal
{
    std::vector<uint32_t> proposalSet;
    uint32_t roundNumber;
    uint32_t proposalNumber;
};

struct ProposerState {
    uint32_t proposalNumber;
    uint32_t ackCount;
    uint32_t nackCount;
    std::unordered_set<uint32_t> proposedValue;
};

struct Config
{
    uint32_t rounds;
    uint32_t vs;
    uint32_t ds;

    std::vector<std::unordered_set<uint32_t>> proposals;
};