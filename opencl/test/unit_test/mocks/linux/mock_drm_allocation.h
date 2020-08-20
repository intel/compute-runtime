/*
 * Copyright (C) 2019-2020 Intel Corporation
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
    using BufferObject::bindInfo;
    using BufferObject::BufferObject;
    using BufferObject::handle;

    MockBufferObject(Drm *drm) : BufferObject(drm, 0, 0, 1) {
    }
};

class MockDrmAllocation : public DrmAllocation {
  public:
    using DrmAllocation::bufferObjects;
    using DrmAllocation::memoryPool;

    MockDrmAllocation(AllocationType allocationType, MemoryPool::Type pool) : DrmAllocation(0, allocationType, nullptr, nullptr, 0, static_cast<size_t>(0), pool) {
    }
};

} // namespace NEO
