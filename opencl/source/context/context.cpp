/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/scheduler/scheduler_kernel.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/sharing_factory.h"

#include "d3d_sharing_functions.h"

#include <algorithm>
#include <memory>

namespace NEO {

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
    schedulerBuiltIn = std::make_unique<BuiltInKernel>();
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
    delete static_cast<SchedulerKernel *>(schedulerBuiltIn->pKernel);
    delete schedulerBuiltIn->pProgram;
    schedulerBuiltIn->pKernel = nullptr;
    schedulerBuiltIn->pProgram = nullptr;
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

bool Context::areMultiStorageAllocationsPreferred() {
    return this->contextType != ContextType::CONTEXT_TYPE_SPECIALIZED;
}

bool Context::createImpl(const cl_context_properties *properties,
                         const ClDeviceVector &inputDevices,
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
            if (castToObject<Platform>(reinterpret_cast<cl_platform_id>(propertyValue)) == nullptr) {
                errcodeRet = CL_INVALID_PLATFORM;
                return false;
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
                errcodeRet = processExtraProperties(propertyType, propertyValue);
            }
            if (errcodeRet != CL_SUCCESS) {
                return false;
            }
            break;
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
        if (memoryManager->isAsyncDeleterEnabled()) {
            memoryManager->getDeferredDeleter()->addClient();
        }

        bool anySvmSupport = false;
        for (auto &device : devices) {
            device->incRefInternal();
            anySvmSupport |= device->getHardwareInfo().capabilityTable.ftrSvm;
        }

        if (anySvmSupport) {
            this->svmAllocsManager = new SVMAllocsManager(this->memoryManager);
        }
    }

    auto commandQueue = CommandQueue::create(this, devices[0], nullptr, true, errcodeRet);
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
        retVal = changeGetInfoStatusToCLResultType(::getInfo(paramValue, paramValueSize, pValue, valueSize));
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

size_t Context::getTotalNumDevices() const {
    size_t numAvailableDevices = 0u;
    for (auto &device : devices) {
        numAvailableDevices += device->getNumAvailableDevices();
    }
    return numAvailableDevices;
}

ClDevice *Context::getDevice(size_t deviceOrdinal) const {
    return (ClDevice *)devices[deviceOrdinal];
}

cl_int Context::getSupportedImageFormats(
    Device *device,
    cl_mem_flags flags,
    cl_mem_object_type imageType,
    cl_uint numEntries,
    cl_image_format *imageFormats,
    cl_uint *numImageFormatsReturned) {
    size_t numImageFormats = 0;
    const bool nv12ExtensionEnabled = device->getSpecializedDevice<ClDevice>()->getDeviceInfo().nv12Extension;
    const bool packedYuvExtensionEnabled = device->getSpecializedDevice<ClDevice>()->getDeviceInfo().packedYuvExtension;

    auto appendImageFormats = [&](ArrayRef<const ClSurfaceFormatInfo> formats) {
        if (imageFormats) {
            size_t offset = numImageFormats;
            for (size_t i = 0; i < formats.size() && offset < numEntries; ++i) {
                imageFormats[offset++] = formats[i].OCLImageFormat;
            }
        }
        numImageFormats += formats.size();
    };

    if (flags & CL_MEM_READ_ONLY) {
        if (this->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport >= 20) {
            appendImageFormats(SurfaceFormats::readOnly20());
        } else {
            appendImageFormats(SurfaceFormats::readOnly12());
        }
        if (Image::isImage2d(imageType) && nv12ExtensionEnabled) {
            appendImageFormats(SurfaceFormats::planarYuv());
        }
        if (Image::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readOnlyDepth());
        }
        if (Image::isImage2d(imageType) && packedYuvExtensionEnabled) {
            appendImageFormats(SurfaceFormats::packedYuv());
        }
    } else if (flags & CL_MEM_WRITE_ONLY) {
        appendImageFormats(SurfaceFormats::writeOnly());
        if (Image::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readWriteDepth());
        }
    } else if (nv12ExtensionEnabled && (flags & CL_MEM_NO_ACCESS_INTEL)) {
        if (this->getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport >= 20) {
            appendImageFormats(SurfaceFormats::readOnly20());
        } else {
            appendImageFormats(SurfaceFormats::readOnly12());
        }
        if (Image::isImage2d(imageType)) {
            appendImageFormats(SurfaceFormats::planarYuv());
        }
    } else {
        appendImageFormats(SurfaceFormats::readWrite());
        if (Image::isImage2dOr2dArray(imageType)) {
            appendImageFormats(SurfaceFormats::readWriteDepth());
        }
    }
    if (numImageFormatsReturned) {
        *numImageFormatsReturned = static_cast<cl_uint>(numImageFormats);
    }
    return CL_SUCCESS;
}

SchedulerKernel &Context::getSchedulerKernel() {
    if (schedulerBuiltIn->pKernel) {
        return *static_cast<SchedulerKernel *>(schedulerBuiltIn->pKernel);
    }

    auto initializeSchedulerProgramAndKernel = [&] {
        cl_int retVal = CL_SUCCESS;

        auto src = SchedulerKernel::loadSchedulerKernel(&getDevice(0)->getDevice());

        auto program = Program::createFromGenBinary(*getDevice(0)->getExecutionEnvironment(),
                                                    this,
                                                    src.resource.data(),
                                                    src.resource.size(),
                                                    true,
                                                    &retVal,
                                                    &getDevice(0)->getDevice());
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
        DEBUG_BREAK_IF(!program);

        retVal = program->processGenBinary();
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);

        schedulerBuiltIn->pProgram = program;

        auto kernelInfo = schedulerBuiltIn->pProgram->getKernelInfo(SchedulerKernel::schedulerName);
        DEBUG_BREAK_IF(!kernelInfo);

        schedulerBuiltIn->pKernel = Kernel::create<SchedulerKernel>(
            schedulerBuiltIn->pProgram,
            *kernelInfo,
            &retVal);

        UNRECOVERABLE_IF(schedulerBuiltIn->pKernel->getScratchSize() != 0);

        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
    };
    std::call_once(schedulerBuiltIn->programIsInitialized, initializeSchedulerProgramAndKernel);

    UNRECOVERABLE_IF(schedulerBuiltIn->pKernel == nullptr);
    return *static_cast<SchedulerKernel *>(schedulerBuiltIn->pKernel);
}

bool Context::isDeviceAssociated(const ClDevice &clDevice) const {
    for (const auto &device : devices) {
        if (device == &clDevice) {
            return true;
        }
    }
    return false;
}

AsyncEventsHandler &Context::getAsyncEventsHandler() {
    return *static_cast<ClExecutionEnvironment *>(devices[0]->getExecutionEnvironment())->getAsyncEventsHandler();
}

} // namespace NEO
