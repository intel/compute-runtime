/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/vec.h"

#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/context/context_type.h"
#include "opencl/source/context/driver_diagnostics.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/source/helpers/destructor_callback.h"

#include <list>
#include <map>
#include <set>

namespace NEO {

class AsyncEventsHandler;
struct BuiltInKernel;
class CommandQueue;
class Device;
class DeviceQueue;
class MemObj;
class MemoryManager;
class SharingFunctions;
class SVMAllocsManager;
class SchedulerKernel;
class Program;

template <>
struct OpenCLObjectMapper<_cl_context> {
    typedef class Context DerivedType;
};

class Context : public BaseObject<_cl_context> {
  public:
    static const cl_ulong objectMagic = 0xA4234321DC002130LL;

    bool createImpl(const cl_context_properties *properties,
                    const ClDeviceVector &devices,
                    void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *),
                    void *userData, cl_int &errcodeRet);

    template <typename T>
    static T *create(const cl_context_properties *properties,
                     const ClDeviceVector &devices,
                     void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
                     void *data, cl_int &errcodeRet) {

        auto pContext = new T(funcNotify, data);

        if (!pContext->createImpl(properties, devices, funcNotify, data, errcodeRet)) {
            delete pContext;
            pContext = nullptr;
        }

        return pContext;
    }

    Context &operator=(const Context &) = delete;
    Context(const Context &) = delete;

    ~Context() override;

    cl_int setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_context, void *),
                                 void *userData);

    cl_int getInfo(cl_context_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    cl_int getSupportedImageFormats(Device *device, cl_mem_flags flags,
                                    cl_mem_object_type imageType, cl_uint numEntries,
                                    cl_image_format *imageFormats, cl_uint *numImageFormats);

    size_t getNumDevices() const;
    size_t getTotalNumDevices() const;
    ClDevice *getDevice(size_t deviceOrdinal) const;

    MemoryManager *getMemoryManager() const {
        return memoryManager;
    }

    SVMAllocsManager *getSVMAllocsManager() const {
        return svmAllocsManager;
    }

    const std::set<uint32_t> &getRootDeviceIndices() const;

    uint32_t getMaxRootDeviceIndex() const;

    DeviceQueue *getDefaultDeviceQueue();
    void setDefaultDeviceQueue(DeviceQueue *queue);

    CommandQueue *getSpecialQueue();
    void setSpecialQueue(CommandQueue *commandQueue);
    void overrideSpecialQueueAndDecrementRefCount(CommandQueue *commandQueue);

    template <typename Sharing>
    Sharing *getSharing();

    template <typename Sharing>
    void registerSharing(Sharing *sharing);

    template <typename... Args>
    void providePerformanceHint(cl_diagnostics_verbose_level flags, PerformanceHints performanceHint, Args &&... args) {
        DEBUG_BREAK_IF(contextCallback == nullptr);
        DEBUG_BREAK_IF(driverDiagnostics == nullptr);
        char hint[DriverDiagnostics::maxHintStringSize];
        snprintf(hint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[performanceHint], std::forward<Args>(args)..., 0);
        if (driverDiagnostics->validFlags(flags)) {
            if (contextCallback) {
                contextCallback(hint, &flags, sizeof(flags), userData);
            }
            if (DebugManager.flags.PrintDriverDiagnostics.get() != -1) {
                printf("\n%s\n", hint);
            }
        }
    }

    template <typename... Args>
    void providePerformanceHintForMemoryTransfer(cl_command_type commandType, bool transferRequired, Args &&... args) {
        cl_diagnostics_verbose_level verboseLevel = transferRequired ? CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL
                                                                     : CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL;
        PerformanceHints hint = driverDiagnostics->obtainHintForTransferOperation(commandType, transferRequired);

        providePerformanceHint(verboseLevel, hint, args...);
    }

    cl_bool isProvidingPerformanceHints() const {
        return driverDiagnostics != nullptr;
    }

    bool getInteropUserSyncEnabled() { return interopUserSync; }
    void setInteropUserSyncEnabled(bool enabled) { interopUserSync = enabled; }
    bool areMultiStorageAllocationsPreferred();

    ContextType peekContextType() const { return contextType; }

    SchedulerKernel &getSchedulerKernel();

    bool isDeviceAssociated(const ClDevice &clDevice) const;
    ClDevice *getSubDeviceByIndex(uint32_t subDeviceIndex) const;

    AsyncEventsHandler &getAsyncEventsHandler() const;

    DeviceBitfield getDeviceBitfieldForAllocation(uint32_t rootDeviceIndex) const;
    bool getResolvesRequiredInKernels() const {
        return resolvesRequiredInKernels;
    }
    void setResolvesRequiredInKernels(bool resolves) {
        resolvesRequiredInKernels = resolves;
    }
    const ClDeviceVector &getDevices() const {
        return devices;
    }

  protected:
    struct BuiltInKernel {
        const char *pSource = nullptr;
        Program *pProgram = nullptr;
        std::once_flag programIsInitialized; // guard for creating+building the program
        Kernel *pKernel = nullptr;

        BuiltInKernel() {
        }
    };

    Context(void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *) = nullptr,
            void *userData = nullptr);

    // OS specific implementation
    void *getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize);

    cl_int processExtraProperties(cl_context_properties propertyType, cl_context_properties propertyValue);
    void setupContextType();

    std::set<uint32_t> rootDeviceIndices = {};
    std::map<uint32_t, DeviceBitfield> deviceBitfields;
    std::vector<std::unique_ptr<SharingFunctions>> sharingFunctions;
    ClDeviceVector devices;
    std::list<ContextDestructorCallback *> destructorCallbacks;
    std::unique_ptr<BuiltInKernel> schedulerBuiltIn;

    const cl_context_properties *properties = nullptr;
    size_t numProperties = 0u;
    void(CL_CALLBACK *contextCallback)(const char *, const void *, size_t, void *) = nullptr;
    void *userData = nullptr;
    MemoryManager *memoryManager = nullptr;
    SVMAllocsManager *svmAllocsManager = nullptr;
    CommandQueue *specialQueue = nullptr;
    DeviceQueue *defaultDeviceQueue = nullptr;
    DriverDiagnostics *driverDiagnostics = nullptr;

    uint32_t maxRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    cl_bool preferD3dSharedResources = 0u;
    ContextType contextType = ContextType::CONTEXT_TYPE_DEFAULT;

    bool interopUserSync = false;
    bool resolvesRequiredInKernels = false;
};
} // namespace NEO
