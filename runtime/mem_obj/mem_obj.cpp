/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj.h"

#include "common/helpers/bit_helpers.h"
#include "core/gmm_helper/gmm.h"
#include "core/gmm_helper/resource_info.h"
#include "core/helpers/aligned_memory.h"
#include "core/memory_manager/deferred_deleter.h"
#include "core/os_interface/os_context.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/helpers/get_info.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"

#include <algorithm>

namespace NEO {

MemObj::MemObj(Context *context,
               cl_mem_object_type memObjectType,
               const MemoryPropertiesFlags &memoryProperties,
               cl_mem_flags flags,
               cl_mem_flags_intel flagsIntel,
               size_t size,
               void *memoryStorage,
               void *hostPtr,
               GraphicsAllocation *gfxAllocation,
               bool zeroCopy,
               bool isHostPtrSVM,
               bool isObjectRedescribed)
    : context(context), memObjectType(memObjectType), memoryProperties(memoryProperties), flags(flags), flagsIntel(flagsIntel), size(size),
      memoryStorage(memoryStorage), hostPtr(hostPtr),
      isZeroCopy(zeroCopy), isHostPtrSVM(isHostPtrSVM), isObjectRedescribed(isObjectRedescribed),
      graphicsAllocation(gfxAllocation) {

    if (context) {
        context->incRefInternal();
        memoryManager = context->getMemoryManager();
        executionEnvironment = context->getDevice(0)->getExecutionEnvironment();
    }
}

MemObj::~MemObj() {
    bool needWait = false;
    if (allocatedMapPtr != nullptr) {
        needWait = true;
    }
    if (mapOperationsHandler.size() > 0 && !getCpuAddressForMapping()) {
        needWait = true;
    }
    if (!destructorCallbacks.empty()) {
        needWait = true;
    }

    if (memoryManager && !isObjectRedescribed) {
        if (peekSharingHandler()) {
            peekSharingHandler()->releaseReusedGraphicsAllocation();
        }
        if (graphicsAllocation && !associatedMemObject && !isHostPtrSVM && graphicsAllocation->peekReuseCount() == 0) {
            memoryManager->removeAllocationFromHostPtrManager(graphicsAllocation);
            bool doAsyncDestructions = DebugManager.flags.EnableAsyncDestroyAllocations.get();
            if (!doAsyncDestructions) {
                needWait = true;
            }
            if (needWait && graphicsAllocation->isUsed()) {
                memoryManager->waitForEnginesCompletion(*graphicsAllocation);
            }
            destroyGraphicsAllocation(graphicsAllocation, doAsyncDestructions);
            graphicsAllocation = nullptr;
        }

        if (!associatedMemObject) {
            releaseMapAllocation();
            releaseAllocatedMapPtr();
        }
        if (mcsAllocation) {
            destroyGraphicsAllocation(mcsAllocation, false);
        }

        if (associatedMemObject) {
            if (associatedMemObject->getGraphicsAllocation() != this->getGraphicsAllocation()) {
                destroyGraphicsAllocation(graphicsAllocation, false);
            }
            associatedMemObject->decRefInternal();
        }
    }
    if (!destructorCallbacks.empty()) {
        for (auto iter = destructorCallbacks.rbegin(); iter != destructorCallbacks.rend(); iter++) {
            (*iter)->invoke(this);
            delete *iter;
        }
    }

    if (context) {
        context->decRefInternal();
    }
}

void MemObj::DestructorCallback::invoke(cl_mem memObj) {
    this->funcNotify(memObj, userData);
}

cl_int MemObj::getMemObjectInfo(cl_mem_info paramName,
                                size_t paramValueSize,
                                void *paramValue,
                                size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t srcParamSize = 0;
    void *srcParam = nullptr;
    cl_bool usesSVMPointer;
    cl_uint refCnt = 0;
    cl_uint mapCount = 0;
    cl_mem clAssociatedMemObject = static_cast<cl_mem>(this->associatedMemObject);
    cl_context ctx = nullptr;
    uint64_t internalHandle = 0llu;

    switch (paramName) {
    case CL_MEM_TYPE:
        srcParamSize = sizeof(memObjectType);
        srcParam = &memObjectType;
        break;

    case CL_MEM_FLAGS:
        srcParamSize = sizeof(flags);
        srcParam = &flags;
        break;

    case CL_MEM_SIZE:
        srcParamSize = sizeof(size);
        srcParam = &size;
        break;

    case CL_MEM_HOST_PTR:
        srcParamSize = sizeof(hostPtr);
        srcParam = &hostPtr;
        break;

    case CL_MEM_CONTEXT:
        srcParamSize = sizeof(context);
        ctx = context;
        srcParam = &ctx;
        break;

    case CL_MEM_USES_SVM_POINTER:
        usesSVMPointer = isHostPtrSVM && isValueSet(flags, CL_MEM_USE_HOST_PTR);
        srcParamSize = sizeof(cl_bool);
        srcParam = &usesSVMPointer;
        break;

    case CL_MEM_OFFSET:
        srcParamSize = sizeof(offset);
        srcParam = &offset;
        break;

    case CL_MEM_ASSOCIATED_MEMOBJECT:
        srcParamSize = sizeof(clAssociatedMemObject);
        srcParam = &clAssociatedMemObject;
        break;

    case CL_MEM_MAP_COUNT:
        srcParamSize = sizeof(mapCount);
        mapCount = static_cast<cl_uint>(mapOperationsHandler.size());
        srcParam = &mapCount;
        break;

    case CL_MEM_REFERENCE_COUNT:
        refCnt = static_cast<cl_uint>(this->getReference());
        srcParamSize = sizeof(refCnt);
        srcParam = &refCnt;
        break;
    case CL_MEM_ALLOCATION_HANDLE_INTEL:
        internalHandle = this->getGraphicsAllocation()->peekInternalHandle(this->memoryManager);
        srcParamSize = sizeof(internalHandle);
        srcParam = &internalHandle;
        break;

    default:
        getOsSpecificMemObjectInfo(paramName, &srcParamSize, &srcParam);
        break;
    }

    retVal = ::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);

