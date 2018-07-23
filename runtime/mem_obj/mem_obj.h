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

#pragma once
#include "runtime/api/cl_types.h"
#include "runtime/helpers/base_object.h"
#include "runtime/helpers/completion_stamp.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/sharings/sharing.h"
#include "runtime/mem_obj/map_operations_handler.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace OCLRT {
class GraphicsAllocation;
struct KernelInfo;
class MemoryManager;
class Context;
class CommandQueue;
class Device;

template <>
struct OpenCLObjectMapper<_cl_mem> {
    typedef class MemObj DerivedType;
};

class MemObj : public BaseObject<_cl_mem> {
  public:
    const static cl_ulong maskMagic = 0xFFFFFFFFFFFFFF00LL;
    static const cl_ulong objectMagic = 0xAB2212340CACDD00LL;

    MemObj(Context *context,
           cl_mem_object_type memObjectType,
           cl_mem_flags flags,
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
    void setCompletionStamp(CompletionStamp completionStamp, Device *pDevice, CommandQueue *pCmdQ);
    CompletionStamp getCompletionStamp() const;

    bool addMappedPtr(void *ptr, size_t ptrLength, cl_map_flags &mapFlags, MemObjSizeArray &size, MemObjOffsetArray &offset, uint32_t mipLevel);
    bool findMappedPtr(void *mappedPtr, MapInfo &outMapInfo) { return mapOperationsHandler.find(mappedPtr, outMapInfo); }
    void removeMappedPtr(void *mappedPtr) { return mapOperationsHandler.remove(mappedPtr); }
    void *getBasePtrForMap();

    MOCKABLE_VIRTUAL void setAllocatedMapPtr(void *allocatedMapPtr);
    void *getAllocatedMapPtr() const { return allocatedMapPtr; }

    void setHostPtrMinSize(size_t size);
    void releaseAllocatedMapPtr();

    bool isMemObjZeroCopy() const;
    bool isMemObjWithHostPtrSVM() const;
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

    CommandQueue *getAssociatedCommandQueue() { return cmdQueuePtr; }
    bool isImageFromImage() const { return isImageFromImageCreated; }

    void *getCpuAddressForMapping();
    void *getCpuAddressForMemoryTransfer();

    std::shared_ptr<SharingHandler> &getSharingHandler() { return sharingHandler; }
    SharingHandler *peekSharingHandler() const { return sharingHandler.get(); }
    void setSharingHandler(SharingHandler *sharingHandler) { this->sharingHandler.reset(sharingHandler); }
    void setParentSharingHandler(std::shared_ptr<SharingHandler> &handler) { sharingHandler = handler; }
    unsigned int acquireCount = 0;
    Context *getContext() const { return context; }

    void waitForCsrCompletion();
    void destroyGraphicsAllocation(GraphicsAllocation *allocation, bool asyncDestroy);
    bool checkIfMemoryTransferIsRequired(size_t offsetInMemObjest, size_t offsetInHostPtr, const void *ptr, cl_command_type cmdType);
    bool mappingOnCpuAllowed() const;
    virtual size_t calculateOffsetForMapping(const MemObjOffsetArray &offset) const { return offset[0]; }
    size_t calculateMappedPtrLength(const MemObjSizeArray &size) const { return calculateOffsetForMapping(size); }
    cl_mem_object_type peekClMemObjType() const { return memObjectType; }
    size_t getOffset() const { return offset; }

  protected:
    void getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam);

    Context *context;
    cl_mem_object_type memObjectType;
    cl_mem_flags flags;
    size_t size;
    size_t hostPtrMinSize = 0;
    void *memoryStorage;
    void *hostPtr;
    void *allocatedMapPtr = nullptr;
    MapOperationsHandler mapOperationsHandler;
    size_t offset = 0;
    MemObj *associatedMemObject = nullptr;
    cl_uint refCount = 0;
    CompletionStamp completionStamp;
    CommandQueue *cmdQueuePtr = nullptr;
    Device *device = nullptr;
    bool isZeroCopy;
    bool isHostPtrSVM;
    bool isObjectRedescribed;
    bool isImageFromImageCreated = false;
    MemoryManager *memoryManager = nullptr;
    GraphicsAllocation *graphicsAllocation;
    GraphicsAllocation *mcsAllocation = nullptr;
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
} // namespace OCLRT
