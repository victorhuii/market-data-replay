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

## Touched-node journal comparison

The same capture hashes and final state checksums were verified across both releases. Each row uses three fresh processes per implementation; measurements were sequential, not randomized/interleaved, so machine load and cache state remain possible confounders.

| Depth | Messages/packet | Copy median/s | Journal median/s | Ratio |
| --- | ---: | ---: | ---: | ---: |
| 100 | 1 | 121,912 | 1,136,905 | 9.33× |
| 100 | 32 | 1,733,954 | 1,971,414 | 1.14× |
| 3000 | 1 | 4,920 | 173,313 | 35.23× |
| 3000 | 32 | 99,709 | 202,462 | 2.03× |

[Raw journal samples](benchmark.json) include the measured source identity. Both implementations retain identical event semantics, sequence metadata, and final active orders. Ratios apply to these finite synthetic workloads only. Deep books still incur locate-consistency scan costs.

## Reproduce

```sh
make
python3 scripts/benchmark.py
```

This overwrites `docs/benchmark.json` with your machine’s measured evidence. Raw captures stay in ignored `runtime/`. The old copy implementation is available through `-DREPLAY_COPY_ATOMICITY`; compile it to a separate binary when comparing so build flags do not silently reuse a stale executable. No network or paid data is required.
