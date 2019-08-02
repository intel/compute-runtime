/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

using namespace NEO;

class MockBufferStorage {
  public:
    MockBufferStorage() : mockGfxAllocation(data, sizeof(data) / 2) {
    }
    MockBufferStorage(bool unaligned) : mockGfxAllocation(unaligned ? alignUp(&data, 4) : alignUp(&data, 64), sizeof(data) / 2) {
    }
    char data[128];
    MockGraphicsAllocation mockGfxAllocation;
    std::unique_ptr<Device> device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
};

class MockBuffer : public MockBufferStorage, public Buffer {
  public:
    using Buffer::magic;
    using Buffer::offset;
    using MemObj::isZeroCopy;
    using MockBufferStorage::device;

    MockBuffer(GraphicsAllocation &alloc)
        : MockBufferStorage(), Buffer(nullptr, CL_MEM_USE_HOST_PTR, alloc.getUnderlyingBufferSize(), alloc.getUnderlyingBuffer(), alloc.getUnderlyingBuffer(), &alloc, true, false, false),
          externalAlloc(&alloc) {
    }
    MockBuffer() : MockBufferStorage(), Buffer(nullptr, CL_MEM_USE_HOST_PTR, sizeof(data), &data, &data, &mockGfxAllocation, true, false, false) {
    }
    ~MockBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation
            // return to mock allocations
            this->graphicsAllocation = &this->mockGfxAllocation;
        }
    }
    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3Cache, bool isReadOnly) override {
        Buffer::setSurfaceState(device.get(), memory, getSize(), getCpuAddress(), (externalAlloc != nullptr) ? externalAlloc : &mockGfxAllocation);
    }
    GraphicsAllocation *externalAlloc = nullptr;
};

class AlignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;
    AlignedBuffer() : MockBufferStorage(false), Buffer(nullptr, CL_MEM_USE_HOST_PTR, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64), &mockGfxAllocation, true, false, false) {
    }
    AlignedBuffer(GraphicsAllocation *gfxAllocation) : MockBufferStorage(), Buffer(nullptr, CL_MEM_USE_HOST_PTR, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64), gfxAllocation, true, false, false) {
    }
    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3Cache, bool isReadOnly) override {
        Buffer::setSurfaceState(device.get(), memory, getSize(), getCpuAddress(), &mockGfxAllocation);
    }
};

class UnalignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;
    UnalignedBuffer() : MockBufferStorage(true), Buffer(nullptr, CL_MEM_USE_HOST_PTR, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4), &mockGfxAllocation, false, false, false) {
    }
    UnalignedBuffer(GraphicsAllocation *gfxAllocation) : MockBufferStorage(true), Buffer(nullptr, CL_MEM_USE_HOST_PTR, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4), gfxAllocation, false, false, false) {
    }
    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3Cache, bool isReadOnly) override {
        Buffer::setSurfaceState(device.get(), memory, getSize(), getCpuAddress(), &mockGfxAllocation);
    }
};

class MockPublicAccessBuffer : public Buffer {
  public:
    using Buffer::getGraphicsAllocationType;
};
