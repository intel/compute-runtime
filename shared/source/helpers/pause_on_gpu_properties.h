/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

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

inline bool pauseModeAllowed(int32_t debugFlagValue, uint32_t taskCount, PauseMode pauseMode) {
    if (!featureEnabled(debugFlagValue)) {
        // feature disabled
        return false;
    }

    if ((DebugManager.flags.PauseOnGpuMode.get() != PauseMode::BeforeAndAfterWorkload) && (DebugManager.flags.PauseOnGpuMode.get() != pauseMode)) {
        // mode not allowed
        return false;
    }

    if (debugFlagValue == DebugFlagValues::OnEachEnqueue) {
        // pause on each enqueue
        return true;
    }

    return (debugFlagValue == static_cast<int32_t>(taskCount));
}

inline bool gpuScratchRegWriteAllowed(int32_t debugFlagValue, uint32_t taskCount) {
    if (!featureEnabled(debugFlagValue)) {
        // feature disabled
        return false;
    }

    return (debugFlagValue == static_cast<int32_t>(taskCount));
}
} // namespace PauseOnGpuProperties
} // namespace NEO
