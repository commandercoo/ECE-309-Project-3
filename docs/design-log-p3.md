# Design Log — Project 3

**Author:** William Zhou

**P2 code:** My own Conversation and SentinelScanner from Project 2.

**Tools used:** CMake and Clang are the build tools; Git keeps the milestones.

## Bringing P2 along

I kept my Conversation and SentinelScanner as they were in P2. The new Message adds a tool role, but Conversation just stores messages, so it doesn't need to know what that role means. Its extra copying during growth is still there too.

## Next pieces

With the P2 pieces copied over, the registry is next. The starter handles the JSON, file tools, and model loop, so I can focus on the table, the path check, and the calculator's evaluation step. The supplied tests should help catch anything that stops working along the way.
