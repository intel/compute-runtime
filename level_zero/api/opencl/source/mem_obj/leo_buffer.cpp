/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/core/source/driver/driver_handle.h"

namespace NEO {
namespace LEO {

void Buffer::checkUsageAndReleaseOldAllocation(uint32_t rootDeviceIndex) {
    auto graphicsAllocation = getGraphicsAllocation(rootDeviceIndex);
    if (graphicsAllocation->peekReuseCount() == 0) {
        context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->removeSVMAlloc(*this->getAllocData());
        this->allocData = nullptr;
        context->getL0Object()->getDriverHandle()->memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    }
}

void Buffer::resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation) {
    auto lock = this->takeOwnership();

    auto allocData = this->getAllocData();
    auto newAllocData = *allocData;

    checkUsageAndReleaseOldAllocation(newGraphicsAllocation->getRootDeviceIndex());

    newAllocData.gpuAllocations.addAllocation(newGraphicsAllocation);
    newAllocData.setAllocId(++context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->allocationsCounter);
    context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(newAllocData);

    this->allocData = nullptr;
    this->usmPtr = reinterpret_cast<void *>(newGraphicsAllocation->getGpuAddress());
}

void Buffer::refreshDeviceAddress(uint32_t rootDeviceIndex) {
    if (this->isSubBuffer()) {
        static_cast<Buffer *>(this->associatedMemObject)->refreshDeviceAddress(rootDeviceIndex);
        return;
    }

    auto lock = this->takeOwnership();

    auto allocData = this->getAllocData();
    if (nullptr == allocData) {
        return;
    }

    auto graphicsAllocation = allocData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    if (nullptr == graphicsAllocation) {
        return;
    }

    this->usmPtr = reinterpret_cast<void *>(graphicsAllocation->getGpuAddress());
}

void Buffer::removeGraphicsAllocation(uint32_t rootDeviceIndex) {
    auto lock = this->takeOwnership();
    checkUsageAndReleaseOldAllocation(rootDeviceIndex);
    this->getAllocData()->gpuAllocations.removeAllocation(rootDeviceIndex);
}

Buffer *Buffer::createSharedBuffer(Context *context, cl_mem_flags flags, SharingHandler *sharingHandler, MultiGraphicsAllocation &&multiGraphicsAllocation) {
    SvmAllocationData allocData(static_cast<uint32_t>(multiGraphicsAllocation.getGraphicsAllocations().size()));

    for (const auto alloc : multiGraphicsAllocation.getGraphicsAllocations()) {
        allocData.gpuAllocations.addAllocation(alloc);
    }
    allocData.cpuAllocation = nullptr;
    allocData.size = multiGraphicsAllocation.getDefaultGraphicsAllocation()->getUnderlyingBufferSize();
    allocData.device = &context->getClDevice()->getDevice();
    allocData.isInternalAllocation = true;
    allocData.setAllocId(++context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->allocationsCounter);

    context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getClDevice()->getDevice());
    auto sharedBuffer = new Buffer(context,
                                   memoryProperties,
                                   flags,
                                   reinterpret_cast<void *>(multiGraphicsAllocation.getDefaultGraphicsAllocation()->getGpuAddress()),
                                   nullptr,
                                   multiGraphicsAllocation.getDefaultGraphicsAllocation()->getUnderlyingBufferSize(),
                                   false);

    sharedBuffer->setSharingHandler(sharingHandler);
    return sharedBuffer;
}

bool Buffer::isZeroCopyAllowedForHostPtr(const void *hostPtr, size_t size, MemoryManager *memoryManager) {
    if (nullptr == hostPtr) {
        return false;
    }

    if (debugManager.flags.DisableZeroCopyForUseHostPtr.get()) {
        return false;
    }

    // Sharing memory with the GPU requires cache line granularity, otherwise the buffer edges would
    // share cache lines with unrelated CPU data.
    if (!isL3Capable(hostPtr, size)) {
        return false;
    }

    if (auto memRestrictions = memoryManager->getAlignedMallocRestrictions()) {
        if (memRestrictions->minAddress > reinterpret_cast<uintptr_t>(hostPtr)) {
            return false;
        }
    }

    return true;
}

