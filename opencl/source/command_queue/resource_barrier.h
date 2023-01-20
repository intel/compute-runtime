/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/memory_manager/resource_surface.h"

namespace NEO {
class CommandQueue;
class BarrierCommand {
  public:
    BarrierCommand(CommandQueue *commandQueue, const cl_resource_barrier_descriptor_intel *descriptors, uint32_t numDescriptors);
    ~BarrierCommand() {}
    uint32_t numSurfaces = 0;
    StackVec<ResourceSurface, 32> surfaces;
    StackVec<Surface *, 32> surfacePtrs;
};
} // namespace NEO
