# Decentralized Systems Project

This repository contains my EPFL distributed algorithms project implementation in C++. The work is organized around the three project milestones rather than being flattened into a single final branch, because each milestone represents a distinct step in the design and a different algorithmic focus.

## Repository Structure

- `template_cpp/`: my C++ implementation, build scripts, and runnable binary layout
- `template_cpp/src/`: the core networking and algorithm code
- `example/`: sample hosts/config files for the project tasks
- `tools/`: helper scripts provided for running and stressing experiments

The main implementation work is in `template_cpp/src`. That is where I implemented the networking layer, broadcast logic, delivery tracking, serialization, and lattice agreement logic.

## Branches

This project is meant to be read branch-by-branch:

- `Milestone-1`: Perfect Links
- `Milestone-2`: Uniform Reliable Broadcast with FIFO ordering
- `Milestone-3`: Lattice Agreement

I am intentionally keeping these milestones on separate branches instead of merging them into a single linear codebase.

That separation is part of the story of the project:

- `Milestone-1` isolates the point-to-point reliability layer
- `Milestone-2` builds a broadcast stack on top of that transport layer
- `Milestone-3` focuses on lattice agreement, which uses reliable point-to-point communication but does not build on the URB/FIFO broadcast stack in the same way

Keeping the branches separate makes the progression easier to review and avoids presenting milestone 3 as if it were simply "milestone 2 plus more code." It was a different problem with a different protocol structure, so preserving that split is both technically cleaner and historically accurate.

## Milestone Progression

### 1. Perfect Links

The first milestone implements reliable point-to-point communication over UDP-style sockets. In this branch, the code focuses on:

- packet identifiers
- acknowledgments and retransmission
- duplicate suppression
- batched send/receive handling
- output logging in the format expected by the project

### 2. Uniform Reliable Broadcast with FIFO ordering

The second milestone extends the communication layer into a broadcast abstraction. This branch adds:

- rebroadcast-based reliable dissemination
- tracking of which processes have seen a message
- majority-based delivery conditions for uniform reliable broadcast
- per-sender FIFO delivery constraints

This is the branch where the broadcast stack is the main object of interest.

### 3. Lattice Agreement

The third milestone implements lattice agreement as a separate protocol branch. It reuses the lower-level transport ideas, but it is not modeled as a continuation of the URB/FIFO stack. Instead, it introduces:

- proposal rounds and proposal numbers
- ACK/NACK handling for proposals
- accepted-value growth through set union
- decision flushing in round order
- message encoding specific to lattice agreement payloads

That separation is intentional: lattice agreement solves a different coordination problem than FIFO uniform reliable broadcast, so keeping it in its own branch makes the repository easier to understand.

## What I Implemented

The core algorithmic code in `template_cpp/src` was implemented by me. That includes:

- the Perfect Links transport layer
- delivery tracking and message ID handling
- the FIFO / reliable broadcast logic on the milestone 2 branch
- the lattice agreement protocol on the milestone 3 branch
- packet/message serialization for the protocol messages
- logging/output behavior needed by the project

The repository also still contains the parser/build/run scaffolding, sample configs, and a couple of helper scripts from the project environment. I kept those because they make the implementation easier to run and inspect, but the main portfolio material is the C++ implementation.

## Building

The C++ code is set up through `template_cpp/`:

```bash
cd template_cpp
./build.sh
./run.sh --id <id> --hosts <hosts-file> --output <output-file> --config <config-file>
```

The exact config file depends on the branch and milestone. Sample configs are available under `example/configs/`.

## Reading Order

If you want to review the project as a portfolio piece, the best order is:

1. `Milestone-1` for the reliability foundation
2. `Milestone-2` for the broadcast layer built on top of it
3. `Milestone-3` for the separate lattice agreement implementation
