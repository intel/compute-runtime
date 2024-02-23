/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/command_encoder_args.h"

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
        Invalid
    };
    void *pDestination = nullptr;
    void *pCommand = nullptr;
    CommandType type = Invalid;
};

struct CmdListKernelLaunchParams {
    void *outWalker = nullptr;
    CommandToPatch *outSyncCommand = nullptr;
    std::vector<CommandToPatch> *outListCommands = nullptr;
    NEO::RequiredPartitionDim requiredPartitionDim = NEO::RequiredPartitionDim::none;
    NEO::RequiredDispatchWalkOrder requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none;
    uint32_t additionalSizeParam = NEO::additionalKernelLaunchSizeParamNotSet;
    uint32_t numKernelsInSplitLaunch = 0;
    uint32_t numKernelsExecutedInSplitLaunch = 0;
    bool isIndirect = false;
    bool isPredicate = false;
    bool isCooperative = false;
    bool isKernelSplitOperation = false;
    bool isBuiltInKernel = false;
    bool isDestinationAllocationInSystemMemory = false;
    bool isHostSignalScopeEvent = false;
    bool skipInOrderNonWalkerSignaling = false;
    bool pipeControlSignalling = false;
    bool omitAddingKernelResidency = false;
    bool omitAddingEventResidency = false;
};
} // namespace L0
