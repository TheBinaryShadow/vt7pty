// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT

#ifndef VT7PTY_SHARED_NARROW_H
#define VT7PTY_SHARED_NARROW_H

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "Assert.h"

namespace vt7pty::internal {

template <std::integral T>
using ComparableInteger = std::conditional_t<
    std::is_same_v<T, char>,
    std::conditional_t<std::is_signed_v<char>, int8_t, uint8_t>,
    std::conditional_t<
        std::is_same_v<T, wchar_t>,
        std::conditional_t<std::is_signed_v<wchar_t>, int16_t, uint16_t>,
        std::conditional_t<std::is_same_v<T, bool>, uint8_t, T>>>;

template <std::integral To, std::integral From>
constexpr bool isNarrowable(From value) noexcept {
    using ComparableTo = ComparableInteger<To>;
    using ComparableFrom = ComparableInteger<From>;
    return std::in_range<ComparableTo>(static_cast<ComparableFrom>(value));
}

template <std::integral To, std::integral From>
To checkedNarrow(From value) {
    ASSERT(isNarrowable<To>(value));
    return static_cast<To>(value);
}

} // namespace vt7pty::internal

#endif // VT7PTY_SHARED_NARROW_H
