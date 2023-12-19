/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/command_encoder_args.h"

#include <cstdint>

namespace L0 {
struct CmdListKernelLaunchParams {
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
};
} // namespace L0
