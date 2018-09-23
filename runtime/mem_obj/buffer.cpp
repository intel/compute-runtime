/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/validators.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {

BufferFuncs bufferFactory[IGFX_MAX_CORE] = {};

Buffer::Buffer(Context *context,
               cl_mem_flags flags,
               size_t size,
               void *memoryStorage,
               void *hostPtr,
               GraphicsAllocation *gfxAllocation,
               bool zeroCopy,
               bool isHostPtrSVM,
               bool isObjectRedescribed)
    : MemObj(context,
             CL_MEM_OBJECT_BUFFER,
             flags,
             size,
             memoryStorage,
             hostPtr,
             gfxAllocation,
             zeroCopy,
             isHostPtrSVM,
             isObjectRedescribed) {
    magic = objectMagic;
    setHostPtrMinSize(size);
}

Buffer::Buffer() : MemObj(nullptr, CL_MEM_OBJECT_BUFFER, 0, 0, nullptr, nullptr, nullptr, false, false, false) {
}

Buffer::~Buffer() = default;

bool Buffer::isSubBuffer() {
    return this->associatedMemObject != nullptr;
}

bool Buffer::isValidSubBufferOffset(size_t offset) {
    for (size_t i = 0; i < context->getNumDevices(); ++i) {
        cl_uint address_align = 32; // 4 byte alignment
        if ((offset & (address_align / 8 - 1)) == 0) {
            return true;
        }
    }
    return false;
}

Buffer *Buffer::create(Context *context,
                       cl_mem_flags flags,
                       size_t size,
                       void *hostPtr,
                       cl_int &errcodeRet) {
    Buffer *pBuffer = nullptr;
    errcodeRet = CL_SUCCESS;

    GraphicsAllocation *memory = nullptr;
    bool zeroCopyAllowed = true;
    bool isHostPtrSVM = false;

    bool alignementSatisfied = true;
    bool allocateMemory = true;
    bool copyMemoryFromHostPtr = false;
    GraphicsAllocation::AllocationType allocationType = getGraphicsAllocationType(
        flags,
        context->isSharedContext,
        context->getDevice(0)->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);

    MemoryManager *memoryManager = context->getMemoryManager();
    UNRECOVERABLE_IF(!memoryManager);

    checkMemory(flags, size, hostPtr, errcodeRet, alignementSatisfied, copyMemoryFromHostPtr, memoryManager);

    if (errcodeRet != CL_SUCCESS) {
        return nullptr;
    }

    if (allocationType == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) {
        zeroCopyAllowed = false;
        allocateMemory = true;
        if (flags & CL_MEM_USE_HOST_PTR) {
            copyMemoryFromHostPtr = true;
        }
    }

    if (allocationType == GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY) {
        if (flags & CL_MEM_ALLOC_HOST_PTR) {
            zeroCopyAllowed = true;
            allocateMemory = true;
        } else if (flags & CL_MEM_USE_HOST_PTR) {
            allocateMemory = false;
            if (!alignementSatisfied || DebugManager.flags.DisableZeroCopyForUseHostPtr.get()) {
                zeroCopyAllowed = false;
                allocateMemory = true;
            }
        }
    }

    if (context->isSharedContext) {
        zeroCopyAllowed = true;
        copyMemoryFromHostPtr = false;
        allocateMemory = false;
    }

    if (flags & CL_MEM_USE_HOST_PTR) {
        memory = context->getSVMAllocsManager()->getSVMAlloc(hostPtr);
        if (memory) {
            allocationType = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
            isHostPtrSVM = true;
            zeroCopyAllowed = true;
            copyMemoryFromHostPtr = false;
            allocateMemory = false;
        }
    }

    if (hostPtr && context->isProvidingPerformanceHints()) {
        if (zeroCopyAllowed) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_BUFFER_MEETS_ALIGNMENT_RESTRICTIONS, hostPtr, size);
        } else {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, hostPtr, size, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    if (DebugManager.flags.DisableZeroCopyForBuffers.get()) {
        zeroCopyAllowed = false;
    }

    if (!memory) {
        AllocationFlags allocFlags = MemObjHelper::getAllocationFlags(flags);
        DevicesBitfield devices = MemObjHelper::getDevicesBitfield(flags);
        allocFlags.flags.allocateMemory = allocateMemory;
        memory = memoryManager->allocateGraphicsMemoryInPreferredPool(allocFlags, devices, hostPtr, static_cast<size_t>(size), allocationType);
    }

    if (allocateMemory) {
        if (memory) {
            memoryManager->addAllocationToHostPtrManager(memory);
        }
        if (context->isProvidingPerformanceHints()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_BUFFER_NEEDS_ALLOCATE_MEMORY);
        }
    } else {
        if (!memory && Buffer::isReadOnlyMemoryPermittedByFlags(flags)) {
            allocationType = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
            zeroCopyAllowed = false;
            copyMemoryFromHostPtr = true;
            AllocationFlags allocFlags = MemObjHelper::getAllocationFlags(flags);
            DevicesBitfield devices = MemObjHelper::getDevicesBitfield(flags);
            allocFlags.flags.allocateMemory = true;
            memory = memoryManager->allocateGraphicsMemoryInPreferredPool(allocFlags, devices, nullptr, static_cast<size_t>(size), allocationType);
        }
    }

    if (!memory) {
        errcodeRet = CL_OUT_OF_HOST_MEMORY;
        return nullptr;
    }

    if (!MemoryPool::isSystemMemoryPool(memory->getMemoryPool())) {
        zeroCopyAllowed = false;
    } else if (allocationType == GraphicsAllocation::AllocationType::BUFFER) {
        allocationType = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
    }

    memory->setAllocationType(allocationType);
    memory->setMemObjectsAllocationWithWritableFlags(!(flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)));

    DBG_LOG(LogMemoryObject, __FUNCTION__, "hostPtr:", hostPtr, "size:", size, "memoryStorage:", memory->getUnderlyingBuffer(), "GPU address:", std::hex, memory->getGpuAddress());

    pBuffer = createBufferHw(context,
                             flags,
                             size,
                             memory->getUnderlyingBuffer(),
                             const_cast<void *>(hostPtr),
                             memory,
                             zeroCopyAllowed,
                             isHostPtrSVM,
                             false);
    if (!pBuffer) {
        errcodeRet = CL_OUT_OF_HOST_MEMORY;
        memoryManager->removeAllocationFromHostPtrManager(memory);
        memoryManager->freeGraphicsMemory(memory);
        return nullptr;
    }

    pBuffer->setHostPtrMinSize(size);

    if (copyMemoryFromHostPtr) {
        if (memory->gmm && memory->gmm->isRenderCompressed) {
            auto cmdQ = context->getSpecialQueue();
            if (CL_SUCCESS != cmdQ->enqueueWriteBuffer(pBuffer, CL_TRUE, 0, size, hostPtr, 0, nullptr, nullptr)) {
                errcodeRet = CL_OUT_OF_RESOURCES;
            }
        } else {
            memcpy_s(memory->getUnderlyingBuffer(), size, hostPtr, size);
        }
    }

    if (errcodeRet != CL_SUCCESS) {
        pBuffer->release();
        return nullptr;
    }

    return pBuffer;
}

