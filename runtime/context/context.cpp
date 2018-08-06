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

#include "runtime/context/context.h"
#include "runtime/helpers/surface_formats.h"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/mem_obj/image.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/get_info.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/platform/platform.h"
#include "runtime/helpers/string.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/sharings/sharing.h"
#include <algorithm>
#include <memory>
#include "d3d_sharing_functions.h"

namespace OCLRT {

Context::Context(
    void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
    void *data) {
    properties = nullptr;
    numProperties = 0;
    contextCallback = funcNotify;
    userData = data;
    memoryManager = nullptr;
    specialQueue = nullptr;
    defaultDeviceQueue = nullptr;
    driverDiagnostics = nullptr;
    sharingFunctions.resize(SharingType::MAX_SHARING_VALUE);
}

Context::~Context() {
    delete[] properties;
    if (specialQueue) {
        delete specialQueue;
    }
    if (svmAllocsManager) {
        delete svmAllocsManager;
    }
    if (driverDiagnostics) {
        delete driverDiagnostics;
    }
    if (memoryManager && memoryManager->isAsyncDeleterEnabled()) {
        memoryManager->getDeferredDeleter()->removeClient();
    }
    gtpinNotifyContextDestroy((cl_context)this);
    for (auto &device : devices) {
        device->decRefInternal();
    }
}

DeviceQueue *Context::getDefaultDeviceQueue() {
    return defaultDeviceQueue;
}

void Context::setDefaultDeviceQueue(DeviceQueue *queue) {
    defaultDeviceQueue = queue;
}

CommandQueue *Context::getSpecialQueue() {
    return specialQueue;
}

void Context::setSpecialQueue(CommandQueue *commandQueue) {
    specialQueue = commandQueue;
}
void Context::overrideSpecialQueueAndDecrementRefCount(CommandQueue *commandQueue) {
    setSpecialQueue(commandQueue);
    commandQueue->setIsSpecialCommandQueue(true);
    //decrement ref count that special queue added
    this->decRefInternal();
};

bool Context::createImpl(const cl_context_properties *properties,
                         const DeviceVector &inputDevices,
                         void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
                         void *data, cl_int &errcodeRet) {

    auto propertiesCurrent = properties;
    bool interopUserSync = false;
    int32_t driverDiagnosticsUsed = -1;
    auto sharingBuilder = sharingFactory.build();

    std::unique_ptr<DriverDiagnostics> driverDiagnostics;
    while (propertiesCurrent && *propertiesCurrent) {
        errcodeRet = CL_SUCCESS;

        auto propertyType = propertiesCurrent[0];
        auto propertyValue = propertiesCurrent[1];
        propertiesCurrent += 2;

        switch (propertyType) {
        case CL_CONTEXT_PLATFORM: {
            cl_platform_id pid = platform();
            if (reinterpret_cast<cl_platform_id>(propertyValue) != pid) {
                errcodeRet = CL_INVALID_PLATFORM;
            }
        } break;
        case CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL:
            driverDiagnosticsUsed = static_cast<int32_t>(propertyValue);
            break;
        case CL_CONTEXT_INTEROP_USER_SYNC:
            interopUserSync = propertyValue > 0;
            break;
        default:
            if (!sharingBuilder->processProperties(propertyType, propertyValue, errcodeRet)) {
                errcodeRet = CL_INVALID_PROPERTY;
            }
            break;
        }
        if (errcodeRet != CL_SUCCESS) {
            return false;
        }
    }

    auto numProperties = ptrDiff(propertiesCurrent, properties) / sizeof(cl_context_properties);
    cl_context_properties *propertiesNew = nullptr;

    // copy the user properties if there are any
    if (numProperties) {
        propertiesNew = new cl_context_properties[numProperties + 1];
        memcpy_s(propertiesNew, (numProperties + 1) * sizeof(cl_context_properties), properties, numProperties * sizeof(cl_context_properties));
        propertiesNew[numProperties] = 0;
        numProperties++;
    }

    if (DebugManager.flags.PrintDriverDiagnostics.get() != -1) {
        driverDiagnosticsUsed = DebugManager.flags.PrintDriverDiagnostics.get();
    }
    if (driverDiagnosticsUsed >= 0) {
        driverDiagnostics.reset(new DriverDiagnostics((cl_diagnostics_verbose_level)driverDiagnosticsUsed));
    }

    this->numProperties = numProperties;
    this->properties = propertiesNew;
    this->setInteropUserSyncEnabled(interopUserSync);

    if (!sharingBuilder->finalizeProperties(*this, errcodeRet)) {
        return false;
    }

    this->driverDiagnostics = driverDiagnostics.release();
    this->devices = inputDevices;

    // We currently assume each device uses the same MemoryManager
    if (devices.size() > 0) {
        auto device = this->getDevice(0);
        this->memoryManager = device->getMemoryManager();
        this->svmAllocsManager = new SVMAllocsManager(this->memoryManager);
        if (memoryManager->isAsyncDeleterEnabled()) {
            memoryManager->getDeferredDeleter()->addClient();
        }
        if (this->sharingFunctions[SharingType::VA_SHARING]) {
            device->getCommandStreamReceiver().peekKmdNotifyHelper()->initMaxPowerSavingMode();
        }
    }

    for (auto &device : devices) {
        device->incRefInternal();
    }

    auto commandQueue = CommandQueue::create(this, devices[0], nullptr, errcodeRet);
    DEBUG_BREAK_IF(commandQueue == nullptr);
    overrideSpecialQueueAndDecrementRefCount(commandQueue);

    return true;
}

cl_int Context::getInfo(cl_context_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t valueSize = 0;
    const void *pValue = nullptr;
    cl_uint numDevices;
    cl_uint refCount = 0;
    std::vector<cl_device_id> devIDs;
    auto callGetinfo = true;

    switch (paramName) {
    case CL_CONTEXT_DEVICES:
        valueSize = devices.size() * sizeof(cl_device_id);
        devices.toDeviceIDs(devIDs);
        pValue = devIDs.data();
        break;

    case CL_CONTEXT_NUM_DEVICES:
        numDevices = (cl_uint)(devices.size());
        valueSize = sizeof(numDevices);
        pValue = &numDevices;
        break;

    case CL_CONTEXT_PROPERTIES:
        valueSize = this->numProperties * sizeof(cl_context_properties);
        pValue = this->properties;
        if (valueSize == 0) {
            callGetinfo = false;
        }

        break;

    case CL_CONTEXT_REFERENCE_COUNT:
        refCount = static_cast<cl_uint>(this->getReference());
        valueSize = sizeof(refCount);
        pValue = &refCount;
        break;

    default:
        pValue = getOsContextInfo(paramName, &valueSize);
        break;
    }

    if (callGetinfo) {
        retVal = ::getInfo(paramValue, paramValueSize, pValue, valueSize);
    } else {
        retVal = CL_SUCCESS;
    }

    if (paramValueSizeRet) {
        *paramValueSizeRet = valueSize;
    }

    return retVal;
}

size_t Context::getNumDevices() const {
    return devices.size();
}

Device *Context::getDevice(size_t deviceOrdinal) {
    return (Device *)devices[deviceOrdinal];
}

cl_int Context::getSupportedImageFormats(
    Device *device,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_image_format *imageFormats,
    cl_uint *numImageFormatsReturned) {

    size_t numImageFormats = 0;
    size_t numDepthFormats = 0;
    const SurfaceFormatInfo *baseSurfaceFormats = nullptr;
    const SurfaceFormatInfo *depthFormats = nullptr;

    const bool nv12ExtensionEnabled = device->getDeviceInfo().nv12Extension;
    const bool packedYuvExtensionEnabled = device->getDeviceInfo().packedYuvExtension;
    bool appendPlanarYUVSurfaces = false;
    bool appendPackedYUVSurfaces = false;
    bool appendDepthSurfaces = true;

    if (flags & CL_MEM_READ_ONLY) {
        numImageFormats = numReadOnlySurfaceFormats;
        baseSurfaceFormats = readOnlySurfaceFormats;
        depthFormats = readOnlyDepthSurfaceFormats;
        numDepthFormats = numReadOnlyDepthSurfaceFormats;
        appendPlanarYUVSurfaces = true;
        appendPackedYUVSurfaces = true;
    } else if (flags & CL_MEM_WRITE_ONLY) {
        numImageFormats = numWriteOnlySurfaceFormats;
        baseSurfaceFormats = writeOnlySurfaceFormats;
        depthFormats = readWriteDepthSurfaceFormats;
        numDepthFormats = numReadWriteDepthSurfaceFormats;
    } else if (nv12ExtensionEnabled && (flags & CL_MEM_NO_ACCESS_INTEL)) {
        numImageFormats = numReadOnlySurfaceFormats;
        baseSurfaceFormats = readOnlySurfaceFormats;
        appendPlanarYUVSurfaces = true;
    } else {
        numImageFormats = numReadWriteSurfaceFormats;
        baseSurfaceFormats = readWriteSurfaceFormats;
        depthFormats = readWriteDepthSurfaceFormats;
        numDepthFormats = numReadWriteDepthSurfaceFormats;
    }

    if (!Image::isImage2d(imageType)) {
        appendPlanarYUVSurfaces = false;
        appendPackedYUVSurfaces = false;
    }
    if (!Image::isImage2dOr2dArray(imageType)) {
        appendDepthSurfaces = false;
    }

    if (imageFormats) {
        cl_uint entry = 0;
        auto appendFormats = [&](const SurfaceFormatInfo *srcSurfaceFormats, size_t srcFormatsCount) {
            for (size_t srcFormatPos = 0; srcFormatPos < srcFormatsCount && entry < numEntries; ++srcFormatPos, ++entry) {
                imageFormats[entry] = srcSurfaceFormats[srcFormatPos].OCLImageFormat;
            }
        };

        appendFormats(baseSurfaceFormats, numImageFormats);

        if (nv12ExtensionEnabled && appendPlanarYUVSurfaces) {
            appendFormats(planarYuvSurfaceFormats, numPlanarYuvSurfaceFormats);
        }

        if (appendDepthSurfaces) {
            appendFormats(depthFormats, numDepthFormats);
        }

        if (packedYuvExtensionEnabled && appendPackedYUVSurfaces) {
            appendFormats(packedYuvSurfaceFormats, numPackedYuvSurfaceFormats);
        }
    }

    if (numImageFormatsReturned) {
        if (nv12ExtensionEnabled && appendPlanarYUVSurfaces) {
            numImageFormats += numPlanarYuvSurfaceFormats;
        }
        if (packedYuvExtensionEnabled && appendPackedYUVSurfaces) {
            numImageFormats += numPackedYuvSurfaceFormats;
        }
        if (appendDepthSurfaces) {
            numImageFormats += numDepthFormats;
        }

        *numImageFormatsReturned = static_cast<cl_uint>(numImageFormats);
    }
    return CL_SUCCESS;
}
} // namespace OCLRT
