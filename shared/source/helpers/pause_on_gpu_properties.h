/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace NEO {
class LinearStream;
template <typename GfxFamily>
struct LriHelper;
template <typename GfxFamily>
struct EncodeSetMMIO;

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

inline bool pauseModeEnabled(int32_t debugFlagValue, PauseMode pauseMode) {
    if (!featureEnabled(debugFlagValue)) {
        // feature disabled
        return false;
    }

    return (debugManager.flags.PauseOnGpuMode.get() == PauseMode::BeforeAndAfterWorkload) || (debugManager.flags.PauseOnGpuMode.get() == pauseMode);
}

inline bool pauseModeAllowed(int32_t debugFlagValue, TaskCountType taskCount, PauseMode pauseMode) {
    if (!pauseModeEnabled(debugFlagValue, pauseMode)) {
        return false;
    }

    if (debugFlagValue == DebugFlagValues::OnEachEnqueue) {
        // pause on each enqueue
        return true;
    }

    return (debugFlagValue == static_cast<int64_t>(taskCount));
}

struct PauseSelection {
    bool beforeWorkload = false;
    bool afterWorkload = false;
};

inline PauseSelection selectPauses(int32_t debugFlagValue, TaskCountType taskCount) {
    return {.beforeWorkload = pauseModeAllowed(debugFlagValue, taskCount, PauseMode::BeforeWorkload),
            .afterWorkload = pauseModeAllowed(debugFlagValue, taskCount, PauseMode::AfterWorkload)};
}

inline PauseSelection selectPauseSpace(int32_t debugFlagValue, TaskCountType taskCount, bool selectedAtExecution) {
    if (!selectedAtExecution) {
        return selectPauses(debugFlagValue, taskCount);
    }
    return {.beforeWorkload = pauseModeEnabled(debugFlagValue, PauseMode::BeforeWorkload),
            .afterWorkload = pauseModeEnabled(debugFlagValue, PauseMode::AfterWorkload)};
}

struct PendingSubmissionPauses {
    void *afterWorkloadCommand = nullptr;
    size_t afterWorkloadCommandSize = 0;
    bool beforeWorkloadProgrammed = false;
};

// A numbered pause gets one confirmation cycle, so a submission keeps only its first start and last end pause
inline bool claimSubmissionPause(PendingSubmissionPauses &pendingPauses, int32_t debugFlagValue, bool beforeWorkload, void *pauseCommand, size_t pauseCommandSize) {
    if (debugFlagValue == DebugFlagValues::OnEachEnqueue) {
        return true;
    }

    if (beforeWorkload) {
        const bool firstBeforeWorkload = !pendingPauses.beforeWorkloadProgrammed;
        pendingPauses.beforeWorkloadProgrammed = true;
        return firstBeforeWorkload;
    }

    if (pendingPauses.afterWorkloadCommand != nullptr) {
        memset(pendingPauses.afterWorkloadCommand, 0, pendingPauses.afterWorkloadCommandSize);
    }
    pendingPauses.afterWorkloadCommand = pauseCommand;
    pendingPauses.afterWorkloadCommandSize = pauseCommandSize;
    return true;
}

inline bool gpuScratchRegWriteAllowed(int32_t debugFlagValue, TaskCountType taskCount) {
    if (!featureEnabled(debugFlagValue)) {
        // feature disabled
        return false;
    }

    return (debugFlagValue == static_cast<int64_t>(taskCount));
}

template <typename GfxFamily>
inline void programPauseRegisterWrite(LinearStream &commandStream, bool isBcs) {
    const auto registerOffset = static_cast<uint32_t>(debugManager.flags.PauseOnEnqueueRegisterOffset.get());
    const auto registerData = static_cast<uint32_t>(debugManager.flags.PauseOnEnqueueRegisterData.get());
    LriHelper<GfxFamily>::program(&commandStream, registerOffset, registerData,
                                  EncodeSetMMIO<GfxFamily>::isRemapApplicable(registerOffset), isBcs);
}
} // namespace PauseOnGpuProperties
} // namespace NEO
