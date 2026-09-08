/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/api/leo_cl_types.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_surface_formats.h"
#include "level_zero/api/opencl/source/mem_obj/leo_map_operations_handler.h"

#include "CL/cl.h"

#include <atomic>
#include <vector>

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_mem> {
    typedef class MemObj DerivedType;
};

class MemObj : public BaseObject<_cl_mem> {
  public:
    constexpr static cl_ulong maskMagic = 0xFFFFFFFFFFFFFF00LL;
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

    static constexpr bool isHandleListProperty(cl_mem_properties propertyName) {
        return (propertyName == CL_IMAGE_L0_HANDLE_INTEL) ||
               (propertyName == CL_MEM_DEVICE_HANDLE_LIST_KHR);
    }

    static size_t getHandleListEnd(const cl_mem_properties *properties, size_t nameIndex) {
        size_t entryIndex = nameIndex + 1;
        while (properties[entryIndex] != CL_IMAGE_L0_HANDLE_LIST_END_INTEL) {
            ++entryIndex;
        }
        return entryIndex;
    }

    template <typename ReturnType>
    static ReturnType getMemObjProperties(const cl_mem_properties *properties,
                                          cl_mem_properties propertyName,
                                          bool *foundValue = nullptr) {
        if (properties != nullptr) {
            size_t i = 0;
            while (properties[i] != 0) {
                if (properties[i] == propertyName) {
                    if (foundValue) {
                        *foundValue = true;
                    }
                    return static_cast<ReturnType>(properties[i + 1]);
                }
                i = isHandleListProperty(properties[i]) ? getHandleListEnd(properties, i) + 1 : i + 2;
            }
        }

        if (foundValue) {
            *foundValue = false;
        }
        return 0;
    }

    static std::vector<uintptr_t> getMemObjHandleList(const cl_mem_properties *properties,
                                                      cl_mem_properties propertyName,
                                                      bool *foundValue = nullptr) {
        std::vector<uintptr_t> handles{};
        bool found = false;

        if (properties != nullptr) {
            size_t i = 0;
            while (properties[i] != 0) {
                const auto currentName = properties[i];
                const bool matches = (currentName == propertyName);
                found |= matches;

                if (isHandleListProperty(currentName)) {
                    const auto listEnd = getHandleListEnd(properties, i);
                    if (matches) {
                        for (size_t entry = i + 1; entry < listEnd; ++entry) {
                            handles.push_back(static_cast<uintptr_t>(properties[entry]));
                        }
                    }
                    i = listEnd + 1;
                } else {
                    if (matches) {
                        handles.push_back(static_cast<uintptr_t>(properties[i + 1]));
                    }
                    i += 2;
                }
            }
        }

        if (foundValue) {
            *foundValue = found;
        }
        return handles;
    }

    void storeProperties(const cl_mem_properties *properties);
    void setUsesSvm(bool usesSvm) { this->usesSvm = usesSvm; }
    bool getUsesSvm() const { return this->usesSvm; }
    void addCallback(CallbackT callback, void *userData) {
        auto lock = this->takeOwnership();
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

    virtual void refreshDeviceAddress(uint32_t rootDeviceIndex) {}

    std::shared_ptr<SharingHandler> &getSharingHandler() { return sharingHandler; };
    SharingHandler *peekSharingHandler() const { return sharingHandler.get(); };
    void setSharingHandler(SharingHandler *sharingHandler) { this->sharingHandler.reset(sharingHandler); };
    void setParentSharingHandler(std::shared_ptr<SharingHandler> &handler) { sharingHandler = handler; };
    std::atomic<unsigned int> acquireCount{0};
    bool mapMemObjFlagsInvalid(cl_map_flags mapFlags);
    bool readMemObjFlagsInvalid();
    bool writeMemObjFlagsInvalid();

  protected:
    virtual void checkUsageAndReleaseOldAllocation(uint32_t rootDeviceIndex) = 0;

    ze_driver_memory_free_policy_ext_flags_t getFreePolicy() const {
        const bool waitForGpuCompletion = this->properties.flags.useHostPtr || false == this->callbacks.empty();
        return waitForGpuCompletion ? ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE
                                    : ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE;
    }

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
