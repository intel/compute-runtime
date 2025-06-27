/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/mem_obj/buffer.h"

#include <optional>

namespace NEO {
class MockDevice;
class Context;
class GmmHelper;
class GraphicsAllocation;
} // namespace NEO

using namespace NEO;

class MockBufferStorage {
  public:
    MockBufferStorage() : mockGfxAllocation(data, sizeof(data) / 2),
                          multiGfxAllocation(GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation)) {
        initDevice();
    }

    MockBufferStorage(bool unaligned);
    void initDevice();
    ~MockBufferStorage();
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
    using MemObj::hostPtr;
    using MemObj::isZeroCopy;
    using MemObj::memObjectType;
    using MemObj::memoryStorage;
    using MemObj::sizeInPoolAllocator;
    using MockBufferStorage::device;

    void setAllocationType(uint32_t rootDeviceIndex, bool compressed);

    static void setAllocationType(GraphicsAllocation *graphicsAllocation, GmmHelper *gmmHelper, bool compressed);

    MockBuffer(GraphicsAllocation &alloc) : MockBuffer(nullptr, alloc) {}

    MockBuffer(Context *context, GraphicsAllocation &alloc);

    MockBuffer();

    ~MockBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation, do not release it
            this->multiGraphicsAllocation.removeAllocation(0u);
        }
    }

    bool allowCpuAccess() const override {
        if (allowCpuAccessReturnValue.has_value()) {
            return *allowCpuAccessReturnValue;
        }
        return Buffer::allowCpuAccess();
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) override;

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

    std::optional<bool> allowCpuAccessReturnValue{};
};

class AlignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;

    AlignedBuffer();

    AlignedBuffer(GraphicsAllocation *gfxAllocation) : AlignedBuffer(nullptr, gfxAllocation) {}

    AlignedBuffer(Context *context, GraphicsAllocation *gfxAllocation);

    ~AlignedBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation, do not release it
            this->multiGraphicsAllocation.removeAllocation(0u);
        }
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) override;

    GraphicsAllocation *externalAlloc = nullptr;
};

class UnalignedBuffer : public MockBufferStorage, public Buffer {
  public:
    using MockBufferStorage::device;

    UnalignedBuffer();

    UnalignedBuffer(GraphicsAllocation *gfxAllocation) : UnalignedBuffer(nullptr, gfxAllocation) {}

    UnalignedBuffer(Context *context, GraphicsAllocation *gfxAllocation);

    ~UnalignedBuffer() override {
        if (externalAlloc != nullptr) {
            // no ownership over graphics allocation, do not release it
            this->multiGraphicsAllocation.removeAllocation(0u);
        }
    }

    void setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) override;

    GraphicsAllocation *externalAlloc = nullptr;
};

class MockPublicAccessBuffer : public Buffer {
  public:
    using Buffer::getGraphicsAllocationTypeAndCompressionPreference;
};
