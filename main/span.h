#pragma once

#include <cstddef>
#include <cstdint>

namespace ipbuilding {

template <typename T>
class Span {
public:
    constexpr Span() : data_(nullptr), size_(0) {}
    constexpr Span(T *data, std::size_t size) : data_(data), size_(size) {}
    template <std::size_t N>
    constexpr Span(T (&array)[N]) : data_(array), size_(N)
    {
    }

    constexpr T *data() const { return data_; }
    constexpr std::size_t size() const { return size_; }
    constexpr bool empty() const { return size_ == 0; }

    constexpr T &operator[](std::size_t index) const { return data_[index]; }

    constexpr Span<T> subspan(std::size_t offset, std::size_t count) const
    {
        if (offset >= size_) {
            return {};
        }
        const std::size_t clamped = (count > size_ - offset) ? (size_ - offset) : count;
        return Span<T>(data_ + offset, clamped);
    }

private:
    T *data_;
    std::size_t size_;
};

}  // namespace ipbuilding
