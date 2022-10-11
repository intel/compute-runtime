/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

class MockBufferStorage {
  public:
    MockBufferStorage() : mockGfxAllocation(data, sizeof(data) / 2),
                          multiGfxAllocation(GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation)) {
        initDevice();
    }

    MockBufferStorage(bool unaligned) : mockGfxAllocation(unaligned ? alignUp(&data, 4) : alignUp(&data, 64), sizeof(data) / 2),
                                        multiGfxAllocation(GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation)) {
        initDevice();
    }
    void initDevice() {
        VariableBackup<uint32_t> maxOsContextCountBackup(&MemoryManager::maxOsContextCount);
        device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    }
    ~MockBufferStorage() {
        if (mockGfxAllocation.getDefaultGmm()) {
            delete mockGfxAllocation.getDefaultGmm();
        }
    }
    char data[128];
    MockGraphicsAllocation mockGfxAllocation;
    std::unique_ptr<MockDevice> device;
    MultiGraphicsAllocation multiGfxAllocation;
};

class MockBuffer : public MockBufferStorage, public Buffer {
  public:
    using Buffer::magic;
    using Buffer::offset;
    using Buffer::size;
    using MemObj::associatedMemObject;
    using MemObj::context;
    using MemObj::isZeroCopy;
    using MemObj::memObjectType;
    using MockBufferStorage::device;

    void setAllocationType(uint32_t rootDeviceIndex, bool compressed) {
        setAllocationType(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex),
                          device->getRootDeviceEnvironment().getGmmHelper(), compressed);
    }

    static void setAllocationType(GraphicsAllocation *graphicsAllocation, GmmHelper *gmmHelper, bool compressed) {
        if (compressed && !graphicsAllocation->getDefaultGmm()) {
            graphicsAllocation->setDefaultGmm(new Gmm(gmmHelper, nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, compressed, {}, true));
        }

        if (graphicsAllocation->getDefaultGmm()) {
            graphicsAllocation->getDefaultGmm()->isCompressionEnabled = compressed;
        }
    }

    MockBuffer(GraphicsAllocation &alloc) : MockBuffer(nullptr, alloc) {}

    MockBuffer(Context *context, GraphicsAllocation &alloc)
        : MockBufferStorage(), Buffer(
                                   context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                   CL_MEM_USE_HOST_PTR, 0, alloc.getUnderlyingBufferSize(), alloc.getUnderlyingBuffer(), alloc.getUnderlyingBuffer(),
                                   GraphicsAllocationHelper::toMultiGraphicsAllocation(&alloc), true, false, false),
          externalAlloc(&alloc) {
    }

    MockBuffer()
        : MockBufferStorage(), Buffer(
                                   nullptr, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                   CL_MEM_USE_HOST_PTR, 0, sizeof(data), &data, &data,
                                   GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), true, false, false) {
    }

    ~MockBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation, do not release it
            this->multiGraphicsAllocation.removeAllocation(0u);
        }
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool useGlobalAtomics, bool areMultipleSubDevicesInContext) override {
        Buffer::setSurfaceState(this->device.get(), memory, forceNonAuxMode, disableL3, getSize(), getCpuAddress(), 0, (externalAlloc != nullptr) ? externalAlloc : &mockGfxAllocation, 0, 0, false, false);
    }

    void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override {
        ++transferDataToHostPtrCalledCount;

        if (callBaseTransferDataToHostPtr) {
            Buffer::transferDataToHostPtr(copySize, copyOffset);
        }
    }

    void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) override {
        ++transferDataFromHostPtrCalledCount;

        if (callBaseTransferDataFromHostPtr) {
            Buffer::transferDataFromHostPtr(copySize, copyOffset);
        }
    }

    GraphicsAllocation *externalAlloc = nullptr;

    bool callBaseTransferDataToHostPtr{true};
    bool callBaseTransferDataFromHostPtr{true};

    int transferDataToHostPtrCalledCount{0};
    int transferDataFromHostPtrCalledCount{0};
};

class AlignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;

    AlignedBuffer() : MockBufferStorage(false), Buffer(
                                                    nullptr, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                                    CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64),
                                                    GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), true, false, false) {
    }

    AlignedBuffer(GraphicsAllocation *gfxAllocation) : AlignedBuffer(nullptr, gfxAllocation) {}

    AlignedBuffer(Context *context, GraphicsAllocation *gfxAllocation)
        : MockBufferStorage(), Buffer(
                                   context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                   CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64),
                                   GraphicsAllocationHelper::toMultiGraphicsAllocation(gfxAllocation), true, false, false),
          externalAlloc(gfxAllocation) {
    }

    ~AlignedBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation, do not release it
            this->multiGraphicsAllocation.removeAllocation(0u);
        }
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool useGlobalAtomics, bool areMultipleSubDevicesInContext) override {
        Buffer::setSurfaceState(this->device.get(), memory, forceNonAuxMode, disableL3, getSize(), getCpuAddress(), 0, &mockGfxAllocation, 0, 0, false, false);
    }

    GraphicsAllocation *externalAlloc = nullptr;
};

class UnalignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;

    UnalignedBuffer() : MockBufferStorage(true), Buffer(
                                                     nullptr, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                                     CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4),
                                                     GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), false, false, false) {
    }

    UnalignedBuffer(GraphicsAllocation *gfxAllocation) : UnalignedBuffer(nullptr, gfxAllocation) {}

    UnalignedBuffer(Context *context, GraphicsAllocation *gfxAllocation)
        : MockBufferStorage(true), Buffer(
                                       context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                       CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4),
                                       GraphicsAllocationHelper::toMultiGraphicsAllocation(gfxAllocation), false, false, false),
          externalAlloc(gfxAllocation) {
    }

    ~UnalignedBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation, do not release it
            this->multiGraphicsAllocation.removeAllocation(0u);
        }
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool useGlobalAtomics, bool areMultipleSubDevicesInContext) override {
        Buffer::setSurfaceState(this->device.get(), memory, forceNonAuxMode, disableL3, getSize(), getCpuAddress(), 0, &mockGfxAllocation, 0, 0, false, false);
    }

    GraphicsAllocation *externalAlloc = nullptr;
};

class MockPublicAccessBuffer : public Buffer {
  public:
    using Buffer::getGraphicsAllocationTypeAndCompressionPreference;
};
