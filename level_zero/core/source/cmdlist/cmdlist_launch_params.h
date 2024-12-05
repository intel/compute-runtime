/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/command_encoder_args.h"

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
        Invalid
    };
    void *pDestination = nullptr;
    void *pCommand = nullptr;
    size_t offset = 0;
    CommandType type = Invalid;
    size_t inOrderPatchListIndex = 0;
    size_t patchSize = 0;
    uint64_t baseAddress = 0;
};

using CommandToPatchContainer = std::vector<CommandToPatch>;

struct CmdListKernelLaunchParams {
    void *outWalker = nullptr;
    void *cmdWalkerBuffer = nullptr;
    void *hostPayloadBuffer = nullptr;
    CommandToPatch *outSyncCommand = nullptr;
    CommandToPatchContainer *outListCommands = nullptr;
    size_t syncBufferPatchIndex = std::numeric_limits<size_t>::max();
    size_t regionBarrierPatchIndex = std::numeric_limits<size_t>::max();
    uint32_t externalPerThreadScratchSize[2] = {0U, 0U};
    NEO::RequiredPartitionDim requiredPartitionDim = NEO::RequiredPartitionDim::none;
    NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none;
    uint32_t localRegionSize = NEO::localRegionSizeParamNotSet;
    uint32_t numKernelsInSplitLaunch = 0;
    uint32_t numKernelsExecutedInSplitLaunch = 0;
    uint32_t reserveExtraPayloadSpace = 0;
    bool isIndirect = false;
    bool isPredicate = false;
    bool isCooperative = false;
    bool isKernelSplitOperation = false;
    bool isBuiltInKernel = false;
    bool isDestinationAllocationInSystemMemory = false;
    bool isHostSignalScopeEvent = false;
    bool isExpLaunchKernel = false;
    bool skipInOrderNonWalkerSignaling = false;
    bool pipeControlSignalling = false;
    bool omitAddingKernelArgumentResidency = false;
    bool omitAddingKernelInternalResidency = false;
    bool omitAddingEventResidency = false;
    bool omitAddingWaitEventsResidency = false;
    bool makeKernelCommandView = false;
};

struct CmdListMemoryCopyParams {
    bool relaxedOrderingDispatch = false;
    bool forceDisableCopyOnlyInOrderSignaling = false;
    bool copyOffloadAllowed = false;
};
} // namespace L0
