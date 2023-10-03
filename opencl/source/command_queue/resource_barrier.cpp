/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/resource_barrier.h"

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/range.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/mem_obj/mem_obj.h"

namespace NEO {
BarrierCommand::BarrierCommand(CommandQueue *commandQueue, const cl_resource_barrier_descriptor_intel *descriptors, uint32_t numDescriptors) : numSurfaces(numDescriptors) {
    for (auto &description : createRange(descriptors, numDescriptors)) {
        GraphicsAllocation *allocation;
        if (description.memObject) {
            MemObj *memObj = nullptr;
            withCastToInternal(description.memObject, &memObj);
            allocation = memObj->getGraphicsAllocation(commandQueue->getDevice().getRootDeviceIndex());
        } else {
            auto svmData = commandQueue->getContext().getSVMAllocsManager()->getSVMAlloc(description.svmAllocationPointer);
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
