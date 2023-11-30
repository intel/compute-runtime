/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include <cstdint>

namespace NEO {
namespace PauseOnGpuProperties {
enum PauseMode : int32_t {
    BeforeAndAfterWorkload = -1,
    BeforeWorkload = 0,
    AfterWorkload = 1
};

enum DebugFlagValues : int32_t {
    OnEachEnqueue = -2,
    Disabled = -1
};

inline bool featureEnabled(int32_t debugFlagValue) {
    return (debugFlagValue != DebugFlagValues::Disabled);
}

inline bool pauseModeAllowed(int32_t debugFlagValue, TaskCountType taskCount, PauseMode pauseMode) {
    if (!featureEnabled(debugFlagValue)) {
        // feature disabled
        return false;
    }

    if ((debugManager.flags.PauseOnGpuMode.get() != PauseMode::BeforeAndAfterWorkload) && (debugManager.flags.PauseOnGpuMode.get() != pauseMode)) {
        // mode not allowed
        return false;
    }

    if (debugFlagValue == DebugFlagValues::OnEachEnqueue) {
        // pause on each enqueue
        return true;
    }

    return (debugFlagValue == static_cast<int64_t>(taskCount));
}

inline bool gpuScratchRegWriteAllowed(int32_t debugFlagValue, TaskCountType taskCount) {
    if (!featureEnabled(debugFlagValue)) {
        // feature disabled
        return false;
    }

    return (debugFlagValue == static_cast<int64_t>(taskCount));
}
} // namespace PauseOnGpuProperties
} // namespace NEO
