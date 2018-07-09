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

#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/validators.h"
#include "runtime/helpers/string.h"
#include "runtime/gmm_helper/gmm.h"
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
    bool zeroCopy = false;
    bool isHostPtrSVM = false;
    bool allocateMemory = false;
    bool copyMemoryFromHostPtr = false;

    MemoryManager *memoryManager = context->getMemoryManager();
    UNRECOVERABLE_IF(!memoryManager);

    checkMemory(flags, size, hostPtr, errcodeRet, zeroCopy, allocateMemory, copyMemoryFromHostPtr, memoryManager);

    if (hostPtr && context->isProvidingPerformanceHints()) {
        if (zeroCopy) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_BUFFER_MEETS_ALIGNMENT_RESTRICTIONS, hostPtr, size);
        } else {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL, CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS, hostPtr, size, MemoryConstants::pageSize, MemoryConstants::pageSize);
        }
    }

    if (context->isSharedContext) {
        zeroCopy = true;
        copyMemoryFromHostPtr = false;
        allocateMemory = false;
    }
    if (errcodeRet == CL_SUCCESS) {
        while (true) {
            if (flags & CL_MEM_USE_HOST_PTR) {
                memory = context->getSVMAllocsManager()->getSVMAlloc(hostPtr);
                if (memory) {
                    zeroCopy = true;
                    isHostPtrSVM = true;
                    copyMemoryFromHostPtr = false;
                    allocateMemory = false;
                }
            }

            if (!memory) {
                memory = memoryManager->allocateGraphicsMemoryInPreferredPool(zeroCopy, allocateMemory, true, false, hostPtr, static_cast<size_t>(size), GraphicsAllocation::AllocationType::BUFFER);
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
                    zeroCopy = false;
                    copyMemoryFromHostPtr = true;
                    allocateMemory = true;
                    memory = memoryManager->allocateGraphicsMemoryInPreferredPool(zeroCopy, allocateMemory, true, false, nullptr, static_cast<size_t>(size), GraphicsAllocation::AllocationType::BUFFER);
                }
            }

            if (!memory) {
                errcodeRet = CL_OUT_OF_HOST_MEMORY;
                break;
            }

            auto allocationType = (flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS))
                                      ? GraphicsAllocation::AllocationType::BUFFER
                                      : GraphicsAllocation::AllocationType::BUFFER | GraphicsAllocation::AllocationType::WRITABLE;
            memory->setAllocationType(allocationType);

            DBG_LOG(LogMemoryObject, __FUNCTION__, "hostPtr:", hostPtr, "size:", size, "memoryStorage:", memory->getUnderlyingBuffer(), "GPU address:", std::hex, memory->getGpuAddress());

            if (copyMemoryFromHostPtr) {
                memcpy_s(memory->getUnderlyingBuffer(), size, hostPtr, size);
            }

            pBuffer = createBufferHw(context,
                                     flags,
                                     size,
                                     memory->getUnderlyingBuffer(),
                                     const_cast<void *>(hostPtr),
                                     memory,
                                     zeroCopy,
                                     isHostPtrSVM,
                                     false);
            if (!pBuffer && allocateMemory) {
                memoryManager->removeAllocationFromHostPtrManager(memory);
                memoryManager->freeGraphicsMemory(memory);
                memory = nullptr;
            }

            if (pBuffer) {
                pBuffer->setHostPtrMinSize(size);
            }
            break;
        }
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
                         bool &isZeroCopy,
                         bool &allocateMemory,
                         bool &copyMemoryFromHostPtr,
                         MemoryManager *memMngr) {
    errcodeRet = CL_SUCCESS;
    isZeroCopy = false;
    allocateMemory = false;
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
                minAddress > reinterpret_cast<uintptr_t>(hostPtr) ||
                DebugManager.flags.DisableZeroCopyForUseHostPtr.get() ||
                DebugManager.flags.DisableZeroCopyForBuffers.get()) {
                allocateMemory = true;
                isZeroCopy = false;
                copyMemoryFromHostPtr = true;
            } else {
                isZeroCopy = true;
            }
        } else {
            errcodeRet = CL_INVALID_HOST_PTR;
        }
    } else {
        allocateMemory = true;
        isZeroCopy = true;
        if (DebugManager.flags.DisableZeroCopyForBuffers.get()) {
            isZeroCopy = false;
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
    DEBUG_BREAK_IF(!(graphicsAllocation->isLocked() || (this->getCpuAddress() == reinterpret_cast<void *>(addressToPatch)) || (graphicsAllocation->gpuBaseAddress != 0) || (this->getCpuAddress() == nullptr && this->getGraphicsAllocation()->peekSharedHandle())));

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
    auto pContext = castToObject<Context>(context);
    DEBUG_BREAK_IF(nullptr == pContext);
    const auto device = pContext->getDevice(0);
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
    return pBuffer;
}

void Buffer::setSurfaceState(const Device *device,
                             void *surfaceState,
                             size_t svmSize,
                             void *svmPtr,
                             GraphicsAllocation *gfxAlloc,
                             cl_mem_flags flags) {
    auto buffer = Buffer::createBufferHwFromDevice(device, flags, svmSize, svmPtr, svmPtr, gfxAlloc, false, false, false);
    buffer->setArgStateful(surfaceState);
    buffer->graphicsAllocation = nullptr;
    delete buffer;
}
} // namespace OCLRT
