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
    contextCallback = funcNotify;
    userData = data;
    sharingFunctions.resize(SharingType::MAX_SHARING_VALUE);
    schedulerBuiltIn = std::make_unique<BuiltInKernel>();
}

Context::~Context() {
    delete[] properties;

    for (auto rootDeviceIndex = 0u; rootDeviceIndex < specialQueues.size(); rootDeviceIndex++) {
        if (specialQueues[rootDeviceIndex]) {
            delete specialQueues[rootDeviceIndex];
        }
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
    for (auto callback : destructorCallbacks) {
        callback->invoke(this);
        delete callback;
    }
    for (auto &device : devices) {
        device->decRefInternal();
    }
    delete static_cast<SchedulerKernel *>(schedulerBuiltIn->pKernel);
    delete schedulerBuiltIn->pProgram;
    schedulerBuiltIn->pKernel = nullptr;
    schedulerBuiltIn->pProgram = nullptr;
}

cl_int Context::setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_context, void *),
                                      void *userData) {
    auto cb = new ContextDestructorCallback(funcNotify, userData);

    std::unique_lock<std::mutex> theLock(mtx);
    destructorCallbacks.push_front(cb);
    return CL_SUCCESS;
}

const std::set<uint32_t> &Context::getRootDeviceIndices() const {
    return rootDeviceIndices;
}

uint32_t Context::getMaxRootDeviceIndex() const {
    return maxRootDeviceIndex;
}

DeviceQueue *Context::getDefaultDeviceQueue() {
    return defaultDeviceQueue;
}

void Context::setDefaultDeviceQueue(DeviceQueue *queue) {
    defaultDeviceQueue = queue;
}

CommandQueue *Context::getSpecialQueue(uint32_t rootDeviceIndex) {
    return specialQueues[rootDeviceIndex];
}

void Context::setSpecialQueue(CommandQueue *commandQueue, uint32_t rootDeviceIndex) {
    specialQueues[rootDeviceIndex] = commandQueue;
}
void Context::overrideSpecialQueueAndDecrementRefCount(CommandQueue *commandQueue, uint32_t rootDeviceIndex) {
    setSpecialQueue(commandQueue, rootDeviceIndex);
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
            if (!sharingBuilder->processProperties(propertyType, propertyValue)) {
                errcodeRet = CL_INVALID_PROPERTY;
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

    for (const auto &device : inputDevices) {
        rootDeviceIndices.insert(device->getRootDeviceIndex());
    }

    this->driverDiagnostics = driverDiagnostics.release();
    if (rootDeviceIndices.size() > 1 && !DebugManager.flags.EnableMultiRootDeviceContexts.get()) {
        DEBUG_BREAK_IF("No support for context with multiple root devices");
        errcodeRet = CL_OUT_OF_HOST_MEMORY;
        return false;
    }

    devices = inputDevices;
    for (auto &rootDeviceIndex : rootDeviceIndices) {
        DeviceBitfield deviceBitfield{};
        for (const auto &pDevice : devices) {
            if (pDevice->getRootDeviceIndex() == rootDeviceIndex) {
                deviceBitfield |= pDevice->getDeviceBitfield();
            }
        }
        deviceBitfields.insert({rootDeviceIndex, deviceBitfield});
    }

    if (devices.size() > 0) {
        maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
        specialQueues.resize(maxRootDeviceIndex + 1u);
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
        setupContextType();
    }

    for (auto &device : devices) {
        if (!specialQueues[device->getRootDeviceIndex()]) {
            auto commandQueue = CommandQueue::create(this, device, nullptr, true, errcodeRet); // NOLINT
            DEBUG_BREAK_IF(commandQueue == nullptr);
            overrideSpecialQueueAndDecrementRefCount(commandQueue, device->getRootDeviceIndex());
        }
    }

    return true;
}

cl_int Context::getInfo(cl_context_info paramName, size_t paramValueSize,
                        void *paramValue, size_t *paramValueSizeRet) {
    cl_int retVal;
    size_t valueSize = GetInfo::invalidSourceSize;
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

    GetInfoStatus getInfoStatus = GetInfoStatus::SUCCESS;
    if (callGetinfo) {
        getInfoStatus = GetInfo::getInfo(paramValue, paramValueSize, pValue, valueSize);
    }

    retVal = changeGetInfoStatusToCLResultType(getInfoStatus);
    GetInfo::setParamValueReturnSize(paramValueSizeRet, valueSize, getInfoStatus);

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

    if (isValueSet(CL_MEM_KERNEL_READ_AND_WRITE, flags) && device->getSpecializedDevice<ClDevice>()->areOcl21FeaturesEnabled() == false) {
        if (numImageFormatsReturned) {
            *numImageFormatsReturned = static_cast<cl_uint>(numImageFormats);
        }
        return CL_SUCCESS;
    }

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
        if (this->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features) {
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
        if (this->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features) {
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
        auto device = &getDevice(0)->getDevice();
        auto src = SchedulerKernel::loadSchedulerKernel(device);

        auto program = Program::createBuiltInFromGenBinary(this,
                                                           devices,
                                                           src.resource.data(),
                                                           src.resource.size(),
                                                           &retVal);
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);
        DEBUG_BREAK_IF(!program);

        retVal = program->processGenBinary(device->getRootDeviceIndex());
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
    for (const auto &pDevice : devices) {
        if (pDevice == &clDevice) {
            return true;
        }
    }
    return false;
}

ClDevice *Context::getSubDeviceByIndex(uint32_t subDeviceIndex) const {

    auto isExpectedSubDevice = [subDeviceIndex](ClDevice *pClDevice) -> bool {
        bool isSubDevice = (pClDevice->getDeviceInfo().parentDevice != nullptr);
        if (isSubDevice == false) {
            return false;
        }

        auto &subDevice = static_cast<SubDevice &>(pClDevice->getDevice());
        return (subDevice.getSubDeviceIndex() == subDeviceIndex);
    };

    auto foundDeviceIterator = std::find_if(devices.begin(), devices.end(), isExpectedSubDevice);
    return (foundDeviceIterator != devices.end() ? *foundDeviceIterator : nullptr);
}

AsyncEventsHandler &Context::getAsyncEventsHandler() const {
    return *static_cast<ClExecutionEnvironment *>(devices[0]->getExecutionEnvironment())->getAsyncEventsHandler();
}

DeviceBitfield Context::getDeviceBitfieldForAllocation(uint32_t rootDeviceIndex) const {
    return deviceBitfields.at(rootDeviceIndex);
}

void Context::setupContextType() {
    if (contextType == ContextType::CONTEXT_TYPE_DEFAULT) {
        if (devices.size() > 1) {
            for (const auto &pDevice : devices) {
                if (!pDevice->getDeviceInfo().parentDevice) {
                    contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
                    return;
                }
            }
        }
        if (devices[0]->getDeviceInfo().parentDevice) {
            contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
        }
    }
}

} // namespace NEO
