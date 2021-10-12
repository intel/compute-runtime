/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"

namespace NEO {

class MockBufferObject : public BufferObject {
  public:
    using BufferObject::bindExtHandles;
    using BufferObject::bindInfo;
    using BufferObject::BufferObject;
    using BufferObject::handle;

    MockBufferObject(Drm *drm) : BufferObject(drm, 0, 0, 1) {
    }
};

class MockDrmAllocation : public DrmAllocation {
  public:
    using DrmAllocation::bufferObjects;
    using DrmAllocation::enabledMemAdviseFlags;
    using DrmAllocation::memoryPool;
    using DrmAllocation::registeredBoBindHandles;

    MockDrmAllocation(AllocationType allocationType, MemoryPool::Type pool) : DrmAllocation(0, allocationType, nullptr, nullptr, 0, static_cast<size_t>(0), pool) {
    }

    void registerBOBindExtHandle(Drm *drm) override {
        registerBOBindExtHandleCalled = true;
        DrmAllocation::registerBOBindExtHandle(drm);
    }

    void markForCapture() override {
        markedForCapture = true;
        DrmAllocation::markForCapture();
    }

    bool registerBOBindExtHandleCalled = false;
    bool markedForCapture = false;
};

} // namespace NEO
