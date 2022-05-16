/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/resource_barrier.h"

#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/range.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_validators.h"

namespace NEO {
BarrierCommand::BarrierCommand(CommandQueue *commandQueue, const cl_resource_barrier_descriptor_intel *descriptors, uint32_t numDescriptors) : numSurfaces(numDescriptors) {
    for (auto description : createRange(descriptors, numDescriptors)) {
        GraphicsAllocation *allocation;
        if (description.mem_object) {
            MemObj *memObj = nullptr;
            withCastToInternal(description.mem_object, &memObj);
            allocation = memObj->getGraphicsAllocation(commandQueue->getDevice().getRootDeviceIndex());
        } else {
            auto svmData = commandQueue->getContext().getSVMAllocsManager()->getSVMAlloc(description.svm_allocation_pointer);
            UNRECOVERABLE_IF(svmData == nullptr);
            allocation = svmData->gpuAllocations.getGraphicsAllocation(commandQueue->getDevice().getRootDeviceIndex());
        }
        surfaces.push_back(ResourceSurface(allocation, description.type, description.scope));
    }
    for (auto it = surfaces.begin(), end = surfaces.end(); it != end; it++) {
        surfacePtrs.push_back(it);
    }
}
} // namespace NEO