Buffer *Buffer::createSharedBuffer(Context *context, cl_mem_flags flags, SharingHandler *sharingHandler,
                                   GraphicsAllocation *graphicsAllocation) {
    auto sharedBuffer = createBufferHw(context, flags, graphicsAllocation->getUnderlyingBufferSize(), nullptr, nullptr, graphicsAllocation, false, false, false);

    sharedBuffer->setSharingHandler(sharingHandler);
    return sharedBuffer;
}

void Buffer::checkMemory(cl_mem_flags flags,
                         size_t size,
                         void *hostPtr,
                         cl_int &errcodeRet,
                         bool &alignementSatisfied,
                         bool &copyMemoryFromHostPtr,
                         MemoryManager *memMngr) {
    errcodeRet = CL_SUCCESS;
    alignementSatisfied = true;
    copyMemoryFromHostPtr = false;
    uintptr_t minAddress = 0;
    auto memRestrictions = memMngr->getAlignedMallocRestrictions();
    if (memRestrictions) {
        minAddress = memRestrictions->minAddress;
    }

    if (flags & CL_MEM_USE_HOST_PTR) {
        if (hostPtr) {
            auto fragment = memMngr->hostPtrManager.getFragment(hostPtr);
            if (fragment && fragment->driverAllocation) {
                errcodeRet = CL_INVALID_HOST_PTR;
                return;
            }
            if (alignUp(hostPtr, MemoryConstants::cacheLineSize) != hostPtr ||
                alignUp(size, MemoryConstants::cacheLineSize) != size ||
                minAddress > reinterpret_cast<uintptr_t>(hostPtr)) {
                alignementSatisfied = false;
                copyMemoryFromHostPtr = true;
            }
        } else {
            errcodeRet = CL_INVALID_HOST_PTR;
        }
    }

    if (flags & CL_MEM_COPY_HOST_PTR) {
        if (hostPtr) {
            copyMemoryFromHostPtr = true;
        } else {
            errcodeRet = CL_INVALID_HOST_PTR;
        }
    }
    return;
}

