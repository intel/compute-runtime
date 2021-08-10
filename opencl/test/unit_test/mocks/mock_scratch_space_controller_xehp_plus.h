/*
 * Copyright (C) 2016-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/scratch_space_controller_xehp_plus.h"

namespace NEO {
struct MockScratchSpaceControllerXeHPPlus : public ScratchSpaceControllerXeHPPlus {
    using ScratchSpaceControllerXeHPPlus::computeUnitsUsedForScratch;
    using ScratchSpaceControllerXeHPPlus::getOffsetToSurfaceState;
    using ScratchSpaceControllerXeHPPlus::perThreadScratchSize;
    using ScratchSpaceControllerXeHPPlus::privateScratchAllocation;
    using ScratchSpaceControllerXeHPPlus::privateScratchSizeBytes;
    using ScratchSpaceControllerXeHPPlus::scratchAllocation;
    using ScratchSpaceControllerXeHPPlus::scratchSizeBytes;
    using ScratchSpaceControllerXeHPPlus::ScratchSpaceControllerXeHPPlus;
    using ScratchSpaceControllerXeHPPlus::singleSurfaceStateSize;
    using ScratchSpaceControllerXeHPPlus::slotId;
    using ScratchSpaceControllerXeHPPlus::stateSlotsCount;
    using ScratchSpaceControllerXeHPPlus::surfaceStateHeap;
    using ScratchSpaceControllerXeHPPlus::updateSlots;
};
} // namespace NEO
