// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT

// Included by FidelityTest.cc to share its evidence writer.
class LifecycleResizeCapture {
public:
    LifecycleResizeCapture() {
        using namespace lifecycle_resize_trace;
        LARGE_INTEGER frequency{};
        check(QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0, "resize trace clock");
        frequency_ = frequency.QuadPart;
        name_ = L"Local\\VT7Pty.LifecycleResizeTrace." + std::to_wstring(GetCurrentProcessId()) +
            L"." + std::to_wstring(timestamp());
        mapping_.reset(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, static_cast<DWORD>(sizeof(Buffer)), name_.c_str()));
        check(mapping_.value && GetLastError() != ERROR_ALREADY_EXISTS, "create resize trace mapping");
        void *view = MapViewOfFile(mapping_.value, FILE_MAP_WRITE, 0, 0, sizeof(Buffer));
        check(view != nullptr, "map resize trace");
        buffer_ = new (view) Buffer{};
        buffer_->version = magic;
    }

    ~LifecycleResizeCapture() { UnmapViewOfFile(buffer_); }
    LifecycleResizeCapture(const LifecycleResizeCapture &) = delete;
    LifecycleResizeCapture &operator=(const LifecycleResizeCapture &) = delete;

    void prepareLaunch() {
        check(SetEnvironmentVariableW(lifecycle_resize_trace::environment, name_.c_str()),
            "set resize trace launch environment");
    }

    void finishLaunch(HANDLE agent) {
        check(SetEnvironmentVariableW(lifecycle_resize_trace::environment, nullptr),
            "clear resize trace launch environment");
        const DWORD pid = GetProcessId(agent);
        const auto writer = static_cast<DWORD>(InterlockedCompareExchange(&buffer_->agentPid, 0, 0));
        check(pid && pid == writer, "resize trace agent did not connect");
    }

    void start() noexcept {
        begin_ = lifecycle_resize_trace::timestamp();
        InterlockedExchange(&buffer_->enabled, 1);
    }

    void stop() noexcept {
        if (begin_ && !end_) {
            InterlockedExchange(&buffer_->enabled, 0);
            end_ = lifecycle_resize_trace::timestamp();
        }
    }

    int64_t record(int ordinal, int columns, int rows, int64_t begin, int64_t end) {
        check(ordinal == calls_ + 1 && ordinal <= lifecycleSamples, "resize trace call order");
        callsData_[calls_++] = {columns, rows, begin, end};
        return (end - begin) * 1000000 / frequency_;
    }

    void save(const std::wstring &directory, HANDLE agent) const {
        using namespace lifecycle_resize_trace;
        check(WaitForSingleObject(agent, 0) == WAIT_OBJECT_0, "resize trace agent still running");
        check(buffer_->count <= capacity, "resize trace span count");
        static constexpr const char *phaseNames[] = {
            "Dispatch", "Resize", "Reply", "PollTimeout", "Input", "Scrape", "OpenBuffer",
            "SyncResize", "Font", "BufferSize", "WindowRect", "Freeze", "Unfreeze", "SizeEvent"
        };

        std::string data = "{\"SchemaVersion\":1,\"Frequency\":" + std::to_string(frequency_) +
            ",\"ExpectedAgentPid\":" + std::to_string(GetProcessId(agent)) +
            ",\"AgentPid\":" + std::to_string(static_cast<DWORD>(buffer_->agentPid)) +
            ",\"BeginQpc\":" + std::to_string(begin_) + ",\"EndQpc\":" + std::to_string(end_) +
            ",\"Dropped\":" + std::to_string(buffer_->dropped) + ",\"Calls\":[";
        for (int i = 0; i < calls_; ++i) {
            if (i) data += ',';
            data += "{\"Ordinal\":" + std::to_string(i + 1) + ',' + spanFields(callsData_[i]) + '}';
        }
        data += "],\"Spans\":[";
        for (uint32_t i = 0; i < buffer_->count; ++i) {
            const auto &span = buffer_->spans[i];
            const auto phase = static_cast<size_t>(span.phase);
            check(phase < std::size(phaseNames), "resize trace phase");
            if (i) data += ',';
            data += "{\"Phase\":" + json(phaseNames[phase]) + ',' + spanFields(span) + '}';
        }
        writeFile(directory + L"\\resize-trace.json", data + "]}");
    }

private:
    struct Call {
        int columns;
        int rows;
        int64_t begin;
        int64_t end;
    };

    template<class Record>
    static std::string spanFields(const Record &entry) {
        return "\"Columns\":" + std::to_string(entry.columns) + ",\"Rows\":" +
            std::to_string(entry.rows) + ",\"BeginQpc\":" + std::to_string(entry.begin) +
            ",\"EndQpc\":" + std::to_string(entry.end);
    }

    Handle mapping_;
    lifecycle_resize_trace::Buffer *buffer_ = nullptr;
    std::wstring name_;
    int64_t frequency_ = 0;
    int64_t begin_ = 0;
    int64_t end_ = 0;
    int calls_ = 0;
    Call callsData_[lifecycleSamples]{};
};
