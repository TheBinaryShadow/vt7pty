// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <utility>

// A solicited cursor-position reply is a separate frame, even when it arrives
// between bytes of a key token. Only one internal request can be outstanding.
// Reply bytes may span pipe reads, but must not be interleaved with key bytes.
class CursorReportFilter {
public:
    bool outstanding() const { return m_outstanding; }
    bool pending() const { return !m_prefix.empty(); }
    void request() { m_outstanding = true; }

    std::string append(std::string_view bytes) {
        std::string keys;
        keys.reserve(bytes.size());
        for (char ch : bytes) {
            if (!m_outstanding || (m_prefix.empty() && ch != '\x1b')) {
                keys += ch;
                continue;
            }
            m_prefix += ch;
            while (!m_prefix.empty()) {
                const int result = match(m_prefix);
                if (result < 0) break;
                if (result > 0) {
                    m_prefix.clear();
                    m_outstanding = false;
                    break;
                }
                // Replay rejected prefixes exactly, including a nested ESC
                // that can itself begin the actual reply.
                keys += m_prefix.front();
                m_prefix.erase(0, 1);
            }
        }
        return keys;
    }

    // The same escape timeout resolves an ambiguous, unfinished reply prefix.
    // The outstanding request survives so a late complete reply is consumed.
    std::string expire() { return std::exchange(m_prefix, {}); }

private:
    // -1: possible prefix; 0: ordinary input; 1: complete ESC[row;columnR.
    // Seven digits per coordinate bound the retained prefix to 18 bytes.
    static int match(std::string_view bytes) {
        size_t at = 0;
        for (char wanted : std::string_view("\x1b[")) {
            if (at == bytes.size()) return -1;
            if (bytes[at++] != wanted) return 0;
        }
        for (char delimiter : std::string_view(";R")) {
            size_t digits = 0;
            while (at < bytes.size() && bytes[at] >= '0' && bytes[at] <= '9') {
                if (++digits > 7) return 0;
                ++at;
            }
            if (at == bytes.size()) return -1;
            if (!digits || bytes[at++] != delimiter) return 0;
        }
        return at == bytes.size() ? 1 : 0;
    }
    bool m_outstanding = false;
    std::string m_prefix;
};
