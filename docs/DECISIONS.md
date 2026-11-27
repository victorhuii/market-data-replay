# Design choices

Market Data Replay — implementation rationale and tradeoffs.

| Choice | Why it fits this project | Cost or boundary |
| --- | --- | --- |
| Explicit supported-message subset | Keep binary contracts and failure behavior reviewable. | Arbitrary full feeds with unsupported types fail closed. |
| Touched-node rollback journal | Retain original map nodes until packet commit and restore them without allocation after failure. | Per-packet bookkeeping and active-locate scans still cost time. |
| Independent Python reference and frozen captures | Compare complete final orders and identities across implementations. | Synthetic captures do not establish live multicast correctness. |

## Evidence and scope

Independent book reference, sanitizer checks, and allocation-failure rollback evidence. No live feed, PCAP parser or retransmission requester; checksum is not authentication.

Ownership: **Original implementation**. Attribution and source notices remain in the [README](../README.md).

See [architecture](ARCHITECTURE.md) and [portfolio notes](PORTFOLIO.md) for implementation details and existing evidence. This documentation update does not add a new benchmark or training run.