void *Buffer::allocateWithHostPtr(Context *context, void *hostPtr, size_t size, const MemoryProperties &memoryProperties) {
    auto rootDeviceIndex = context->getClDevice()->getRootDeviceIndex();
    auto driverHandle = context->getL0Object()->getDriverHandle();
    auto memoryManager = driverHandle->getMemoryManager();
    auto svmAllocsManager = driverHandle->getSvmAllocsManager();

    auto allocationProperties = MemoryPropertiesHelper::getAllocationProperties(rootDeviceIndex,
                                                                                memoryProperties,
                                                                                false,
                                                                                size,
                                                                                AllocationType::bufferHostMemory,
                                                                                false,
                                                                                context->getClDevice()->getHardwareInfo(),
                                                                                context->getDeviceBitfields().at(rootDeviceIndex),
                                                                                context->isSingleDeviceContext());

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties, hostPtr);
    if (nullptr == allocation) {
        return nullptr;
    }

    SvmAllocationData allocData(rootDeviceIndex);
    allocData.gpuAllocations.addAllocation(allocation);
    allocData.cpuAllocation = nullptr;
    allocData.size = allocation->getAllocationOffset() + size;
    allocData.memoryType = InternalMemoryType::hostUnifiedMemory;
    allocData.allocationFlagsProperty.hostptr = castToUint64(hostPtr);
    allocData.device = nullptr;
    allocData.setAllocId(++svmAllocsManager->allocationsCounter);
    svmAllocsManager->insertSVMAlloc(allocData);

    return reinterpret_cast<void *>(allocation->getGpuAddress());
}

void *Buffer::tryImportUserPtr(Context *context, void *hostPtr, size_t size, const MemoryProperties &memoryProperties, bool preferHostMemory) {
    if (!memoryProperties.flags.useHostPtr || !preferHostMemory) {
        return nullptr;
    }

    auto driverHandle = context->getL0Object()->getDriverHandle();
    if (!isZeroCopyAllowedForHostPtr(hostPtr, size, driverHandle->getMemoryManager())) {
        return nullptr;
    }

    return allocateWithHostPtr(context, hostPtr, size, memoryProperties);
}

Buffer::~Buffer() {
    if (associatedMemObject) {
        associatedMemObject->decRefInternal();
    } else if (this->peekSharingHandler()) {
        peekSharingHandler()->releaseReusedGraphicsAllocation();
        this->checkUsageAndReleaseOldAllocation(this->getContext()->getClDevice()->getRootDeviceIndex());
    } else {
        if (!externalHandle && !usesSvm) {
            ze_memory_free_ext_desc_t freeDesc{ZE_STRUCTURE_TYPE_MEMORY_FREE_EXT_DESC, nullptr, ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE};
            zeMemFreeExt(context->getL0ContextHandle(), &freeDesc, usmPtr);
            if (usmPtr == cpuPtr) {
                this->cpuPtr = nullptr;
            }
        }
    }
}

cl_mem_object_type Buffer::getClObjectType() {
    return CL_MEM_OBJECT_BUFFER;
}

NEO::SvmAllocationData *Buffer::getAllocData() {
    if (!allocData) {
        allocData = this->context->getL0Object()->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(this->getUsmPtr());
    }
    return allocData;
}

cl_int Buffer::createMapAllocation() {
    cl_int ret = CL_SUCCESS;
    if (this->isSubBuffer()) {
        auto parentBuffer = NEO::LEO::castToObject<NEO::LEO::Buffer>(this->associatedMemObject);
        if (nullptr == parentBuffer->getCpuPtr()) {
            ret = parentBuffer->createMapAllocation();
        }
        if (CL_SUCCESS == ret) {
            this->setCpuPtr(ptrOffset(parentBuffer->getCpuPtr(), this->offset));
        }
    } else {
        DEBUG_BREAK_IF(nullptr != this->getCpuPtr());
        ze_host_mem_alloc_desc_t hostDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr, 0};
        void *cpuPtr = nullptr;
        ret = L0ToClResultMapper(zeMemAllocHost(this->context->getL0ContextHandle(),
                                                &hostDesc,
                                                this->getAllocData()->size,
                                                1,
                                                &cpuPtr));
        this->setCpuPtr(cpuPtr);
    }
    return ret;
}

