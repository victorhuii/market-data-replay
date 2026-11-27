# Replay measurements

Owned synthetic ITCH subset messages, 20,000 events, three fresh processes per profile. Capture read/parse, wire/semantic validation, and atomic packet application are timed. Fixture generation, output and checkpoint serialization are excluded. No network, licensed feed, CPU affinity or cold-cache controls. These are local comparative measurements, not production throughput claims.

The copy-based reference release showed a clear packet-rate/depth tradeoff:

| Target depth | Maximum messages per packet | Median messages/s |
| --- | ---: | ---: |
| 100 | 1 | 121,912 |
| 100 | 32 | 1,733,954 |
| 3,000 | 1 | 4,920 |
| 3,000 | 32 | 99,709 |

[Raw baseline samples and machine/compiler/source identities](benchmark-copy-baseline.json). Every profile was compared against the independent Python book, and both packetizations had identical final state checksums at the same depth.

The next controlled implementation experiment replaces whole-book packet copies with a rollback journal of touched map nodes. Keep input identities and financial/event semantics unchanged; compare new measurements and retain this baseline even if the new implementation disappoints. This is a systems experiment, not tuning a trading strategy.
