/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"

namespace NEO {
class Context;
class GmmHelper;
} // namespace NEO

MockBufferStorage::MockBufferStorage(bool unaligned) : mockGfxAllocation(unaligned ? alignUp(&data, 4) : alignUp(&data, 64), sizeof(data) / 2),
                                                       multiGfxAllocation(GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation)) {
    initDevice();
}

void MockBufferStorage::initDevice() {
    VariableBackup<uint32_t> maxOsContextCountBackup(&MemoryManager::maxOsContextCount);
    device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
}

MockBufferStorage::~MockBufferStorage() {
    if (mockGfxAllocation.getDefaultGmm()) {
        delete mockGfxAllocation.getDefaultGmm();
    }
}

void MockBuffer::setAllocationType(uint32_t rootDeviceIndex, bool compressed) {
    setAllocationType(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex),
                      device->getRootDeviceEnvironment().getGmmHelper(), compressed);
}

void MockBuffer::setAllocationType(GraphicsAllocation *graphicsAllocation, GmmHelper *gmmHelper, bool compressed) {
    if (compressed && !graphicsAllocation->getDefaultGmm()) {
        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = compressed;
        graphicsAllocation->setDefaultGmm(new Gmm(gmmHelper, nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    }

    if (graphicsAllocation->getDefaultGmm()) {
        graphicsAllocation->getDefaultGmm()->setCompressionEnabled(compressed);
    }
}

MockBuffer::MockBuffer(Context *context, GraphicsAllocation &alloc)
    : MockBufferStorage(), Buffer(
                               context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                               CL_MEM_USE_HOST_PTR, 0, alloc.getUnderlyingBufferSize(), alloc.getUnderlyingBuffer(), alloc.getUnderlyingBuffer(),
                               GraphicsAllocationHelper::toMultiGraphicsAllocation(&alloc), true, false, false),
      externalAlloc(&alloc) {
}

MockBuffer::MockBuffer()
    : MockBufferStorage(), Buffer(
                               nullptr, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                               CL_MEM_USE_HOST_PTR, 0, sizeof(data), &data, &data,
                               GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), true, false, false) {
}

void MockBuffer::setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) {
    Buffer::setSurfaceState(this->device.get(), memory, forceNonAuxMode, disableL3, getSize(), getCpuAddress(), 0, (externalAlloc != nullptr) ? externalAlloc : &mockGfxAllocation, 0, 0, false);
}

AlignedBuffer::AlignedBuffer() : MockBufferStorage(false), Buffer(
                                                               nullptr, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                                               CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64),
                                                               GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), true, false, false) {
}

AlignedBuffer::AlignedBuffer(Context *context, GraphicsAllocation *gfxAllocation)
    : MockBufferStorage(), Buffer(
                               context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                               CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 64), alignUp(&data, 64),
                               GraphicsAllocationHelper::toMultiGraphicsAllocation(gfxAllocation), true, false, false),
      externalAlloc(gfxAllocation) {
}

void AlignedBuffer::setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) {
    Buffer::setSurfaceState(this->device.get(), memory, forceNonAuxMode, disableL3, getSize(), getCpuAddress(), 0, &mockGfxAllocation, 0, 0, false);
}

UnalignedBuffer::UnalignedBuffer() : MockBufferStorage(true), Buffer(
                                                                  nullptr, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                                                  CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4),
                                                                  GraphicsAllocationHelper::toMultiGraphicsAllocation(&mockGfxAllocation), false, false, false) {
}

UnalignedBuffer::UnalignedBuffer(Context *context, GraphicsAllocation *gfxAllocation)
    : MockBufferStorage(true), Buffer(
                                   context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, MockBufferStorage::device.get()),
                                   CL_MEM_USE_HOST_PTR, 0, sizeof(data) / 2, alignUp(&data, 4), alignUp(&data, 4),
                                   GraphicsAllocationHelper::toMultiGraphicsAllocation(gfxAllocation), false, false, false),
      externalAlloc(gfxAllocation) {
}

void UnalignedBuffer::setArgStateful(void *memory, bool forceNonAuxMode, bool disableL3, bool alignSizeForAuxTranslation, bool isReadOnly, const Device &device, bool areMultipleSubDevicesInContext) {
    Buffer::setSurfaceState(this->device.get(), memory, forceNonAuxMode, disableL3, getSize(), getCpuAddress(), 0, &mockGfxAllocation, 0, 0, false);
}