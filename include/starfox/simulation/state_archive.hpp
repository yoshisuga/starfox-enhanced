#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace starfox::simulation {

// A save state archive. One visit_state() per class drives both directions,
// so a field cannot be written but not read back.
//
// Most of the simulation's sub-objects are trivially copyable and travel as a
// single block; only the three classes that own heap containers - the
// simulation, the map machine and the emulated CPU - enumerate their fields.

template <typename T, typename Archive>
concept HasVisitState = requires(T& value, Archive& archive) {
    value.visit_state(archive);
};

class StateWriter {
public:
    void raw(const void* data, std::size_t size) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        bytes_.insert(bytes_.end(), bytes, bytes + size);
    }

    template <typename T>
    void operator()(T& value) {
        if constexpr (HasVisitState<T, StateWriter>) {
            value.visit_state(*this);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            raw(&value, sizeof(T));
        } else {
            static_assert(sizeof(T) == 0,
                "no save state encoding for this member type");
        }
    }

    template <typename T, std::size_t N>
    void operator()(std::array<T, N>& value) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            raw(value.data(), sizeof(T) * N);
        } else {
            for (auto& element : value) (*this)(element);
        }
    }

    template <typename T>
    void operator()(std::vector<T>& value) {
        write_size(value.size());
        if constexpr (std::is_trivially_copyable_v<T>) {
            raw(value.data(), sizeof(T) * value.size());
        } else {
            for (auto& element : value) (*this)(element);
        }
    }

    void operator()(std::string& value) {
        write_size(value.size());
        raw(value.data(), value.size());
    }

    template <typename K, typename V>
    void operator()(std::unordered_map<K, V>& value) {
        write_size(value.size());
        for (auto& entry : value) {
            auto key = entry.first;
            (*this)(key);
            (*this)(entry.second);
        }
    }

    [[nodiscard]] std::vector<std::uint8_t> take() noexcept {
        return std::move(bytes_);
    }

private:
    void write_size(std::size_t size) {
        const auto encoded = static_cast<std::uint64_t>(size);
        raw(&encoded, sizeof(encoded));
    }

    std::vector<std::uint8_t> bytes_;
};

class StateReader {
public:
    explicit StateReader(std::span<const std::uint8_t> bytes) noexcept
        : bytes_{bytes} {}

    void raw(void* data, std::size_t size) {
        if (cursor_ + size > bytes_.size()) {
            throw std::runtime_error{"save state ended unexpectedly"};
        }
        std::memcpy(data, bytes_.data() + cursor_, size);
        cursor_ += size;
    }

    template <typename T>
    void operator()(T& value) {
        if constexpr (HasVisitState<T, StateReader>) {
            value.visit_state(*this);
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            raw(&value, sizeof(T));
        } else {
            static_assert(sizeof(T) == 0,
                "no save state encoding for this member type");
        }
    }

    template <typename T, std::size_t N>
    void operator()(std::array<T, N>& value) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            raw(value.data(), sizeof(T) * N);
        } else {
            for (auto& element : value) (*this)(element);
        }
    }

    template <typename T>
    void operator()(std::vector<T>& value) {
        value.assign(read_size(), T{});
        if constexpr (std::is_trivially_copyable_v<T>) {
            raw(value.data(), sizeof(T) * value.size());
        } else {
            for (auto& element : value) (*this)(element);
        }
    }

    void operator()(std::string& value) {
        value.assign(read_size(), '\0');
        raw(value.data(), value.size());
    }

    template <typename K, typename V>
    void operator()(std::unordered_map<K, V>& value) {
        const auto count = read_size();
        value.clear();
        value.reserve(count);
        for (std::size_t entry = 0; entry < count; ++entry) {
            K key{};
            V mapped{};
            (*this)(key);
            (*this)(mapped);
            value.emplace(key, mapped);
        }
    }

    [[nodiscard]] bool exhausted() const noexcept {
        return cursor_ == bytes_.size();
    }

private:
    [[nodiscard]] std::size_t read_size() {
        std::uint64_t encoded{};
        raw(&encoded, sizeof(encoded));
        // A corrupt length must not be turned into a huge allocation.
        if (encoded > bytes_.size()) {
            throw std::runtime_error{"save state declares an implausible size"};
        }
        return static_cast<std::size_t>(encoded);
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t cursor_{};
};

} // namespace starfox::simulation
