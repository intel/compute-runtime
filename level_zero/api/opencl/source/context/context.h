/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/sharings/sharing.h"
#include "level_zero/core/source/context/context.h"

#include <map>
#include <memory>

namespace NEO {
namespace LEO {

class RegularEventsManager;

template <>
struct OpenCLObjectMapper<_cl_context> {
    typedef class Context DerivedType;
};

class Context : public BaseObject<_cl_context> {
  public:
    static const cl_ulong objectMagic = 0xA4234321DC002130LL;

    using CallbackT = void(CL_CALLBACK *)(cl_context, void *);

    Context(const cl_context_properties *properties, ze_context_handle_t contextHandle, cl_uint numDevices, const cl_device_id *devices, bool externalHandle);
    Context() = delete;
    ~Context() override;

    template <typename ReturnType>
    static ReturnType getContextProperties(const cl_context_properties *properties,
                                           cl_context_properties propertyName,
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

    cl_int initialize();

    cl_int getInfo(cl_context_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    cl_int getMemAllocInfo(const void *ptr,
                           cl_mem_info_intel paramName,
                           size_t paramValueSize,
                           void *paramValue,
                           size_t *paramValueSizeRet);

    void *getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize);

    cl_int getSupportedImageFormats(
        Device *device,
        cl_mem_flags flags,
        cl_mem_object_type imageType,
        cl_uint numEntries,
        cl_image_format *imageFormats,
        cl_uint *numImageFormatsReturned);

    ze_event_handle_t obtainRegularEvent(bool timestamp);
    void returnRegularEvent(ze_event_handle_t event, bool timestamp);

    void registerCallback(CallbackT func, void *userData) { this->callbacks.emplace_back(func, userData); };

    template <typename Sharing>
    Sharing *getSharing() {
        if (Sharing::sharingId >= sharingFunctions.size()) {
            return nullptr;
        }

        return reinterpret_cast<Sharing *>(sharingFunctions[Sharing::sharingId].get());
    }

    template <typename Sharing>
    void registerSharing(Sharing *sharing) {
        UNRECOVERABLE_IF(!sharing);
        this->sharingFunctions[Sharing::sharingId].reset(sharing);
    }

    uint32_t getDefaultRootDeviceIndex() const { return this->clDevices[0]->getRootDeviceIndex(); };
    ze_command_list_handle_t getInternalCopyCmdList(uint32_t rootDeviceIndex) const { return this->internalCopyCmdLists.at(rootDeviceIndex); };
    ze_command_list_handle_t getInternalCopyCmdList() const { return getInternalCopyCmdList(getDefaultRootDeviceIndex()); };
    ze_command_list_handle_t getInternalComputeCmdList(uint32_t rootDeviceIndex) const { return this->internalComputeCmdLists.at(rootDeviceIndex); };
    ze_command_list_handle_t getInternalComputeCmdList() const { return getInternalComputeCmdList(getDefaultRootDeviceIndex()); };

    [[nodiscard]] std::unique_lock<std::mutex> lockInternalCopy() { return std::unique_lock(this->internalCopyMtx); };
    [[nodiscard]] std::unique_lock<std::mutex> lockInternalCompute() { return std::unique_lock(this->internalComputeMtx); };

    bool isTerminated() const { return this->executionTerminated; };
    void terminateExecution() { this->executionTerminated = true; };
    bool getInteropUserSyncEnabled() { return interopUserSync; }

    ClDevice *getClDevice() const { return this->clDevices[0]; };
    const std::vector<ClDevice *> &getClDevices() const { return this->clDevices; };
    ClDevice *findClDevice(ze_device_handle_t l0Device) const;
    ClDevice *getClDeviceByRootDeviceIndex(uint32_t rootDeviceIndex) const;

    const RootDeviceIndicesContainer &getRootDeviceIndices() const { return this->rootDeviceIndices; };
    const std::map<uint32_t, DeviceBitfield> &getDeviceBitfields() const { return this->deviceBitfields; };
    bool isSingleDeviceContext() const { return this->rootDeviceIndices.size() == 1u; };

    ze_context_handle_t getL0ContextHandle() const { return this->contextHandle; };
    L0::Context *getL0Object() const { return L0::Context::fromHandle(this->contextHandle); };

  protected:
    void storeProperties(const cl_context_properties *properties);
    void setInteropUserSyncEnabled(bool enabled) { interopUserSync = enabled; }

    std::vector<std::pair<CallbackT, void *>> callbacks{};

    std::vector<std::unique_ptr<SharingFunctions>> sharingFunctions;
    cl_bool preferD3dSharedResources = 0u;

    std::unique_ptr<RegularEventsManager> regularEventsManager;

    std::vector<cl_context_properties> contextProperties{};

    std::vector<ClDevice *> clDevices{};
    RootDeviceIndicesContainer rootDeviceIndices{};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{};
    ze_context_handle_t contextHandle = nullptr;
    std::map<uint32_t, ze_command_list_handle_t> internalCopyCmdLists{};
    std::map<uint32_t, ze_command_list_handle_t> internalComputeCmdLists{};
    std::mutex internalCopyMtx;
    std::mutex internalComputeMtx;

    bool interopUserSync = false;
    bool executionTerminated = false;
    bool externalHandle = false;
};

static_assert(NEO::NonCopyableAndNonMovable<Context>);

} // namespace LEO
} // namespace NEO
