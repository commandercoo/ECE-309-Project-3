#pragma once

#include <string>
#include <string_view>

class SentinelScanner {
public:
    // An empty sentinel is invalid. A match permanently ends this scanner.
    explicit SentinelScanner(std::string sentinel);
    struct Out { std::string safe_text; bool sentinel_found; };
    Out feed(std::string_view chunk);
    Out flush();

private:
    friend struct CoreTestAccess;
    std::string sentinel_;
    std::string pending_;
    bool found_ = false;
};
