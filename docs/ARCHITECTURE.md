# Architecture

Bounded ITCH-subset decoding and MoldUDP64 replay with sequence checks, overlapping retransmissions, and atomic packet recovery.

## Data flow

```mermaid
flowchart TD
    n0["Length-prefixed owned capture"]
    n1["MoldUDP64 bounds and sequence validation"]
    n2["Supported ITCH event decoding"]
    n3["Touched-node packet transaction"]
    n4["Canonical checkpoint and reference audit"]
    n0 --> n1
    n1 --> n2
    n2 --> n3
    n3 --> n4
```

## Source map

- [src/replay.hpp](../src/replay.hpp)
- [src/main.cpp](../src/main.cpp)
- [tests/reference.py](../tests/reference.py)
- [tests/allocation.cpp](../tests/allocation.cpp)
- [scripts/benchmark.py](../scripts/benchmark.py)

## Evidence and boundaries

Independent book reference, sanitizer checks, and allocation-failure rollback evidence. No live feed, PCAP parser or retransmission requester; checksum is not authentication.

See [setup and recorded results](../README.md), [design choices](DECISIONS.md), and [portfolio notes](PORTFOLIO.md).
