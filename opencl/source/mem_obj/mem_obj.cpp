/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/mem_obj.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/heap_allocator.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/sharing.h"

#include <algorithm>

namespace NEO {

MemObj::MemObj(Context *context,
               cl_mem_object_type memObjectType,
               const MemoryProperties &memoryProperties,
               cl_mem_flags flags,
               cl_mem_flags_intel flagsIntel,
               size_t size,
               void *memoryStorage,
               void *hostPtr,
               MultiGraphicsAllocation &&multiGraphicsAllocation,
               bool zeroCopy,
               bool isHostPtrSVM,
               bool isObjectRedescribed)
    : context(context), memObjectType(memObjectType), memoryProperties(memoryProperties), flags(flags), flagsIntel(flagsIntel), size(size),
      memoryStorage(memoryStorage), hostPtr(hostPtr),
      isZeroCopy(zeroCopy), isHostPtrSVM(isHostPtrSVM), isObjectRedescribed(isObjectRedescribed),
      multiGraphicsAllocation(std::move(multiGraphicsAllocation)),
      mapAllocations(static_cast<uint32_t>(this->multiGraphicsAllocation.getGraphicsAllocations().size() - 1)) {
    if (context) {
        context->incRefInternal();
        memoryManager = context->getMemoryManager();
        auto device = context->getDevice(0);
        executionEnvironment = device->getExecutionEnvironment();
    }
}

MemObj::~MemObj() {
    if (!context) {
        return;
    }

    bool needWait = false;

    if (allocatedMapPtr != nullptr) {
        needWait = true;
    }

    if (auto mapOperationsHandler = getMapOperationsHandlerIfExists(); mapOperationsHandler != nullptr) {
        if (mapOperationsHandler->size() > 0 && !getCpuAddressForMapping()) {
            needWait = true;
        }
        context->getMapOperationsStorage().removeHandler(this);
    }

    if (!destructorCallbacks.empty()) {
        needWait = true;
    }

    if (!isObjectRedescribed) {
        if (peekSharingHandler()) {
            peekSharingHandler()->releaseReusedGraphicsAllocation();
        }

        needWait |= multiGraphicsAllocation.getGraphicsAllocations().size() > 1u;
        for (auto graphicsAllocation : multiGraphicsAllocation.getGraphicsAllocations()) {
            auto rootDeviceIndex = graphicsAllocation ? graphicsAllocation->getRootDeviceIndex() : 0;

            bool doAsyncDestructions = debugManager.flags.EnableAsyncDestroyAllocations.get() && !this->memoryProperties.flags.useHostPtr;
            if (graphicsAllocation && !associatedMemObject && !isHostPtrSVM && graphicsAllocation->peekReuseCount() == 0) {
                memoryManager->removeAllocationFromHostPtrManager(graphicsAllocation);
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
                releaseMapAllocation(rootDeviceIndex, doAsyncDestructions);
            }
            if (mcsAllocation) {
                destroyGraphicsAllocation(mcsAllocation, false);
                mcsAllocation = nullptr;
            }
        }
        if (associatedMemObject) {
            associatedMemObject->decRefInternal();
            context->getBufferPoolAllocator().tryFreeFromPoolBuffer(associatedMemObject, this->offset, this->sizeInPoolAllocator);
        }
        if (!associatedMemObject) {
            releaseAllocatedMapPtr();
        }
    }

    destructorCallbacks.invoke(this);

    const bool needDecrementContextRefCount = !context->getBufferPoolAllocator().isPoolBuffer(this);
    if (needDecrementContextRefCount) {
        context->decRefInternal();
    }
}

cl_int MemObj::getMemObjectInfo(cl_mem_info paramName,
                                size_t paramValueSize,
                                void *paramValue,
                                size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t srcParamSize = GetInfo::invalidSourceSize;
    void *srcParam = nullptr;
    cl_bool usesSVMPointer;
    size_t clOffset = 0;
    cl_uint refCnt = 0;
    cl_uint mapCount = 0;
    cl_mem clAssociatedMemObject = static_cast<cl_mem>(this->associatedMemObject);
    cl_context ctx = nullptr;
    uint64_t internalHandle = 0llu;
    auto rootDeviceIndex = context->getDevice(0)->getRootDeviceIndex();
    auto allocation = multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
    cl_bool usesCompression;

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
        clOffset = this->getOffset();
        if (nullptr != this->associatedMemObject) {
            if (this->getContext()->getBufferPoolAllocator().isPoolBuffer(this->associatedMemObject)) {
                clOffset = 0;
            } else {
                clOffset -= this->associatedMemObject->getOffset();
            }
        }
        srcParamSize = sizeof(clOffset);
        srcParam = &clOffset;
        break;

    case CL_MEM_ASSOCIATED_MEMOBJECT:
        if (this->getContext()->getBufferPoolAllocator().isPoolBuffer(this->associatedMemObject)) {
            clAssociatedMemObject = nullptr;
        }
        srcParamSize = sizeof(clAssociatedMemObject);
        srcParam = &clAssociatedMemObject;
        break;

    case CL_MEM_MAP_COUNT:
        srcParamSize = sizeof(mapCount);
        mapCount = static_cast<cl_uint>(getMapOperationsHandler().size());
        srcParam = &mapCount;
        break;

    case CL_MEM_REFERENCE_COUNT:
        refCnt = static_cast<cl_uint>(this->getReference());
        srcParamSize = sizeof(refCnt);
        srcParam = &refCnt;
        break;

    case CL_MEM_ALLOCATION_HANDLE_INTEL: {
        auto retVal = allocation->peekInternalHandle(this->memoryManager, internalHandle);
        if (retVal != 0) {
            return CL_OUT_OF_RESOURCES;
        }
        srcParamSize = sizeof(internalHandle);
        srcParam = &internalHandle;
        break;
    }

    case CL_MEM_USES_COMPRESSION_INTEL:
        usesCompression = allocation->isCompressionEnabled();
        srcParam = &usesCompression;
        srcParamSize = sizeof(cl_bool);
        break;

    case CL_MEM_PROPERTIES:
        srcParamSize = propertiesVector.size() * sizeof(cl_mem_properties);
        srcParam = propertiesVector.data();
        break;

    default:
        getOsSpecificMemObjectInfo(paramName, &srcParamSize, &srcParam);
        break;
    }

