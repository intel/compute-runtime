/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sharing.h"

#include "opencl/source/mem_obj/mem_obj.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {

int SharingHandler::acquire(MemObj *memObj, uint32_t rootDeviceIndex) {
    if (acquireCount == 0) {
        UpdateData updateData{rootDeviceIndex};
        auto graphicsAllocation = memObj->getGraphicsAllocation(rootDeviceIndex);
        auto currentSharedHandle = graphicsAllocation->peekSharedHandle();
        updateData.sharedHandle = currentSharedHandle;
        updateData.memObject = memObj;
        int result = synchronizeHandler(updateData);
        resolveGraphicsAllocationChange(currentSharedHandle, &updateData);
        if (result != CL_SUCCESS) {
            return result;
        }
        if (updateData.synchronizationStatus != SynchronizeStatus::ACQUIRE_SUCCESFUL) {
            return CL_OUT_OF_RESOURCES;
        }

        DEBUG_BREAK_IF(memObj->getGraphicsAllocation(rootDeviceIndex)->peekSharedHandle() != updateData.sharedHandle);
    }
    acquireCount++;
    return CL_SUCCESS;
}

int SharingHandler::synchronizeHandler(UpdateData &updateData) {
    auto result = validateUpdateData(updateData);
    if (result == CL_SUCCESS) {
        synchronizeObject(updateData);
    }
    return result;
}

int SharingHandler::validateUpdateData(UpdateData &updateData) {
    return CL_SUCCESS;
}

void SharingHandler::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
}

void SharingHandler::release(MemObj *memObject, uint32_t rootDeviceIndex) {
    DEBUG_BREAK_IF(acquireCount <= 0);
    acquireCount--;
    if (acquireCount == 0) {
        releaseResource(memObject, rootDeviceIndex);
    }
}
} // namespace NEO
