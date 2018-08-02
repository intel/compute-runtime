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

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"

using namespace OCLRT;

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
    void setArgStateful(void *memory, bool forceNonAuxMode) override {
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
    void setArgStateful(void *memory, bool forceNonAuxMode) override {
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
    void setArgStateful(void *memory, bool forceNonAuxMode) override {
        Buffer::setSurfaceState(device.get(), memory, getSize(), getCpuAddress(), &mockGfxAllocation);
    }
};

class MockPublicAccessBuffer : public Buffer {
  public:
    using Buffer::getGraphicsAllocationType;
};
