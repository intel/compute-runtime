/*
 * Copyright (c) 2017, Intel Corporation
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
#include "runtime/device/device_vector.h"
#include "runtime/event/event.h"
#include "runtime/event/event_registry.h"
#include "runtime/context/driver_diagnostics.h"
#include <vector>

namespace OCLRT {

class Device;
class DeviceQueue;
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

    EventsRegistry &getEventsRegistry() {
        return eventsRegistry;
    }

    DeviceQueue *getDefaultDeviceQueue();
    void setDefaultDeviceQueue(DeviceQueue *queue);

    CommandQueue *getSpecialQueue();
    void setSpecialQueue(CommandQueue *commandQueue);
    bool isSpecialQueue(CommandQueue *commandQueue);
    void deleteSpecialQueue();

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
            contextCallback(hint, &flags, sizeof(flags), userData);
        }
    }

    cl_bool isProvidingPerformanceHints() const {
        return driverDiagnostics != nullptr;
    }

    bool getInteropUserSyncEnabled() { return interopUserSync; }
    void setInteropUserSyncEnabled(bool enabled) { interopUserSync = enabled; }

  protected:
    Context(void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *) = nullptr,
            void *userData = nullptr);

    // OS specific implementation
    cl_int createContextOsProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue);
    void *getOsContextInfo(cl_context_info &paramName, size_t *srcParamSize);

    const cl_context_properties *properties;
    size_t numProperties;
    void(CL_CALLBACK *contextCallback)(const char *, const void *, size_t, void *);
    void *userData;

    DeviceVector devices;
    MemoryManager *memoryManager;
    SVMAllocsManager *svmAllocsManager = nullptr;
    EventsRegistry eventsRegistry;
    CommandQueue *specialQueue;
    DeviceQueue *defaultDeviceQueue;
    std::vector<std::unique_ptr<SharingFunctions>> sharingFunctions;
    DriverDiagnostics *driverDiagnostics;
    bool interopUserSync = false;
    cl_bool preferD3dSharedResources = 0u;
};
} // namespace OCLRT