void *Buffer::getUsmPtr() const {
    if (this->isSubBuffer()) {
        auto parentPtr = static_cast<const Buffer *>(this->associatedMemObject)->getUsmPtr();
        return parentPtr ? ptrOffset(parentPtr, this->offset) : nullptr;
    }
    return usmPtr;
}

void **Buffer::getUsmPtrRef() {
    this->usmPtr = this->getUsmPtr();
    return &this->usmPtr;
}

size_t Buffer::getApiSize() const {
    return this->bufferSize;
}

bool Buffer::isCompressionEnabled() {
    return getAllocData()->gpuAllocations.getDefaultGraphicsAllocation()->isCompressionEnabled();
}

bool Buffer::isSubBuffer() const {
    return nullptr != this->associatedMemObject;
}

bool Buffer::isValidSubBufferOffset(size_t offset) {
    if (this->isCompressionEnabled()) {
        // From spec: "origin value is aligned to the CL_DEVICE_MEM_BASE_ADDR_ALIGN value"
        if (!isAligned(offset, context->getClDevice()->getDeviceInfo().memBaseAddressAlign / 8u)) {
            return false;
        }
    }
    return isAligned<4u>(offset);
}

cl_int Buffer::validateSubBufferRegion(const cl_buffer_region *region) {
    if (this->isSubBuffer()) {
        return CL_INVALID_MEM_OBJECT;
    }

    if (!region) {
        return CL_INVALID_VALUE;
    }

    if (region->size == 0) {
        return CL_INVALID_BUFFER_SIZE;
    }

    if (region->origin > this->getApiSize() ||
        region->origin + region->size > this->getApiSize()) {
        return CL_INVALID_VALUE;
    }

    if (!this->isValidSubBufferOffset(region->origin)) {
        return CL_MISALIGNED_SUB_BUFFER_OFFSET;
    }

    return CL_SUCCESS;
}

Buffer *Buffer::createSubBuffer(cl_mem_flags flags,
                                cl_mem_flags_intel flagsIntel,
                                const cl_buffer_region *region) {
    flags = NEO::LEO::Buffer::inheritFlags(flags, this->flags);
    cl_mem_flags_intel emptyFlagsIntel = 0;
    NEO::MemoryProperties memoryProperties =
        NEO::LEO::ClMemoryPropertiesHelper::createMemoryProperties(flags, emptyFlagsIntel, 0, this->properties.pDevice);
    auto subBuffer = new NEO::LEO::Buffer(this->context,
                                          memoryProperties,
                                          flags,
                                          this->usmPtr ? ptrOffset(this->usmPtr, region->origin) : nullptr,
                                          this->cpuPtr ? ptrOffset(this->cpuPtr, region->origin) : nullptr,
                                          region->size,
                                          false);

    this->incRefInternal();
    subBuffer->associatedMemObject = this;
    subBuffer->offset = region->origin;
    subBuffer->setUsesSvm(this->usesSvm);
    subBuffer->setParentSharingHandler(this->getSharingHandler());
    return subBuffer;
}

cl_mem_flags Buffer::inheritFlags(cl_mem_flags flags, cl_mem_flags parentFlags) {
    if ((flags & (CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_READ_WRITE)) == 0) {
        flags |= parentFlags & (CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_READ_WRITE);
    }
    if ((flags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) == 0) {
        flags |= parentFlags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY |
                                CL_MEM_HOST_NO_ACCESS);
    }
    flags |= parentFlags & (CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR);
    return flags;
}

} // namespace LEO
} // namespace NEO
