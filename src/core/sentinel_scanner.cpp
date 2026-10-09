#include "core/sentinel_scanner.h"

#include <stdexcept>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {
    if (sentinel_.empty()) throw std::invalid_argument("Sentinel must not be empty");
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    Out out{"", found_};
    if (found_) return out;

    for (char c : chunk) {
        // Keep only a suffix that is a prefix of the sentinel and can
        // extend with c. Discarded characters can no longer start a match.
        while (!pending_.empty() &&
               (sentinel_.compare(0, pending_.size(), pending_) != 0 ||
                sentinel_[pending_.size()] != c)) {
            out.safe_text += pending_.front();
            pending_.erase(0, 1);
        }
        if (sentinel_[pending_.size()] == c) {
            if (pending_.size() + 1 == sentinel_.size()) {
                pending_.clear();
                found_ = true;
                out.sentinel_found = true;
                return out; // Ignore everything following the first match.
            }
            pending_ += c;
        } else {
            out.safe_text += c;
        }
    }
    return out;
}

SentinelScanner::Out SentinelScanner::flush() {
    Out out{std::move(pending_), found_};
    pending_.clear();
    return out;
}
