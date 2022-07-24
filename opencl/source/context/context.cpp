/*
 * Copyright (C) 2018-2022 Intel Corporation
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
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
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
}

Context::~Context() {
    gtpinNotifyContextDestroy((cl_context)this);

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
    destructorCallbacks.invoke(this);
    for (auto &device : devices) {
        device->decRefInternal();
    }
}

cl_int Context::setDestructorCallback(void(CL_CALLBACK *funcNotify)(cl_context, void *),
                                      void *userData) {
    std::unique_lock<std::mutex> theLock(mtx);
    destructorCallbacks.add(funcNotify, userData);
    return CL_SUCCESS;
}

cl_int Context::tryGetExistingHostPtrAllocation(const void *ptr,
                                                size_t size,
                                                uint32_t rootDeviceIndex,
                                                GraphicsAllocation *&allocation,
                                                InternalMemoryType &memoryType,
                                                bool &isCpuCopyAllowed) {
    cl_int retVal = tryGetExistingSvmAllocation(ptr, size, rootDeviceIndex, allocation, memoryType, isCpuCopyAllowed);
    if (retVal != CL_SUCCESS || allocation != nullptr) {
        return retVal;
    }

    retVal = tryGetExistingMapAllocation(ptr, size, allocation);
    return retVal;
}

cl_int Context::tryGetExistingSvmAllocation(const void *ptr,
                                            size_t size,
                                            uint32_t rootDeviceIndex,
                                            GraphicsAllocation *&allocation,
                                            InternalMemoryType &memoryType,
                                            bool &isCpuCopyAllowed) {
    if (getSVMAllocsManager()) {
        SvmAllocationData *svmEntry = getSVMAllocsManager()->getSVMAlloc(ptr);
        if (svmEntry) {
            memoryType = svmEntry->memoryType;
            if ((svmEntry->gpuAllocations.getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + svmEntry->size) < (castToUint64(ptr) + size)) {
                return CL_INVALID_OPERATION;
            }
            allocation = svmEntry->cpuAllocation ? svmEntry->cpuAllocation : svmEntry->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
            if (isCpuCopyAllowed) {
                if (svmEntry->memoryType == DEVICE_UNIFIED_MEMORY) {
                    isCpuCopyAllowed = false;
                }
            }
        }
    }
    return CL_SUCCESS;
}

cl_int Context::tryGetExistingMapAllocation(const void *ptr,
                                            size_t size,
                                            GraphicsAllocation *&allocation) {
    if (MapInfo mapInfo = {}; mapOperationsStorage.getInfoForHostPtr(ptr, size, mapInfo)) {
        if (mapInfo.graphicsAllocation) {
            allocation = mapInfo.graphicsAllocation;
        }
    }
    return CL_SUCCESS;
}

const RootDeviceIndicesContainer &Context::getRootDeviceIndices() const {
    return rootDeviceIndices;
}

uint32_t Context::getMaxRootDeviceIndex() const {
    return maxRootDeviceIndex;
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
    // decrement ref count that special queue added
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
        case CL_CONTEXT_PLATFORM:
            break;
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

    bool containsDeviceWithSubdevices = false;
    for (const auto &device : inputDevices) {
        rootDeviceIndices.push_back(device->getRootDeviceIndex());
        containsDeviceWithSubdevices |= device->getNumGenericSubDevices() > 1;
    }
    rootDeviceIndices.remove_duplicates();

    this->driverDiagnostics = driverDiagnostics.release();
    if (rootDeviceIndices.size() > 1 && containsDeviceWithSubdevices && !DebugManager.flags.EnableMultiRootDeviceContexts.get()) {
        DEBUG_BREAK_IF("No support for context with multiple devices with subdevices");
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

        setupContextType();
        if (anySvmSupport) {
            this->svmAllocsManager = new SVMAllocsManager(this->memoryManager,
                                                          this->areMultiStorageAllocationsPreferred());
        }
    }

    for (auto &device : devices) {
        if (!specialQueues[device->getRootDeviceIndex()]) {
            auto commandQueue = CommandQueue::create(this, device, nullptr, true, errcodeRet); // NOLINT(clang-analyzer-cplusplus.NewDelete)
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

bool Context::containsMultipleSubDevices(uint32_t rootDeviceIndex) const {
    return deviceBitfields.at(rootDeviceIndex).count() > 1;
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

Platform *Context::getPlatformFromProperties(const cl_context_properties *properties, cl_int &errcode) {
    errcode = CL_SUCCESS;
    auto propertiesCurrent = properties;
    while (propertiesCurrent && *propertiesCurrent) {
        auto propertyType = propertiesCurrent[0];
        auto propertyValue = propertiesCurrent[1];
        propertiesCurrent += 2;
        if (CL_CONTEXT_PLATFORM == propertyType) {
            Platform *pPlatform = nullptr;
            errcode = validateObject(withCastToInternal(reinterpret_cast<cl_platform_id>(propertyValue), &pPlatform));
            return pPlatform;
        }
    }
    return nullptr;
}

bool Context::isSingleDeviceContext() {
    return devices[0]->getNumGenericSubDevices() == 0 && getNumDevices() == 1;
}
} // namespace NEO
