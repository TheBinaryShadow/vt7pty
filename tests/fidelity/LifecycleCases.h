// Copyright (c) 2026 VT7Pty contributors
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <string_view>

namespace fidelity {

inline constexpr int lifecycleRows = 200;
inline constexpr int lifecycleSamples = 16;
inline constexpr int lifecycleIdleMs = 15000;
inline constexpr int lifecycleActiveMs = 20000;
inline constexpr int lifecycleActiveStepMs = 25;
inline constexpr int lifecycleActiveStepLimit = 1 + lifecycleActiveMs / lifecycleActiveStepMs;
inline constexpr unsigned int lifecycleFixtureDeadlineMs = 65000;
inline constexpr unsigned int lifecycleExitCode = 37;
inline constexpr unsigned int lifecycleShutdownMs = 10000;

enum class LifecycleMode { CmdDrain, PowerShellDrain, MissingTail, Recover, Close, Measure };

struct LifecycleCase {
    std::wstring_view name;
    LifecycleMode mode;
};

inline constexpr LifecycleCase lifecycleCases[] = {
    {L"LIFECYCLE_CMD_DRAIN", LifecycleMode::CmdDrain},
    {L"LIFECYCLE_PS_DRAIN", LifecycleMode::PowerShellDrain},
    {L"LIFECYCLE_NEGATIVE_DRAIN", LifecycleMode::MissingTail},
    {L"LIFECYCLE_RECOVER", LifecycleMode::Recover},
    {L"LIFECYCLE_CLOSE", LifecycleMode::Close},
    {L"LIFECYCLE_MEASURE_1", LifecycleMode::Measure},
    {L"LIFECYCLE_MEASURE_2", LifecycleMode::Measure},
    {L"LIFECYCLE_MEASURE_3", LifecycleMode::Measure},
    {L"LIFECYCLE_MEASURE_4", LifecycleMode::Measure},
};

inline const LifecycleCase *findLifecycleCase(std::wstring_view name) {
    for (const auto &test : lifecycleCases) {
        if (test.name == name) return &test;
    }
    return nullptr;
}

inline std::wstring lifecycleBarrierMarker(int index) {
    return L"VT7_BARRIER_" + std::to_wstring(index) + L"_END";
}

} // namespace fidelity
