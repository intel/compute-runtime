/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/context/context_type.h"
#include "runtime/context/driver_diagnostics.h"
#include "runtime/device/device_vector.h"
#include "runtime/helpers/base_object.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include <vector>

namespace NEO {

class CommandStreamReceiver;
class CommandQueue;
class Device;
class DeviceQueue;
class MemObj;
class MemoryManager;
class SharingFunctions;
class SVMAllocsManager;

template <>
struct OpenCLObjectMapper<_cl_context> {
    typedef class Context DerivedType;
};

class Context : public BaseObject<_cl_context> {
  public:
    static const cl_ulong objectMagic = 0xA4234321DC002130LL;

    bool createImpl(const cl_context_properties *properties,
                    const DeviceVector &devices,
                    void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *),
                    void *userData, cl_int &errcodeRet);

    template <typename T>
    static T *create(const cl_context_properties *properties,
                     const DeviceVector &devices,
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

    cl_int getInfo(cl_context_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    cl_int getSupportedImageFormats(Device *device, cl_mem_flags flags,
                                    cl_mem_object_type imageType, cl_uint numEntries,
                                    cl_image_format *imageFormats, cl_uint *numImageFormats);

    size_t getNumDevices() const;
    Device *getDevice(size_t deviceOrdinal);

    MemoryManager *getMemoryManager() {
        return memoryManager;
    }

    SVMAllocsManager *getSVMAllocsManager() const {
        return svmAllocsManager;
    }

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

    ContextType peekContextType() { return this->contextType; }

    MOCKABLE_VIRTUAL CommandStreamReceiver *getCommandStreamReceiverForBlitOperation(MemObj &memObj) const;

  protected:
    Context(void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *) = nullptr,
            void *userData = nullptr);

    // OS specific implementation
    void *getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize);

    cl_int processExtraProperties(cl_context_properties propertyType, cl_context_properties propertyValue);

    const cl_context_properties *properties;
    size_t numProperties;
    void(CL_CALLBACK *contextCallback)(const char *, const void *, size_t, void *);
    void *userData;

    DeviceVector devices;
    MemoryManager *memoryManager;
    SVMAllocsManager *svmAllocsManager = nullptr;
    CommandQueue *specialQueue;
    DeviceQueue *defaultDeviceQueue;
    std::vector<std::unique_ptr<SharingFunctions>> sharingFunctions;
    DriverDiagnostics *driverDiagnostics;
    bool interopUserSync = false;
    cl_bool preferD3dSharedResources = 0u;
    ContextType contextType = ContextType::CONTEXT_TYPE_DEFAULT;
};
} // namespace NEO
