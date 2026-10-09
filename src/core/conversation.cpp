#include "core/conversation.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

Conversation::~Conversation() { delete[] data_; }

Conversation::Conversation(const Conversation& other) {
    if (other.capacity_ == 0) return;
    // The temporary owner releases the array if a string copy throws.
    std::unique_ptr<Message[]> copy(new Message[other.capacity_]);
    for (std::size_t i = 0; i < other.size_; ++i) copy[i] = other.data_[i];
    data_ = copy.release();
    size_ = other.size_;
    capacity_ = other.capacity_;
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this != &other) {
        Conversation copy(other);
        *this = std::move(copy);
    }
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)),
      capacity_(std::exchange(other.capacity_, 0)) {}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    return *this;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        const auto max_slots = std::numeric_limits<std::size_t>::max() / sizeof(Message);
        if (capacity_ > max_slots / 2) {
            throw std::length_error("Conversation capacity overflow");
        }
        const std::size_t next_capacity = capacity_ == 0 ? 1 : capacity_ * 2;
        std::unique_ptr<Message[]> next(new Message[next_capacity]);
        // Deliberately suboptimal: copy strings during growth instead of
        // moving them. Keep the old array intact until every copy succeeds;
        // next releases partial work automatically if a string copy throws.
        for (std::size_t i = 0; i < size_; ++i) next[i] = data_[i];
        delete[] data_;
        data_ = next.release();
        capacity_ = next_capacity;
    }
    data_[size_] = std::move(m);
    ++size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) throw std::out_of_range("Conversation index out of range");
    return data_[i];
}
