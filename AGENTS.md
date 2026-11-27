# Rules

Read PROJECT_SCOPE.md. Use Nasdaq's primary ITCH/MoldUDP64 specifications. Keep wire claims precise and synthetic data clearly labeled.

Validate lengths and unsigned overflow before access or allocation. Commit each contiguous packet only after all its supported events validate. Reject forward gaps and session changes; never invent missing orders. Preserve sequence and state on rejection. Checkpoints are local integrity aids, not authenticated exchange snapshots. Bound input, active orders and capture size. Use tests against an independently implemented reference and sanitizers. Favor systems correctness over a frontend. Lowercase milestone commits; push each completed feature.
