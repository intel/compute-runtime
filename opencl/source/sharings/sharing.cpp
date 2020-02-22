/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sharing.h"

#include "opencl/source/mem_obj/mem_obj.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {

int SharingHandler::acquire(MemObj *memObj) {
    if (acquireCount == 0) {
        UpdateData updateData;
        auto currentSharedHandle = memObj->getGraphicsAllocation()->peekSharedHandle();
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

        DEBUG_BREAK_IF(memObj->getGraphicsAllocation()->peekSharedHandle() != updateData.sharedHandle);
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

void SharingHandler::release(MemObj *memObject) {
    DEBUG_BREAK_IF(acquireCount <= 0);
    acquireCount--;
    if (acquireCount == 0) {
        releaseResource(memObject);
    }
}
} // namespace NEO
