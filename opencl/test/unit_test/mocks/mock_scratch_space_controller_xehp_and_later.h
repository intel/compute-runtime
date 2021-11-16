/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"

namespace NEO {
struct MockScratchSpaceControllerXeHPAndLater : public ScratchSpaceControllerXeHPAndLater {
    using ScratchSpaceControllerXeHPAndLater::computeUnitsUsedForScratch;
    using ScratchSpaceControllerXeHPAndLater::getOffsetToSurfaceState;
    using ScratchSpaceControllerXeHPAndLater::perThreadScratchSize;
    using ScratchSpaceControllerXeHPAndLater::privateScratchAllocation;
    using ScratchSpaceControllerXeHPAndLater::privateScratchSizeBytes;
    using ScratchSpaceControllerXeHPAndLater::scratchAllocation;
    using ScratchSpaceControllerXeHPAndLater::scratchSizeBytes;
    using ScratchSpaceControllerXeHPAndLater::ScratchSpaceControllerXeHPAndLater;
    using ScratchSpaceControllerXeHPAndLater::singleSurfaceStateSize;
    using ScratchSpaceControllerXeHPAndLater::slotId;
    using ScratchSpaceControllerXeHPAndLater::stateSlotsCount;
    using ScratchSpaceControllerXeHPAndLater::surfaceStateHeap;
    using ScratchSpaceControllerXeHPAndLater::updateSlots;
};
} // namespace NEO
