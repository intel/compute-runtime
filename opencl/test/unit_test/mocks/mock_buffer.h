/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/unit_test/mocks/mock_device.h"

#include "opencl/source/helpers/memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

using namespace NEO;

class MockBufferStorage {
  public:
    MockBufferStorage() : mockGfxAllocation(data, sizeof(data) / 2),
                          multiGfxAllocation(GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation)) {
    }
    MockBufferStorage(bool unaligned) : mockGfxAllocation(unaligned ? alignUp(&data, 4) : alignUp(&data, 64), sizeof(data) / 2),
                                        multiGfxAllocation(GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation)) {
    }
    char data[128];
    MockGraphicsAllocation mockGfxAllocation;
    std::unique_ptr<MockDevice> device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MultiGraphicsAllocation multiGfxAllocation;
};

class MockBuffer : public MockBufferStorage, public Buffer {
  public:
    using Buffer::magic;
    using Buffer::offset;
    using Buffer::size;
    using MemObj::isZeroCopy;
    using MockBufferStorage::device;
    MockBuffer(GraphicsAllocation &alloc)
        : MockBufferStorage(), Buffer(
                                   nullptr, MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, nullptr), CL_MEM_USE_HOST_PTR, 0, alloc.getUnderlyingBufferSize(), alloc.getUnderlyingBuffer(), alloc.getUnderlyingBuffer(),
                                   GraphicsAllocationHelper::toMultiGraphicsAllocation(&alloc), true, false, false),
          externalAlloc(&alloc) {
    }
    MockBuffer()
        : MockBufferStorage(), Buffer(
                                   nullptr, MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, nullptr), CL_MEM_USE_HOST_PTR, 0, sizeof(data), &data, &data,
                                   GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), true, false, false) {
    }
    ~MockBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation
            // return to mock allocations
            this->multiGraphicsAllocation.addAllocation(&this->mockGfxAllocation);
        }
    }
    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device) override {
        Buffer::setSurfaceState(this->device.get(), memory, getSize(), getCpuAddress(), 0, (externalAlloc != nullptr) ? externalAlloc : &mockGfxAllocation, 0, 0);
    }
    GraphicsAllocation *externalAlloc = nullptr;
};

class AlignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;

    AlignedBuffer() : MockBufferStorage(false), Buffer(
                                                    nullptr, MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, nullptr), CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64),
                                                    GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), true, false, false) {
    }
    AlignedBuffer(GraphicsAllocation *gfxAllocation) : MockBufferStorage(), Buffer(
                                                                                nullptr, MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, nullptr), CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64),
                                                                                GraphicsAllocationHelper::toMultiGraphicsAllocation(gfxAllocation), true, false, false) {
    }
    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device) override {
        Buffer::setSurfaceState(this->device.get(), memory, getSize(), getCpuAddress(), 0, &mockGfxAllocation, 0, 0);
    }
};

class UnalignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;

    UnalignedBuffer() : MockBufferStorage(true), Buffer(
                                                     nullptr, MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, nullptr), CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4),
                                                     GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), false, false, false) {
    }
    UnalignedBuffer(GraphicsAllocation *gfxAllocation) : MockBufferStorage(true), Buffer(
                                                                                      nullptr, MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, nullptr), CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4),
                                                                                      GraphicsAllocationHelper::toMultiGraphicsAllocation(gfxAllocation), false, false, false) {
    }
    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device) override {
        Buffer::setSurfaceState(this->device.get(), memory, getSize(), getCpuAddress(), 0, &mockGfxAllocation, 0, 0);
    }
};

class MockPublicAccessBuffer : public Buffer {
  public:
    using Buffer::getGraphicsAllocationType;
};
