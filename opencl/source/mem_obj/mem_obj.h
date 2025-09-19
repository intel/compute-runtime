/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/api/cl_types.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/destructor_callbacks.h"
#include "opencl/source/helpers/properties_helper.h"

#include "memory_properties_flags.h"

#include <cstdint>
#include <vector>

namespace NEO {
class SharingHandler;
struct MapInfo;
class MapOperationsHandler;
class ExecutionEnvironment;
class GraphicsAllocation;
struct KernelInfo;
class MemoryManager;
class Context;
class MemObj;

template <>
struct OpenCLObjectMapper<_cl_mem> {
    typedef class MemObj DerivedType;
};

namespace CreateMemObj {
struct AllocationInfo {
    GraphicsAllocation *mapAllocation = nullptr;
    GraphicsAllocation *memory = nullptr;
    AllocationType allocationType = AllocationType::unknown;

    bool zeroCopyAllowed = true;
    bool isHostPtrSVM = false;

    bool alignmentSatisfied = true;
    bool allocateMemory = true;
    bool copyMemoryFromHostPtr = false;

    bool transferNeeded = false;
};
} // namespace CreateMemObj

using AllocationInfoType = StackVec<CreateMemObj::AllocationInfo, 1>;

class MemObj : public BaseObject<_cl_mem> {
  public:
    constexpr static cl_ulong maskMagic = 0xFFFFFFFFFFFFFF00LL;
    constexpr static cl_ulong objectMagic = 0xAB2212340CACDD00LL;

    MemObj(Context *context,
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
           bool isObjectRedescribed);
    ~MemObj() override;

