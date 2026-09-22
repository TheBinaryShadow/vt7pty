// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT

#include <cassert>
#include <cstdint>
#include <format>
#include <limits>
#include <iostream>
#include <string>

#include "../../src/shared/Narrow.h"

using vt7pty::internal::checkedNarrow;
using vt7pty::internal::isNarrowable;

int main() {
    static_assert(isNarrowable<uint32_t>(uint64_t { 42 }));
    static_assert(!isNarrowable<uint32_t>(
        uint64_t { std::numeric_limits<uint32_t>::max() } + 1));
    static_assert(!isNarrowable<uint32_t>(-1));

    assert(checkedNarrow<uint32_t>(uint64_t { 42 }) == 42);
    assert(checkedNarrow<int16_t>(int32_t { -123 }) == -123);
    assert(std::format("{} {:08x}", 17, 0x2Au) == "17 0000002a");
    assert(std::format(L"{} {}", L"VT7Pty", 7) == L"VT7Pty 7");

    std::cout << "VT7Pty modern C++ utility tests passed.\n";
    return 0;
}