    auto getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, srcParam, srcParamSize);
    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, srcParamSize, getInfoStatus);

    return retVal;
}

cl_int MemObj::setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_mem, void *),
                                     void *userData) {
    std::unique_lock<std::mutex> theLock(mtx);
    destructorCallbacks.add(funcNotify, userData);
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

bool MemObj::isMemObjDisplayable() const {
    return this->isDisplayable;
}

GraphicsAllocation *MemObj::getGraphicsAllocation(uint32_t rootDeviceIndex) const {
    return multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex);
}

void MemObj::checkUsageAndReleaseOldAllocation(uint32_t rootDeviceIndex) {
    auto graphicsAllocation = getGraphicsAllocation(rootDeviceIndex);
    if (graphicsAllocation != nullptr && (peekSharingHandler() == nullptr || graphicsAllocation->peekReuseCount() == 0)) {
        memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    }
}

void MemObj::resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation) {
    TakeOwnershipWrapper<MemObj> lock(*this);
    checkUsageAndReleaseOldAllocation(newGraphicsAllocation->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(newGraphicsAllocation);
}

void MemObj::removeGraphicsAllocation(uint32_t rootDeviceIndex) {
    TakeOwnershipWrapper<MemObj> lock(*this);
    checkUsageAndReleaseOldAllocation(rootDeviceIndex);
    multiGraphicsAllocation.removeAllocation(rootDeviceIndex);
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

void MemObj::setSharingHandler(SharingHandler *sharingHandler) {
    this->sharingHandler.reset(sharingHandler);
}

void MemObj::releaseAllocatedMapPtr() {
    if (allocatedMapPtr) {
        DEBUG_BREAK_IF(isValueSet(flags, CL_MEM_USE_HOST_PTR));
        memoryManager->freeSystemMemory(allocatedMapPtr);
    }
    allocatedMapPtr = nullptr;
}

void MemObj::releaseMapAllocation(uint32_t rootDeviceIndex, bool asyncDestroy) {
    auto mapAllocation = mapAllocations.getGraphicsAllocation(rootDeviceIndex);
    if (mapAllocation && !isHostPtrSVM) {
        if (asyncDestroy && !isValueSet(flags, CL_MEM_USE_HOST_PTR)) {
            destroyGraphicsAllocation(mapAllocation, true);
        } else {
            if (mapAllocation->isUsed()) {
                memoryManager->waitForEnginesCompletion(*mapAllocation);
            }
            destroyGraphicsAllocation(mapAllocation, false);
        }
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
    if (getFlags() & CL_MEM_USE_HOST_PTR) {
        return getHostPtr();
    } else {
        TakeOwnershipWrapper<MemObj> memObjOwnership(*this);
        if (getMapAllocation(rootDeviceIndex)) {
            return getMapAllocation(rootDeviceIndex)->getUnderlyingBuffer();
        } else {
            auto memory = getAllocatedMapPtr();
            if (!memory) {
                memory = memoryManager->allocateSystemMemory(getSize(), MemoryConstants::pageSize);
                setAllocatedMapPtr(memory);
            }
            AllocationProperties properties{rootDeviceIndex,
                                            false, // allocateMemory
                                            getSize(), AllocationType::mapAllocation,
                                            false, // isMultiStorageAllocation
                                            context->getDeviceBitfieldForAllocation(rootDeviceIndex)};

            auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, memory);
            setMapAllocation(allocation);
            return getAllocatedMapPtr();
        }
    }
}

MapOperationsHandler &MemObj::getMapOperationsHandler() {
    return context->getMapOperationsStorage().getHandler(this);
}

MapOperationsHandler *MemObj::getMapOperationsHandlerIfExists() {
    return context->getMapOperationsStorage().getHandlerIfExists(this);
}

bool MemObj::addMappedPtr(void *ptr, size_t ptrLength, cl_map_flags &mapFlags,
                          MemObjSizeArray &size, MemObjOffsetArray &offset,
                          uint32_t mipLevel, GraphicsAllocation *graphicsAllocation) {
    return getMapOperationsHandler().add(ptr, ptrLength, mapFlags, size, offset, mipLevel, graphicsAllocation);
}

bool MemObj::findMappedPtr(void *mappedPtr, MapInfo &outMapInfo) {
    return getMapOperationsHandler().find(mappedPtr, outMapInfo);
}

void MemObj::removeMappedPtr(void *mappedPtr) {
    getMapOperationsHandler().remove(mappedPtr);
}

bool MemObj::isTiledAllocation() const {
    auto graphicsAllocation = multiGraphicsAllocation.getDefaultGraphicsAllocation();
    auto gmm = graphicsAllocation->getDefaultGmm();
    return gmm && (gmm->gmmResourceInfo->getTileModeSurfaceState() != 0);
}

bool MemObj::mappingOnCpuAllowed() const {
    auto graphicsAllocation = multiGraphicsAllocation.getDefaultGraphicsAllocation();
    return !isTiledAllocation() && !peekSharingHandler() && !isMipMapped(this) && !debugManager.flags.DisableZeroCopyForBuffers.get() &&
           !graphicsAllocation->isCompressionEnabled() && MemoryPoolHelper::isSystemMemoryPool(graphicsAllocation->getMemoryPool()) &&
           allowCpuForMapUnmap();
}

bool MemObj::allowCpuAccess() const {
    auto graphicsAllocation = this->multiGraphicsAllocation.getDefaultGraphicsAllocation();
    if (graphicsAllocation->getDefaultGmm() == nullptr) {
        return true;
    }

    return !graphicsAllocation->getDefaultGmm()->getPreferNoCpuAccess();
}

bool MemObj::allowCpuForMapUnmap() const {
    auto ret = allowCpuAccess();
    if (debugManager.flags.AllowZeroCopyWithoutCoherency.get() != -1) {
        ret = debugManager.flags.AllowZeroCopyWithoutCoherency.get();
    }
    return ret;
}

void MemObj::storeProperties(const cl_mem_properties *properties) {
    if (properties) {
        for (size_t i = 0; properties[i] != 0; i += 2) {
            propertiesVector.push_back(properties[i]);
            propertiesVector.push_back(properties[i + 1]);
        }
        propertiesVector.push_back(0);
    }
}

void MemObj::cleanAllGraphicsAllocations(Context &context, MemoryManager &memoryManager, AllocationInfoType &allocationInfo, bool isParentObject) {
    if (!isParentObject) {
        for (auto &index : context.getRootDeviceIndices()) {
            if (allocationInfo[index].memory) {
                memoryManager.removeAllocationFromHostPtrManager(allocationInfo[index].memory);
                memoryManager.freeGraphicsMemory(allocationInfo[index].memory);
            }
        }
    }
}

} // namespace NEO