    cl_int getMemObjectInfo(cl_mem_info paramName,
                            size_t paramValueSize,
                            void *paramValue,
                            size_t *paramValueSizeRet);
    cl_int setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_mem, void *),
                                 void *userData);

    void *getCpuAddress() const;
    void *getHostPtr() const;
    bool getIsObjectRedescribed() const { return isObjectRedescribed; };
    MOCKABLE_VIRTUAL size_t getSize() const;

    MapOperationsHandler &getMapOperationsHandler();
    MapOperationsHandler *getMapOperationsHandlerIfExists();
    bool addMappedPtr(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset, uint32_t mipLevel, GraphicsAllocation *graphicsAllocation);
    bool findMappedPtr(void *mappedPtr, MapInfo &outMapInfo);
    void removeMappedPtr(void *mappedPtr);
    void *getBasePtrForMap(uint32_t rootDeviceIndex);

    MOCKABLE_VIRTUAL void setAllocatedMapPtr(void *allocatedMapPtr);
    void *getAllocatedMapPtr() const { return allocatedMapPtr; }

    void setHostPtrMinSize(size_t size);
    void releaseAllocatedMapPtr();
    void releaseMapAllocation(uint32_t rootDeviceIndex, bool asyncDestroy);

    bool isMemObjZeroCopy() const;
    bool isMemObjWithHostPtrSVM() const;
    bool isMemObjUncacheable() const;
    bool isMemObjUncacheableForSurfaceState() const;
    bool isMemObjDisplayable() const;
    virtual void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) { UNRECOVERABLE_IF(true); };
    virtual void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) { UNRECOVERABLE_IF(true); };

    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) const;
    void resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation);
    void removeGraphicsAllocation(uint32_t rootDeviceIndex);
    GraphicsAllocation *getMcsAllocation() { return mcsAllocation; }
    void setMcsAllocation(GraphicsAllocation *alloc) { mcsAllocation = alloc; }

    bool readMemObjFlagsInvalid();
    bool writeMemObjFlagsInvalid();
    bool mapMemObjFlagsInvalid(cl_map_flags mapFlags);

    MOCKABLE_VIRTUAL bool isTiledAllocation() const;

    void *getCpuAddressForMapping();
    void *getCpuAddressForMemoryTransfer();

    std::shared_ptr<SharingHandler> &getSharingHandler() { return sharingHandler; }
    SharingHandler *peekSharingHandler() const { return sharingHandler.get(); }
    void setSharingHandler(SharingHandler *sharingHandler);
    void setParentSharingHandler(std::shared_ptr<SharingHandler> &handler) { sharingHandler = handler; }
    unsigned int acquireCount = 0;
    Context *getContext() const { return context; }

    void destroyGraphicsAllocation(GraphicsAllocation *allocation, bool asyncDestroy);
    bool checkIfMemoryTransferIsRequired(size_t offsetInMemObject, size_t offsetInHostPtr, const void *ptr, cl_command_type cmdType);
    bool mappingOnCpuAllowed() const;
    MOCKABLE_VIRTUAL bool allowCpuAccess() const;
    bool allowCpuForMapUnmap() const;
    virtual size_t calculateOffsetForMapping(const MemObjOffsetArray &offset) const { return offset[0]; }
    size_t calculateMappedPtrLength(const MemObjSizeArray &size) const { return calculateOffsetForMapping(size); }
    cl_mem_object_type peekClMemObjType() const { return memObjectType; }
    size_t getOffset() const { return offset; }
    MemoryManager *getMemoryManager() const {
        return memoryManager;
    }
    void setMapAllocation(GraphicsAllocation *allocation) {
        mapAllocations.addAllocation(allocation);
    }
    GraphicsAllocation *getMapAllocation(uint32_t rootDeviceIndex) const {
        if (associatedMemObject) {
            return associatedMemObject->getMapAllocation(rootDeviceIndex);
        }
        return mapAllocations.getGraphicsAllocation(rootDeviceIndex);
    }

    const cl_mem_flags &getFlags() const { return flags; }
    const cl_mem_flags &getFlagsIntel() const { return flagsIntel; }
    const MultiGraphicsAllocation &getMultiGraphicsAllocation() const { return multiGraphicsAllocation; }
    static void cleanAllGraphicsAllocations(Context &context, MemoryManager &memoryManager, AllocationInfoType &allocationInfo, bool isParentObject);
    MemObj *getHighestRootMemObj() {
        if (!associatedMemObject) {
            return this;
        }
        return associatedMemObject->getHighestRootMemObj();
    }
    MemObj *getAssociatedMemObject() {
        return associatedMemObject;
    }
    void setSizeInPoolAllocator(size_t size) {
        this->sizeInPoolAllocator = size;
    }
    bool getIsUnifiedMcsSurface() {
        return isMcsSurfaceUnified;
    }
    void setIsUnifiedMcsSurface(bool isMcsSurfaceUnified) {
        this->isMcsSurfaceUnified = isMcsSurfaceUnified;
    }

  protected:
    void getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam);
    void storeProperties(const cl_mem_properties *properties);
    void checkUsageAndReleaseOldAllocation(uint32_t rootDeviceIndex);

    Context *context;
    cl_mem_object_type memObjectType;
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    size_t size;
    size_t sizeInPoolAllocator = 0;
    size_t hostPtrMinSize = 0;
    void *memoryStorage;
    void *hostPtr;
    void *allocatedMapPtr = nullptr;
    size_t offset = 0;
    MemObj *associatedMemObject = nullptr;
    cl_uint refCount = 0;
    ExecutionEnvironment *executionEnvironment = nullptr;
    bool isZeroCopy;
    bool isHostPtrSVM;
    bool isObjectRedescribed;
    bool isDisplayable{false};
    bool isMcsSurfaceUnified = false;
    MemoryManager *memoryManager = nullptr;
    MultiGraphicsAllocation multiGraphicsAllocation;
    GraphicsAllocation *mcsAllocation = nullptr;
    MultiGraphicsAllocation mapAllocations;
    std::shared_ptr<SharingHandler> sharingHandler;
    std::vector<uint64_t> propertiesVector;

    MemObjDestructorCallbacks destructorCallbacks;
};
} // namespace NEO
