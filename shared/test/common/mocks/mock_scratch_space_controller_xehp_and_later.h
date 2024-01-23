/*
 * Copyright (C) 2018-2024 Intel Corporation
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
    using ScratchSpaceControllerXeHPAndLater::scratchSlot0Allocation;
    using ScratchSpaceControllerXeHPAndLater::scratchSlot0SizeInBytes;
    using ScratchSpaceControllerXeHPAndLater::scratchSlot1Allocation;
    using ScratchSpaceControllerXeHPAndLater::scratchSlot1SizeInBytes;
    using ScratchSpaceControllerXeHPAndLater::ScratchSpaceControllerXeHPAndLater;
    using ScratchSpaceControllerXeHPAndLater::singleSurfaceStateSize;
    using ScratchSpaceControllerXeHPAndLater::slotId;
    using ScratchSpaceControllerXeHPAndLater::stateSlotsCount;
    using ScratchSpaceControllerXeHPAndLater::surfaceStateHeap;
    using ScratchSpaceControllerXeHPAndLater::updateSlots;
};
} // namespace NEO
