# Packet transactions

The first version copied the complete order map before applying a packet. It made the commit boundary easy to review but imposed an O(active orders) copy cost even for a one-message packet. The repeated baseline exposed this cost, so the new default uses a touched-node journal.

For every reference ID a new message can modify, the journal records its original state exactly once. An existing map node is retained as a node handle and a working copy is put back into the map. A previously absent ID is recorded as absent. Replacement touches both the original and replacement reference IDs. Repeated modifications, deletion, and reuse of an ID in the same packet therefore preserve the pre-packet state.

All storage for the working copy is allocated before extracting the original node. The journal reserves its entry capacity before applying events. If any validation or allocation fails, it erases the touched working entries, reinserts the retained original nodes without allocating new map nodes, and restores timestamp metadata. Session/sequence metadata is committed only after all events succeed. Unsigned key comparison and node-handle reinsertion do not allocate. Successful commit destroys the old nodes when the journal leaves scope.

The regression harness injects a failure at each allocation in a valid packet that replaces, reduces, deletes and reuses references. On the tested build, 27 successive injected failure points preserved the exact canonical pre-packet state; the next successful run matched the independently computed committed state. These are controlled allocator failures, not process termination or machine power-loss tests.

The optional `-DREPLAY_COPY_ATOMICITY` build retains the copy implementation as a readable reference. Deterministic stream tests also compare order-by-order output against a separate Python decoder/book. The journal is single-threaded; no concurrent reader or lock-free claim is made.

Known remaining costs include one working node copy per touched existing order, a hash set/journal allocation per packet, and an O(active orders) locate-consistency scan on adds. No persistent checkpoint fsync or crash-durability guarantee is claimed. The CLI writes a temporary checkpoint then renames it; a rejected capture never replaces the requested checkpoint.