GraphicsAllocation::AllocationType Buffer::getGraphicsAllocationType(cl_mem_flags flags, bool sharedContext, bool renderCompressedBuffers) {
    GraphicsAllocation::AllocationType type = GraphicsAllocation::AllocationType::BUFFER;
    if (renderCompressedBuffers) {
        type = GraphicsAllocation::AllocationType::BUFFER_COMPRESSED;
    } else if ((flags & CL_MEM_USE_HOST_PTR) || (flags & CL_MEM_ALLOC_HOST_PTR)) {
        type = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
    }

    if (sharedContext) {
        type = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
    }
    return type;
}

bool Buffer::isReadOnlyMemoryPermittedByFlags(cl_mem_flags flags) {
    // Host won't access or will only read and kernel will only read
    if ((flags & (CL_MEM_HOST_NO_ACCESS | CL_MEM_HOST_READ_ONLY)) && (flags & CL_MEM_READ_ONLY)) {
        return true;
    }
    return false;
}

Buffer *Buffer::createSubBuffer(cl_mem_flags flags,
                                const cl_buffer_region *region,
                                cl_int &errcodeRet) {
    DEBUG_BREAK_IF(nullptr == createFunction);
    auto buffer = createFunction(this->context, flags, region->size,
                                 ptrOffset(this->memoryStorage, region->origin),
                                 this->hostPtr ? ptrOffset(this->hostPtr, region->origin) : nullptr,
                                 this->graphicsAllocation,
                                 this->isZeroCopy, this->isHostPtrSVM, true);

    if (this->context->isProvidingPerformanceHints()) {
        this->context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, SUBBUFFER_SHARES_MEMORY, static_cast<cl_mem>(this));
    }

    buffer->associatedMemObject = this;
    buffer->offset = region->origin;
    buffer->setParentSharingHandler(this->getSharingHandler());
    this->incRefInternal();

    errcodeRet = CL_SUCCESS;
    return buffer;
}

uint64_t Buffer::setArgStateless(void *memory, uint32_t patchSize, bool set32BitAddressing) {
    // Subbuffers have offset that graphicsAllocation is not aware of
    uintptr_t addressToPatch = ((set32BitAddressing) ? static_cast<uintptr_t>(graphicsAllocation->getGpuAddressToPatch()) : static_cast<uintptr_t>(graphicsAllocation->getGpuAddress())) + this->offset;
    DEBUG_BREAK_IF(!(graphicsAllocation->isLocked() || (addressToPatch != 0) || (graphicsAllocation->gpuBaseAddress != 0) ||
                     (this->getCpuAddress() == nullptr && this->getGraphicsAllocation()->peekSharedHandle())));

    patchWithRequiredSize(memory, patchSize, addressToPatch);

    return addressToPatch;
}

bool Buffer::bufferRectPitchSet(const size_t *bufferOrigin,
                                const size_t *region,
                                size_t &bufferRowPitch,
                                size_t &bufferSlicePitch,
                                size_t &hostRowPitch,
                                size_t &hostSlicePitch) {
    if (bufferRowPitch == 0)
        bufferRowPitch = region[0];
    if (bufferSlicePitch == 0)
        bufferSlicePitch = region[1] * bufferRowPitch;

    if (hostRowPitch == 0)
        hostRowPitch = region[0];
    if (hostSlicePitch == 0)
        hostSlicePitch = region[1] * hostRowPitch;

    if (bufferRowPitch < region[0] ||
        hostRowPitch < region[0]) {
        return false;
    }
    if ((bufferSlicePitch < region[1] * bufferRowPitch || bufferSlicePitch % bufferRowPitch != 0) ||
        (hostSlicePitch < region[1] * hostRowPitch || hostSlicePitch % hostRowPitch != 0)) {
        return false;
    }

    if ((bufferOrigin[2] + region[2] - 1) * bufferSlicePitch + (bufferOrigin[1] + region[1] - 1) * bufferRowPitch + bufferOrigin[0] + region[0] > this->getSize()) {
        return false;
    }
    return true;
}