    if (paramValueSizeRet) {
        *paramValueSizeRet = srcParamSize;
    }

    return retVal;
}

cl_int MemObj::setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_mem, void *),
                                     void *userData) {
    auto cb = new DestructorCallback(funcNotify, userData);

    std::unique_lock<std::mutex> theLock(mtx);
    destructorCallbacks.push_back(cb);
    return CL_SUCCESS;
}

void *MemObj::getCpuAddress() const {
    return memoryStorage;
}

void *MemObj::getHostPtr() const {
    return hostPtr;
}

size_t MemObj::getSize() const {
    return size;
}

void MemObj::setAllocatedMapPtr(void *allocatedMapPtr) {
    this->allocatedMapPtr = allocatedMapPtr;
}

bool MemObj::isMemObjZeroCopy() const {
    return isZeroCopy;
}

bool MemObj::isMemObjWithHostPtrSVM() const {
    return isHostPtrSVM;
}

bool MemObj::isMemObjUncacheable() const {
    return isValueSet(flagsIntel, CL_MEM_LOCALLY_UNCACHED_RESOURCE);
}

bool MemObj::isMemObjUncacheableForSurfaceState() const {
    return isAnyBitSet(flagsIntel, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE | CL_MEM_LOCALLY_UNCACHED_RESOURCE);
}

GraphicsAllocation *MemObj::getGraphicsAllocation() const {
    return graphicsAllocation;
}

void MemObj::resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation) {
    TakeOwnershipWrapper<MemObj> lock(*this);

    if (graphicsAllocation != nullptr && (peekSharingHandler() == nullptr || graphicsAllocation->peekReuseCount() == 0)) {
        memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    }

    graphicsAllocation = newGraphicsAllocation;
}

bool MemObj::readMemObjFlagsInvalid() {
    return isValueSet(flags, CL_MEM_HOST_WRITE_ONLY) || isValueSet(flags, CL_MEM_HOST_NO_ACCESS);
}

bool MemObj::writeMemObjFlagsInvalid() {
    return isValueSet(flags, CL_MEM_HOST_READ_ONLY) || isValueSet(flags, CL_MEM_HOST_NO_ACCESS);
}

bool MemObj::mapMemObjFlagsInvalid(cl_map_flags mapFlags) {
    return (writeMemObjFlagsInvalid() && (mapFlags & CL_MAP_WRITE)) ||
           (readMemObjFlagsInvalid() && (mapFlags & CL_MAP_READ));
}

void MemObj::setHostPtrMinSize(size_t size) {
    hostPtrMinSize = size;
}

