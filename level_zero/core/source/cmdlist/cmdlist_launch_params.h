/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/command_encoder_args.h"

#include "level_zero/core/source/cmdlist/command_to_patch.h"

#include "cmdlist_launch_params_ext.h"

#include <cstddef>
#include <cstdint>

namespace L0 {

struct CmdListKernelLaunchParams {
    void *outWalker = nullptr;
    void *cmdWalkerBuffer = nullptr;
    void *hostPayloadBuffer = nullptr;
    CommandToPatch *outSyncCommand = nullptr;
    CommandToPatchContainer *outListCommands = nullptr;
    CmdListKernelLaunchParamsExt launchParamsExt{};
    size_t syncBufferPatchIndex = std::numeric_limits<size_t>::max();
    size_t regionBarrierPatchIndex = std::numeric_limits<size_t>::max();
    size_t scratchAddressPatchIndex = std::numeric_limits<size_t>::max();
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
    bool isDestinationAllocationImported = false;
    bool isHostSignalScopeEvent = false;
    bool isExpLaunchKernel = false;
    bool skipInOrderNonWalkerSignaling = false;
    bool pipeControlSignalling = false;
    bool omitAddingKernelArgumentResidency = false;
    bool omitAddingKernelInternalResidency = false;
    bool omitAddingEventResidency = false;
    bool omitAddingWaitEventsResidency = false;
    bool makeKernelCommandView = false;
    bool relaxedOrderingDispatch = false;
};
} // namespace L0
