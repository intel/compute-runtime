/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
class Context;
class MemObj;

enum SynchronizeStatus {
    SHARED_OBJECT_NOT_CHANGED,
    SHARED_OBJECT_REQUIRES_UPDATE,
    ACQUIRE_SUCCESFUL,
    SYNCHRONIZE_ERROR
};

struct UpdateData {
    UpdateData(uint32_t inRootDeviceIndex) : rootDeviceIndex(inRootDeviceIndex){};
    const uint32_t rootDeviceIndex;
    SynchronizeStatus synchronizationStatus = SHARED_OBJECT_NOT_CHANGED;
    osHandle sharedHandle = 0;
    MemObj *memObject = nullptr;
    void *updateData = nullptr;
};

class SharingFunctions {
  public:
    virtual uint32_t getId() const = 0;

    virtual ~SharingFunctions() = default;
};

class SharingHandler {
  public:
    int acquire(MemObj *memObj, uint32_t rootDeviceIndex);
    void release(MemObj *memObject, uint32_t rootDeviceIndex);
    virtual ~SharingHandler() = default;

    virtual void getMemObjectInfo(size_t &paramValueSize, void *&paramValue){};
    virtual void releaseReusedGraphicsAllocation(){};

  protected:
    virtual int synchronizeHandler(UpdateData &updateData);
    virtual int validateUpdateData(UpdateData &updateData);
    virtual void synchronizeObject(UpdateData &updateData) { updateData.synchronizationStatus = SYNCHRONIZE_ERROR; }
    virtual void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData);
    virtual void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex){};
    unsigned int acquireCount = 0u;
};
} // namespace NEO
