/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/surface_formats.h"
#include "level_zero/api/opencl/source/mem_obj/map_operations_handler.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_mem> {
    typedef class MemObj DerivedType;
};

class MemObj : public BaseObject<_cl_mem> {
  public:
    constexpr static cl_ulong objectMagic = 0xAB2212340CACDD00LL;

    using CallbackT = void(CL_CALLBACK *)(cl_mem, void *);

    enum class MemObjType {
        buffer,
        image,
    };

    MemObj(Context *context, MemoryProperties &properties, cl_mem_flags flags, void *cpuPtr, bool externalHandle, MemObjType memObjType);
    MemObj() = delete;
    ~MemObj() override;

    cl_int getMemObjectInfo(cl_mem_info paramName,
                            size_t paramValueSize,
                            void *paramValue,
                            size_t *paramValueSizeRet);

    void getOsSpecificMemObjectInfo(const cl_mem_info &paramName, size_t *srcParamSize, void **srcParam);

    template <typename ReturnType>
    static ReturnType getMemObjProperties(const cl_mem_properties *properties,
                                          cl_mem_properties propertyName,
                                          bool *foundValue = nullptr) {
        if (properties != nullptr) {
            while (*properties != 0) {
                if (*properties == propertyName) {
                    if (foundValue) {
                        *foundValue = true;
                    }
                    return static_cast<ReturnType>(*(properties + 1));
                }
                properties += 2;
            }
        }

        if (foundValue) {
            *foundValue = false;
        }
        return 0;
    }

    void storeProperties(const cl_mem_properties *properties);
    void setUsesSvm(bool usesSvm) { this->usesSvm = usesSvm; }
    void addCallback(CallbackT callback, void *userData) {
        this->callbacks.emplace_back(callback, userData);
    }

    cl_mem_flags getFlags() const { return this->flags; };
    void *getCpuPtr() const { return this->cpuPtr; }
    void setCpuPtr(void *cpuPtr) { this->cpuPtr = cpuPtr; }

    bool isImage() const { return this->memObjType == MemObjType::image; }
    bool isBuffer() const { return this->memObjType == MemObjType::buffer; }
    bool isSubBuffer() const { return this->isBuffer() && this->associatedMemObject != nullptr; }
    Context *getContext() const { return this->context; };

    MapOperationsHandler &getMapOperationsHandler() { return this->mapOperationHandler; }

    virtual cl_mem_object_type getClObjectType() = 0;
    virtual size_t getApiSize() const = 0;
    virtual bool isCompressionEnabled() = 0;

    virtual GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) = 0;
    virtual void resetGraphicsAllocation(GraphicsAllocation *newGraphicsAllocation) = 0;
    virtual void removeGraphicsAllocation(uint32_t rootDeviceIndex) = 0;

    std::shared_ptr<SharingHandler> &getSharingHandler() { return sharingHandler; };
    SharingHandler *peekSharingHandler() const { return sharingHandler.get(); };
    void setSharingHandler(SharingHandler *sharingHandler) { this->sharingHandler.reset(sharingHandler); };
    void setParentSharingHandler(std::shared_ptr<SharingHandler> &handler) { sharingHandler = handler; };
    unsigned int acquireCount = 0;
    bool mapMemObjFlagsInvalid(cl_map_flags mapFlags);
    bool readMemObjFlagsInvalid();
    bool writeMemObjFlagsInvalid();

  protected:
    virtual void checkUsageAndReleaseOldAllocation(uint32_t rootDeviceIndex) = 0;

    MapOperationsHandler mapOperationHandler{};
    std::vector<std::pair<CallbackT, void *>> callbacks{};

    MemoryProperties properties{};

    cl_mem_flags flags = 0;
    std::vector<cl_mem_properties> propertiesVector;

    std::shared_ptr<SharingHandler> sharingHandler;

    Context *context = nullptr;
    void *cpuPtr = nullptr;
    size_t offset = 0u;
    MemObj *associatedMemObject = nullptr;
    const bool externalHandle = false;
    bool usesSvm = false;
    const MemObjType memObjType;
};

} // namespace LEO
} // namespace NEO
