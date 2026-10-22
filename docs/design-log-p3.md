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

The 10,000-tool test gave these numbers at 16,384 buckets and a load factor of 0.61035156:

| Hash | Longest chain |
|---|---:|
| FNV-1a | 6 |
| First character plus length | 9,000 |

FNV-1a produced 10,000 distinct full hashes too. The weak hash groups all four-digit suffixes together. Adding buckets doesn't help when the hash keeps giving those names the same value.

## Keeping pointers valid

Each entry owns its tool through unique_ptr. Resizing moves the owner, not the actual tool, so pointers from find() still point to the same object. Growth counts the new bucket sizes and reserves space before moving entries. That's an extra pass, but allocation failures during preparation leave the old table alone. A rejected duplicate deletes only the incoming tool.

## Workspace paths

The path check follows the spec's order: reject empty strings, NUL bytes, and rooted paths, then normalize. Only a leading `..` after normalization means an escape. Looking for `..` anywhere would wrongly reject `my..notes.txt`. This function only checks text; the provided symlink check handles actual links before file access.

## Calculator trace

The calculator uses two vectors as stacks. For `2 * (3 + 4) - 5`, reading `2` leaves values `[2]`. Reading `*`, then `(`, leaves operators `[*, (]`. Reading `3` gives values `[2, 3]`; `+` makes operators `[*, (, +]`. Reading `4` gives `[2, 3, 4]`.

At `)`, I pop 4 as the right operand and 3 as the left, add them, and remove `(`. Now values are `[2, 7]` and operators are `[*]`. When `-` arrives, multiplication runs first, leaving `[14]`, then minus is pushed. Reading `5` gives `[14, 5]`. The final subtraction leaves `[9]`.

Equal precedence runs immediately too, which keeps subtraction and division left to right. Everything uses loops, so deep parentheses don't add recursive calls.

## Errors as values

A model asking for a forbidden path isn't a C++ bug. Returning false with a reason lets it read the error and try again. The calculator does the same for division by zero. Exceptions make more sense for programmer mistakes, such as reading a JSON number as a string. The dispatcher catches exceptions that escape a tool as a last check.

## Checking it

The registry tests check collisions, duplicates, destruction counts, and all 10,000 saved pointers after resizing. Path tests cover every required case, missing paths, and escapes through file tools and symlinks. Calculator tests cover precedence, bad tokens, 20,000 terms, and 5,000 nested parentheses. All 31 P3 cases pass.

## What I'd change

Insertion hashes the same name twice, and statistics scan every bucket when requested. I'd reuse the insertion hash first if speed became an issue. The calculator's vectors also grow as needed instead of reserving space up front. These choices do a little extra work, but keep the code straightforward.
