# Market Data Replay

Original C++20 systems project: deterministic, bounded reconstruction of visible orders from a documented subset of Nasdaq ITCH 5.0 carried in MoldUDP64 datagrams.

Deliver strict binary bounds, session/sequence integrity, overlapping retransmission handling, transactional packet application, validated restart checkpoints, an independent Python reference, adversarial regression tests, sanitizer CI, and transparent repeated benchmarks. Use owned synthetic fixtures, not licensed market capture data.

Supported ITCH: A/F add, E/C execute, X cancel, D delete, U replace, S system event. Unsupported message types fail closed. This subset cannot consume an arbitrary full production feed. No UDP socket, multicast subscription, recovery requester, PCAP decoder, live exchange connectivity, trading or licensed data is included.

Capture format is project-specific: a four-byte big-endian datagram length followed by each MoldUDP64 payload. Session starts from an empty order map at sequence 1, or from an explicitly loaded checkpoint. An input gap never advances trusted book state. Recovery requires supplying the missing contiguous messages and replaying the suffix; the project does not invent them.

Prices are integer 1/10,000 units and quantities integer shares. Reconstruct visible quantities; do not match orders or reject crossed books (auctions can cross). No trade-tape/hidden-liquidity/queue-position claims. Finite state limits and per-packet book copying are deliberate correctness choices, with measured costs disclosed.
