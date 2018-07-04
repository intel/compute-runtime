/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "CL/cl.h"
#include "runtime/mem_obj/mem_obj.h"
#include "sharing.h"
#include <memory>

namespace OCLRT {

int SharingHandler::acquire(MemObj *memObj) {
    if (acquireCount == 0) {
        UpdateData updateData;
        auto currentSharedHandle = memObj->getGraphicsAllocation()->peekSharedHandle();
        updateData.sharedHandle = currentSharedHandle;
        updateData.memObject = memObj;
        int result = synchronizeHandler(&updateData);
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

int SharingHandler::synchronizeHandler(UpdateData *updateData) {
    auto result = validateUpdateData(updateData);
    if (result == CL_SUCCESS) {
        synchronizeObject(updateData);
    }
    return result;
}

int SharingHandler::validateUpdateData(UpdateData *updateData) {
    UNRECOVERABLE_IF(updateData == nullptr);
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
} // namespace OCLRT
