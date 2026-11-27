// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once

#if defined(VT7PTY_LIFECYCLE_RESIZE_TRACE) || defined(VT7PTY_LIFECYCLE_TRACE_READER)

#include <windows.h>
#include <cstdint>

namespace lifecycle_resize_trace {

inline constexpr wchar_t environment[] = L"VT7PTY_TEST_RESIZE_TRACE";
inline constexpr uint32_t magic = 0x31525A56;
inline constexpr uint32_t capacity = 4096;

enum class Phase : uint32_t {
    Dispatch, Resize, Reply, PollTimeout, Input, Scrape, OpenBuffer,
    SyncResize, Font, BufferSize, WindowRect, Freeze, Unfreeze, SizeEvent
};

struct SpanRecord {
    Phase phase;
    int columns;
    int rows;
    int64_t begin;
    int64_t end;
};

struct Buffer {
    uint32_t version;
    volatile LONG agentPid;
    volatile LONG enabled;
    uint32_t count;
    uint32_t dropped;
    SpanRecord spans[capacity];
};

inline int64_t timestamp() noexcept {
    LARGE_INTEGER time{};
    QueryPerformanceCounter(&time);
    return time.QuadPart;
}

#ifdef VT7PTY_LIFECYCLE_RESIZE_TRACE
inline Buffer *openWriter() noexcept {
    wchar_t name[128]{};
    const DWORD length = GetEnvironmentVariableW(environment, name, ARRAYSIZE(name));
    if (length == 0 || length >= ARRAYSIZE(name)) return nullptr;

    const HANDLE mapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, name);
    if (mapping == nullptr) return nullptr;
    auto *buffer = static_cast<Buffer *>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(Buffer)));
    CloseHandle(mapping);
    if (buffer == nullptr) return nullptr;
    if (buffer->version != magic) {
        UnmapViewOfFile(buffer);
        return nullptr;
    }
    InterlockedExchange(&buffer->agentPid, static_cast<LONG>(GetCurrentProcessId()));
    return buffer;
}

// The agent event loop is the sole writer. The controller reads after agent exit.
class Span {
public:
    explicit Span(Phase phase, int columns = 0, int rows = 0) noexcept {
        static Buffer *const shared = openWriter();
        if (shared && InterlockedCompareExchange(&shared->enabled, 0, 0)) {
            buffer_ = shared;
            record_ = {phase, columns, rows, timestamp(), 0};
        }
    }
    ~Span() {
        if (!buffer_) return;
        record_.end = timestamp();
        if (buffer_->count < capacity) {
            buffer_->spans[buffer_->count++] = record_;
        } else {
            ++buffer_->dropped;
        }
    }
    Span(const Span &) = delete;
    Span &operator=(const Span &) = delete;

private:
    Buffer *buffer_ = nullptr;
    SpanRecord record_{};
};
#endif

} // namespace lifecycle_resize_trace
#endif

#ifdef VT7PTY_LIFECYCLE_RESIZE_TRACE
#define VT7PTY_RESIZE_TRACE(phase, ...) \
    const ::lifecycle_resize_trace::Span phase##Trace( \
        ::lifecycle_resize_trace::Phase::phase __VA_OPT__(,) __VA_ARGS__)
#else
#define VT7PTY_RESIZE_TRACE(...) (void)0
#endif
