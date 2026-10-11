# Design Log — Project 3

**Author:** William Zhou

**P2 code:** My own Conversation and SentinelScanner from Project 2.

**Tools used:** CMake and Clang were used to build and test; Git keeps the milestones.

## Bringing P2 along

I kept my Conversation and SentinelScanner as they were in P2. The new Message adds a tool role, but Conversation just stores messages, so it doesn't need to know what that role means. Its extra copying during growth is still there too.

## Hash table

I went with separate chaining: a vector of buckets, each holding a vector of entries. A lookup hashes the name and walks just that bucket. This felt easier to follow than keeping track of a linear probe, and a missing name simply reaches the end of its bucket.

The table starts with 16 buckets. If the next insertion would take the load factor above 1.0, it doubles first and puts every entry into its new bucket. A load factor of 1.0 seems reasonable for chaining without using too much empty space. Null pointers and duplicate names get rejected before resizing.

The hash is FNV-1a with the standard 64-bit starting value, 14695981039346656037. Each byte is XORed in, then multiplied by 1099511628211. Modulo happens separately when choosing a bucket, so hash() still returns the full value.

Doubling is the same basic idea as P2's growing array. The earlier table sizes form a geometric sum, so moving entries across all resizes takes O(n) work overall. That gives amortized O(1) insertion with a good hash and bounded name lengths. Longer names still take longer to hash.

The scale test still needs to check this choice against the weak hash.

## Keeping pointers valid

Each entry owns its tool through unique_ptr. Resizing moves the owner, not the actual tool, so pointers from find() still point to the same object. Growth counts the new bucket sizes and reserves space before moving entries. That's an extra pass, but allocation failures during preparation leave the old table alone. A rejected duplicate deletes only the incoming tool.