void Buffer::transferData(void *dst, void *src, size_t copySize, size_t copyOffset) {
    DBG_LOG(LogMemoryObject, __FUNCTION__, " hostPtr: ", hostPtr, ", size: ", copySize, ", offset: ", copyOffset, ", memoryStorage: ", memoryStorage);
    auto dstPtr = ptrOffset(dst, copyOffset);
    auto srcPtr = ptrOffset(src, copyOffset);
    memcpy_s(dstPtr, copySize, srcPtr, copySize);
}

void Buffer::transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(hostPtr, memoryStorage, copySize[0], copyOffset[0]);
}

void Buffer::transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) {
    transferData(memoryStorage, hostPtr, copySize[0], copyOffset[0]);
}

size_t Buffer::calculateHostPtrSize(const size_t *origin, const size_t *region, size_t rowPitch, size_t slicePitch) {
    size_t hostPtrOffsetInBytes = origin[2] * slicePitch + origin[1] * rowPitch + origin[0];
    size_t hostPtrRegionSizeInbytes = region[0] + rowPitch * (region[1] - 1) + slicePitch * (region[2] - 1);
    size_t hostPtrSize = hostPtrOffsetInBytes + hostPtrRegionSizeInbytes;
    return hostPtrSize;
}

bool Buffer::isReadWriteOnCpuAllowed(cl_bool blocking, cl_uint numEventsInWaitList, void *ptr, size_t size) {
    return (blocking == CL_TRUE && numEventsInWaitList == 0 && !forceDisallowCPUCopy) && graphicsAllocation->peekSharedHandle() == 0 &&
           (isMemObjZeroCopy() || (reinterpret_cast<uintptr_t>(ptr) & (MemoryConstants::cacheLineSize - 1)) != 0) &&
           (!context->getDevice(0)->getDeviceInfo().platformLP || (size <= maxBufferSizeForReadWriteOnCpu)) &&
           !(graphicsAllocation->gmm && graphicsAllocation->gmm->isRenderCompressed);
}

Buffer *Buffer::createBufferHw(Context *context,
                               cl_mem_flags flags,
                               size_t size,
                               void *memoryStorage,
                               void *hostPtr,
                               GraphicsAllocation *gfxAllocation,
                               bool zeroCopy,
                               bool isHostPtrSVM,
                               bool isImageRedescribed) {
    const auto device = context->getDevice(0);
    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = bufferFactory[hwInfo.pPlatform->eRenderCoreFamily].createBufferFunction;
    DEBUG_BREAK_IF(nullptr == funcCreate);
    auto pBuffer = funcCreate(context, flags, size, memoryStorage, hostPtr, gfxAllocation,
                              zeroCopy, isHostPtrSVM, isImageRedescribed);
    DEBUG_BREAK_IF(nullptr == pBuffer);
    if (pBuffer) {
        pBuffer->createFunction = funcCreate;
    }
    return pBuffer;
}

Buffer *Buffer::createBufferHwFromDevice(const Device *device,
                                         cl_mem_flags flags,
                                         size_t size,
                                         void *memoryStorage,
                                         void *hostPtr,
                                         GraphicsAllocation *gfxAllocation,
                                         bool zeroCopy,
                                         bool isHostPtrSVM,
                                         bool isImageRedescribed) {

    const auto &hwInfo = device->getHardwareInfo();

    auto funcCreate = bufferFactory[hwInfo.pPlatform->eRenderCoreFamily].createBufferFunction;
    DEBUG_BREAK_IF(nullptr == funcCreate);
    auto pBuffer = funcCreate(nullptr, flags, size, memoryStorage, hostPtr, gfxAllocation,
                              zeroCopy, isHostPtrSVM, isImageRedescribed);
    pBuffer->executionEnvironment = device->getExecutionEnvironment();
    return pBuffer;
}

void Buffer::setSurfaceState(const Device *device,
                             void *surfaceState,
                             size_t svmSize,
                             void *svmPtr,
                             GraphicsAllocation *gfxAlloc,
                             cl_mem_flags flags) {
    auto buffer = Buffer::createBufferHwFromDevice(device, flags, svmSize, svmPtr, svmPtr, gfxAlloc, false, false, false);
    buffer->setArgStateful(surfaceState, false);
    buffer->graphicsAllocation = nullptr;
    delete buffer;
}
} // namespace OCLRT
