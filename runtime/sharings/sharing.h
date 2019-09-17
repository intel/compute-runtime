/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/memory_manager/graphics_allocation.h"

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
    SynchronizeStatus synchronizationStatus;
    osHandle sharedHandle;
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
    int acquire(MemObj *memObj);
    void release(MemObj *memObject);
    virtual ~SharingHandler() = default;

    virtual void getMemObjectInfo(size_t &paramValueSize, void *&paramValue){};
    virtual void releaseReusedGraphicsAllocation(){};

  protected:
    virtual int synchronizeHandler(UpdateData &updateData);
    virtual int validateUpdateData(UpdateData &updateData);
    virtual void synchronizeObject(UpdateData &updateData) { updateData.synchronizationStatus = SYNCHRONIZE_ERROR; }
    virtual void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData);
    virtual void releaseResource(MemObj *memObject){};
    unsigned int acquireCount = 0u;
};
} // namespace NEO
