#include "tools/tool_registry.h"

#include <cstdint>
#include <stdexcept>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(16) {}
ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    // FNV-1a: mix each byte before multiplying, keeping the full hash.
    std::uint64_t value = 14695981039346656037ULL;
    for (unsigned char c : key) {
        value ^= c;
        value *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(value);
}

void ToolRegistry::grow() {
    if (buckets_.size() > buckets_.max_size() / 2) {
        throw std::length_error("ToolRegistry capacity overflow");
    }
    std::vector<std::vector<Entry>> next(buckets_.size() * 2);
    std::vector<std::size_t> counts(next.size(), 0);

    // Count first so all allocations finish before ownership is moved.
    for (const auto& bucket : buckets_) {
        for (const auto& entry : bucket) ++counts[hash(entry.name) % next.size()];
    }
    for (std::size_t i = 0; i < next.size(); ++i) next[i].reserve(counts[i]);

    for (auto& bucket : buckets_) {
        for (auto& entry : bucket) {
            const std::size_t index = hash(entry.name) % next.size();
            next[index].push_back(std::move(entry));
        }
    }
    buckets_.swap(next);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (tool == nullptr) return false;
    std::string name = tool->name();
    if (find(name) != nullptr) return false;

    // Chaining allows a load factor of 1. Grow before exceeding it.
    if (size_ == buckets_.size()) grow();
    const std::size_t index = hash(name) % buckets_.size();
    buckets_[index].push_back({std::move(name), std::move(tool)});
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    const std::size_t index = hash(name) % buckets_.size();
    for (const auto& entry : buckets_[index]) {
        if (entry.name == name) return entry.tool.get();
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept { return size_; }

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) {
        for (const auto& entry : bucket) fn(*entry.tool);
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept { return buckets_.size(); }

double ToolRegistry::load_factor() const noexcept {
    return static_cast<double>(size_) / buckets_.size();
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& bucket : buckets_) {
        if (bucket.size() > longest) longest = bucket.size();
    }
    return longest;
}
