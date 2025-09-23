/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace L0 {

struct CommandToPatch {
    enum CommandType {
        FrontEndState,
        PauseOnEnqueueSemaphoreStart,
        PauseOnEnqueueSemaphoreEnd,
        PauseOnEnqueuePipeControlStart,
        PauseOnEnqueuePipeControlEnd,
        ComputeWalker,
        SignalEventPostSyncPipeControl,
        WaitEventSemaphoreWait,
        TimestampEventPostSyncStoreRegMem,
        CbEventTimestampPostSyncSemaphoreWait,
        CbEventTimestampClearStoreDataImm,
        CbWaitEventSemaphoreWait,
        CbWaitEventLoadRegisterImm,
        ComputeWalkerInlineDataScratch,
        ComputeWalkerImplicitArgsScratch,
        NoopSpace,
        PrefetchKernelMemory,
        HostFunctionEntry,
        HostFunctionUserData,
        HostFunctionSignalInternalTag,
        HostFunctionWaitInternalTag,
        Invalid
    };
    void *pDestination = nullptr;
    void *pCommand = nullptr;
    uint64_t baseAddress = 0;
    uint64_t gpuAddress = 0;
    mutable uint64_t scratchAddressAfterPatch = 0;
    size_t offset = 0;
    size_t inOrderPatchListIndex = 0;
    size_t patchSize = 0;
    CommandType type = Invalid;
};

using CommandToPatchContainer = std::vector<CommandToPatch>;
} // namespace L0