void *MemObj::getCpuAddressForMapping() {
    void *ptrToReturn = nullptr;
    if (isValueSet(flags, CL_MEM_USE_HOST_PTR)) {
        ptrToReturn = this->hostPtr;
    } else {
        ptrToReturn = this->memoryStorage;
    }
    return ptrToReturn;
}
void *MemObj::getCpuAddressForMemoryTransfer() {
    void *ptrToReturn = nullptr;
    if (isValueSet(flags, CL_MEM_USE_HOST_PTR) && this->isMemObjZeroCopy()) {
        ptrToReturn = this->hostPtr;
    } else {
        ptrToReturn = this->memoryStorage;
    }
    return ptrToReturn;
}
void MemObj::releaseAllocatedMapPtr() {
    if (allocatedMapPtr) {
        DEBUG_BREAK_IF(isValueSet(flags, CL_MEM_USE_HOST_PTR));
        memoryManager->freeSystemMemory(allocatedMapPtr);
    }
    allocatedMapPtr = nullptr;
}

void MemObj::releaseMapAllocation() {
    if (mapAllocation && !isHostPtrSVM) {
        destroyGraphicsAllocation(mapAllocation, false);
    }
}

void MemObj::destroyGraphicsAllocation(GraphicsAllocation *allocation, bool asyncDestroy) {
    if (asyncDestroy) {
        memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(allocation);
    } else {
        memoryManager->freeGraphicsMemory(allocation);
    }
}

bool MemObj::checkIfMemoryTransferIsRequired(size_t offsetInMemObject, size_t offsetInHostPtr, const void *hostPtr, cl_command_type cmdType) {
    auto bufferStorage = ptrOffset(this->getCpuAddressForMemoryTransfer(), offsetInMemObject);
    auto hostStorage = ptrOffset(hostPtr, offsetInHostPtr);
    auto isMemTransferNeeded = !((bufferStorage == hostStorage) &&
                                 (cmdType == CL_COMMAND_WRITE_BUFFER || cmdType == CL_COMMAND_READ_BUFFER ||
                                  cmdType == CL_COMMAND_WRITE_BUFFER_RECT || cmdType == CL_COMMAND_READ_BUFFER_RECT ||
                                  cmdType == CL_COMMAND_WRITE_IMAGE || cmdType == CL_COMMAND_READ_IMAGE));
    return isMemTransferNeeded;
}

void *MemObj::getBasePtrForMap(uint32_t rootDeviceIndex) {
    if (associatedMemObject) {
        return associatedMemObject->getBasePtrForMap(rootDeviceIndex);
    }
    if (getMemoryPropertiesFlags() & CL_MEM_USE_HOST_PTR) {
        return getHostPtr();
    } else {
        TakeOwnershipWrapper<MemObj> memObjOwnership(*this);
        if (getMapAllocation()) {
            return getMapAllocation()->getUnderlyingBuffer();
        } else {
            auto memory = memoryManager->allocateSystemMemory(getSize(), MemoryConstants::pageSize);
            setAllocatedMapPtr(memory);
            AllocationProperties properties{rootDeviceIndex, false, getSize(), GraphicsAllocation::AllocationType::MAP_ALLOCATION, false};

            auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, memory);
            setMapAllocation(allocation);
            return getAllocatedMapPtr();
        }
    }
}

bool MemObj::addMappedPtr(void *ptr, size_t ptrLength, cl_map_flags &mapFlags,
                          MemObjSizeArray &size, MemObjOffsetArray &offset,
                          uint32_t mipLevel) {
    return mapOperationsHandler.add(ptr, ptrLength, mapFlags, size, offset,
                                    mipLevel);
}

bool MemObj::isTiledAllocation() const {
    auto gmm = graphicsAllocation->getDefaultGmm();
    return gmm && (gmm->gmmResourceInfo->getTileModeSurfaceState() != 0);
}

bool MemObj::mappingOnCpuAllowed() const {
    return !isTiledAllocation() && !peekSharingHandler() && !isMipMapped(this) && !DebugManager.flags.DisableZeroCopyForBuffers.get() &&
           !(graphicsAllocation->getDefaultGmm() && graphicsAllocation->getDefaultGmm()->isRenderCompressed) &&
           MemoryPool::isSystemMemoryPool(graphicsAllocation->getMemoryPool());
}
} // namespace NEO
