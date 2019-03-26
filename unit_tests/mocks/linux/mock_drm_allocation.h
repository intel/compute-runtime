/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"

namespace NEO {

class MockBufferObject : public BufferObject {
  public:
    using BufferObject::handle;

    MockBufferObject() : BufferObject(nullptr, 0, false) {
    }
};

class MockDrmAllocation : public DrmAllocation {
  public:
    using DrmAllocation::bo;
    using DrmAllocation::memoryPool;

    MockDrmAllocation(AllocationType allocationType, MemoryPool::Type pool) : DrmAllocation(allocationType, nullptr, nullptr, 0, static_cast<size_t>(0), pool, false) {
    }
};

} // namespace NEO
