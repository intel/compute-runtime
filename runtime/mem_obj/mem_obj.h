/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/mem_obj/map_operations_handler.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/sharings/sharing.h"

#include "mem_obj_types.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace NEO {
class ExecutionEnvironment;
class GraphicsAllocation;
struct KernelInfo;
class MemoryManager;
class Context;

template <>
struct OpenCLObjectMapper<_cl_mem> {
    typedef class MemObj DerivedType;
};

class MemObj : public BaseObject<_cl_mem> {
  public:
    constexpr static cl_ulong maskMagic = 0xFFFFFFFFFFFFFF00LL;
    constexpr static cl_ulong objectMagic = 0xAB2212340CACDD00LL;

    MemObj(Context *context,
           cl_mem_object_type memObjectType,
           MemoryProperties properties,
           size_t size,
           void *memoryStorage,
           void *hostPtr,
           GraphicsAllocation *gfxAllocation,
           bool zeroCopy,
           bool isHostPtrSVM,
           bool isObjectRedescrbied);
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
    size_t getSize() const;
    cl_mem_flags getFlags() const;

    bool addMappedPtr(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset, uint32_t mipLevel);
    bool findMappedPtr(void *mappedPtr, MapInfo &outMapInfo) { return mapOperationsHandler.find(mappedPtr, outMapInfo); }
    void removeMappedPtr(void *mappedPtr) { mapOperationsHandler.remove(mappedPtr); }
    void *getBasePtrForMap();

    MOCKABLE_VIRTUAL void setAllocatedMapPtr(void *allocatedMapPtr);
    void *getAllocatedMapPtr() const { return allocatedMapPtr; }

    void setHostPtrMinSize(size_t size);
    void releaseAllocatedMapPtr();
    void releaseMapAllocation();

    bool isMemObjZeroCopy() const;
    bool isMemObjWithHostPtrSVM() const;
    bool isMemObjUncacheable() const;
    virtual void transferDataToHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) { UNRECOVERABLE_IF(true); };
    virtual void transferDataFromHostPtr(MemObjSizeArray &copySize, MemObjOffsetArray &copyOffset) { UNRECOVERABLE_IF(true); };

    GraphicsAllocation *getGraphicsAllocation();
    void resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation);
    GraphicsAllocation *getMcsAllocation() { return mcsAllocation; }
    void setMcsAllocation(GraphicsAllocation *alloc) { mcsAllocation = alloc; }

    bool readMemObjFlagsInvalid();
    bool writeMemObjFlagsInvalid();
    bool mapMemObjFlagsInvalid(cl_map_flags mapFlags);

    virtual bool allowTiling() const { return false; }

    void *getCpuAddressForMapping();
    void *getCpuAddressForMemoryTransfer();

    std::shared_ptr<SharingHandler> &getSharingHandler() { return sharingHandler; }
    SharingHandler *peekSharingHandler() const { return sharingHandler.get(); }
    void setSharingHandler(SharingHandler *sharingHandler) { this->sharingHandler.reset(sharingHandler); }
    void setParentSharingHandler(std::shared_ptr<SharingHandler> &handler) { sharingHandler = handler; }
    unsigned int acquireCount = 0;
    Context *getContext() const { return context; }

    void destroyGraphicsAllocation(GraphicsAllocation *allocation, bool asyncDestroy);
    bool checkIfMemoryTransferIsRequired(size_t offsetInMemObjest, size_t offsetInHostPtr, const void *ptr, cl_command_type cmdType);
    bool mappingOnCpuAllowed() const;
    virtual size_t calculateOffsetForMapping(const MemObjOffsetArray &offset) const { return offset[0]; }
    size_t calculateMappedPtrLength(const MemObjSizeArray &size) const { return calculateOffsetForMapping(size); }
    cl_mem_object_type peekClMemObjType() const { return memObjectType; }
    size_t getOffset() const { return offset; }
    MemoryManager *getMemoryManager() const {
        return memoryManager;
    }
    void setMapAllocation(GraphicsAllocation *allocation) {
        mapAllocation = allocation;
    }
    GraphicsAllocation *getMapAllocation() const {
        if (associatedMemObject) {
            return associatedMemObject->getMapAllocation();
        }
        return mapAllocation;
    }

  protected:
    void getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam);

    Context *context;
    cl_mem_object_type memObjectType;
    MemoryProperties properties;
    size_t size;
    size_t hostPtrMinSize = 0;
    void *memoryStorage;
    void *hostPtr;
    void *allocatedMapPtr = nullptr;
    MapOperationsHandler mapOperationsHandler;
    size_t offset = 0;
    MemObj *associatedMemObject = nullptr;
    cl_uint refCount = 0;
    ExecutionEnvironment *executionEnvironment = nullptr;
    bool isZeroCopy;
    bool isHostPtrSVM;
    bool isObjectRedescribed;
    MemoryManager *memoryManager = nullptr;
    GraphicsAllocation *graphicsAllocation;
    GraphicsAllocation *mcsAllocation = nullptr;
    GraphicsAllocation *mapAllocation = nullptr;
    std::shared_ptr<SharingHandler> sharingHandler;

    class DestructorCallback {
      public:
        DestructorCallback(void(CL_CALLBACK *funcNotify)(cl_mem, void *),
                           void *userData)
            : funcNotify(funcNotify), userData(userData){};

        void invoke(cl_mem memObj);

      private:
        void(CL_CALLBACK *funcNotify)(cl_mem, void *);
        void *userData;
    };

    std::vector<DestructorCallback *> destructorCallbacks;
};
} // namespace NEO
