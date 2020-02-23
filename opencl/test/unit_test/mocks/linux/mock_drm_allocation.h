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
    using BufferObject::handle;

    MockBufferObject() : BufferObject(nullptr, 0, 0) {
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
