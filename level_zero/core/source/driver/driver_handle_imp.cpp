/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/source/fabric/fabric.h"

#include "driver_version_l0.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace L0 {

struct DriverHandleImp *GlobalDriver;

DriverHandleImp::DriverHandleImp() = default;

ze_result_t DriverHandleImp::createContext(const ze_context_desc_t *desc,
                                           uint32_t numDevices,
                                           ze_device_handle_t *phDevices,
                                           ze_context_handle_t *phContext) {
    ContextImp *context = new ContextImp(this);
    if (nullptr == context) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    if (desc->pNext) {
        const ze_base_desc_t *expDesc = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);
        if (expDesc->stype == ZE_STRUCTURE_TYPE_POWER_SAVING_HINT_EXP_DESC) {
            const ze_context_power_saving_hint_exp_desc_t *powerHintExpDesc =
                reinterpret_cast<const ze_context_power_saving_hint_exp_desc_t *>(expDesc);
            if (powerHintExpDesc->hint == ZE_POWER_SAVING_HINT_TYPE_MIN || powerHintExpDesc->hint <= ZE_POWER_SAVING_HINT_TYPE_MAX) {
                powerHint = static_cast<uint8_t>(powerHintExpDesc->hint);
            } else {
                delete context;
                return ZE_RESULT_ERROR_INVALID_ENUMERATION;
            }
        }
    }

    *phContext = context->toHandle();

    if (numDevices == 0) {
        for (auto device : this->devices) {
            auto neoDevice = device->getNEODevice();
            context->getDevices().insert(std::make_pair(neoDevice->getRootDeviceIndex(), device->toHandle()));
            context->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(),
                                             neoDevice->getDeviceBitfield()});
        }
    } else {
        for (uint32_t i = 0; i < numDevices; i++) {
            auto neoDevice = Device::fromHandle(phDevices[i])->getNEODevice();
            context->getDevices().insert(std::make_pair(neoDevice->getRootDeviceIndex(), phDevices[i]));
            context->rootDeviceIndices.push_back(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(),
                                             neoDevice->getDeviceBitfield()});
        }
    }

    context->rootDeviceIndices.remove_duplicates();

    return ZE_RESULT_SUCCESS;
}

NEO::MemoryManager *DriverHandleImp::getMemoryManager() {
    return this->memoryManager;
}

void DriverHandleImp::setMemoryManager(NEO::MemoryManager *memoryManager) {
    this->memoryManager = memoryManager;
}

NEO::SVMAllocsManager *DriverHandleImp::getSvmAllocsManager() {
    return this->svmAllocsManager;
}

ze_result_t DriverHandleImp::getApiVersion(ze_api_version_t *version) {
    *version = ZE_API_VERSION_1_3;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::getProperties(ze_driver_properties_t *properties) {
    uint32_t versionMajor = static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MAJOR, NULL, 10));
    uint32_t versionMinor = static_cast<uint32_t>(strtoul(L0_PROJECT_VERSION_MINOR, NULL, 10));
    uint32_t versionBuild = static_cast<uint32_t>(strtoul(NEO_VERSION_BUILD, NULL, 10));

    properties->driverVersion = ((versionMajor << 24) & 0xFF000000) |
                                ((versionMinor << 16) & 0x00FF0000) |
                                (versionBuild & 0x0000FFFF);

    uint64_t uniqueId = (properties->driverVersion) | (uuidTimestamp & 0xFFFFFFFF00000000);
    memcpy_s(properties->uuid.id, sizeof(uniqueId), &uniqueId, sizeof(uniqueId));

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties) {
    pIPCProperties->flags = ZE_IPC_PROPERTY_FLAG_MEMORY;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::getExtensionFunctionAddress(const char *pFuncName, void **pfunc) {
    auto funcAddr = extensionFunctionsLookupMap.find(std::string(pFuncName));
    if (funcAddr != extensionFunctionsLookupMap.end()) {
        *pfunc = funcAddr->second;
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t DriverHandleImp::getExtensionProperties(uint32_t *pCount,
                                                    ze_driver_extension_properties_t *pExtensionProperties) {
    if (nullptr == pExtensionProperties) {
        *pCount = static_cast<uint32_t>(this->extensionsSupported.size());
        return ZE_RESULT_SUCCESS;
    }

    *pCount = std::min(static_cast<uint32_t>(this->extensionsSupported.size()), *pCount);

    for (uint32_t i = 0; i < *pCount; i++) {
        auto extension = this->extensionsSupported[i];
        strncpy_s(pExtensionProperties[i].name, ZE_MAX_EXTENSION_NAME,
                  extension.first.c_str(), extension.first.length());
        pExtensionProperties[i].version = extension.second;
    }

    return ZE_RESULT_SUCCESS;
}

DriverHandleImp::~DriverHandleImp() {
    if (memoryManager != nullptr) {
        memoryManager->peekExecutionEnvironment().prepareForCleanup();
        if (this->svmAllocsManager) {
            this->svmAllocsManager->trimUSMDeviceAllocCache();
        }
    }

    for (auto &device : this->devices) {
        delete device;
    }
    if (this->svmAllocsManager) {
        delete this->svmAllocsManager;
        this->svmAllocsManager = nullptr;
    }
}

void DriverHandleImp::updateRootDeviceBitFields(std::unique_ptr<NEO::Device> &neoDevice) {
    const auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
    auto entry = this->deviceBitfields.find(rootDeviceIndex);
    entry->second = neoDevice->getDeviceBitfield();
}

void DriverHandleImp::enableRootDeviceDebugger(std::unique_ptr<NEO::Device> &neoDevice) {
    if (enableProgramDebugging) {
        const auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex].get();
        rootDeviceEnvironment->initDebuggerL0(neoDevice.get());
    }
}

ze_result_t DriverHandleImp::initialize(std::vector<std::unique_ptr<NEO::Device>> neoDevices) {
    bool multiOsContextDriver = false;
    for (auto &neoDevice : neoDevices) {
        ze_result_t returnValue = ZE_RESULT_SUCCESS;
        if (!neoDevice->getHardwareInfo().capabilityTable.levelZeroSupported) {
            continue;
        }

        if (this->memoryManager == nullptr) {
            this->memoryManager = neoDevice->getMemoryManager();
            if (this->memoryManager == nullptr) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        const auto rootDeviceIndex = neoDevice->getRootDeviceIndex();

        enableRootDeviceDebugger(neoDevice);

        this->rootDeviceIndices.push_back(rootDeviceIndex);
        this->deviceBitfields.insert({rootDeviceIndex, neoDevice->getDeviceBitfield()});

        auto pNeoDevice = neoDevice.release();

        auto device = Device::create(this, pNeoDevice, false, &returnValue);
        this->devices.push_back(device);

        multiOsContextDriver |= device->isImplicitScalingCapable();
        if (returnValue != ZE_RESULT_SUCCESS) {
            return returnValue;
        }
    }
    this->rootDeviceIndices.remove_duplicates();

    if (this->devices.size() == 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    this->svmAllocsManager = new NEO::SVMAllocsManager(memoryManager, multiOsContextDriver);
    if (this->svmAllocsManager == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    this->numDevices = static_cast<uint32_t>(this->devices.size());

    extensionFunctionsLookupMap = getExtensionFunctionsLookupMap();

    uuidTimestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());

    if (NEO::DebugManager.flags.EnableHostPointerImport.get() != 0) {
        createHostPointerManager();
    }

    return ZE_RESULT_SUCCESS;
}

DriverHandle *DriverHandle::create(std::vector<std::unique_ptr<NEO::Device>> devices, const L0EnvVariables &envVariables, ze_result_t *returnValue) {
    DriverHandleImp *driverHandle = new DriverHandleImp;
    UNRECOVERABLE_IF(nullptr == driverHandle);

    driverHandle->enableProgramDebugging = envVariables.programDebugging;
    driverHandle->enableSysman = envVariables.sysman;
    driverHandle->enablePciIdDeviceOrder = envVariables.pciIdDeviceOrder;
    ze_result_t res = driverHandle->initialize(std::move(devices));
    if (res != ZE_RESULT_SUCCESS) {
        delete driverHandle;
        *returnValue = res;
        return nullptr;
    }

    GlobalDriver = driverHandle;

    driverHandle->getMemoryManager()->setForceNonSvmForExternalHostPtr(true);

    return driverHandle;
}

ze_result_t DriverHandleImp::getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) {
    bool exposeSubDevices = false;

    if (NEO::DebugManager.flags.ReturnSubDevicesAsApiDevices.get() != -1) {
        exposeSubDevices = NEO::DebugManager.flags.ReturnSubDevicesAsApiDevices.get();
    }

    if (*pCount == 0) {
        if (exposeSubDevices) {
            for (auto &device : this->devices) {
                auto deviceImpl = static_cast<DeviceImp *>(device);
                *pCount += (deviceImpl->numSubDevices > 0 ? deviceImpl->numSubDevices : 1u);
            }
        } else {
            *pCount = this->numDevices;
        }

        return ZE_RESULT_SUCCESS;
    }

    if (phDevices == nullptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    uint32_t i = 0;
    for (auto device : devices) {
        auto deviceImpl = static_cast<DeviceImp *>(device);
        if (deviceImpl->numSubDevices > 0 && exposeSubDevices) {
            for (auto subdevice : deviceImpl->subDevices) {
                phDevices[i++] = subdevice;
                if (i == *pCount) {
                    return ZE_RESULT_SUCCESS;
                }
            }
        } else {
            phDevices[i++] = device;
            if (i == *pCount) {
                return ZE_RESULT_SUCCESS;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

bool DriverHandleImp::findAllocationDataForRange(const void *buffer,
                                                 size_t size,
                                                 NEO::SvmAllocationData **allocData) {

    size_t offset = 0;
    if (size > 0) {
        offset = size - 1;
    }

    // Make sure the host buffer does not overlap any existing allocation
    const char *baseAddress = reinterpret_cast<const char *>(buffer);
    NEO::SvmAllocationData *beginAllocData = svmAllocsManager->getSVMAlloc(baseAddress);
    NEO::SvmAllocationData *endAllocData = svmAllocsManager->getSVMAlloc(baseAddress + offset);

    if (allocData) {
        if (beginAllocData) {
            *allocData = beginAllocData;
        } else {
            *allocData = endAllocData;
        }
    }

    // Return true if the whole range requested is covered by the same allocation
    if (beginAllocData && endAllocData &&
        (beginAllocData->gpuAllocations.getDefaultGraphicsAllocation() == endAllocData->gpuAllocations.getDefaultGraphicsAllocation())) {
        return true;
    }
    return false;
}

std::vector<NEO::SvmAllocationData *> DriverHandleImp::findAllocationsWithinRange(const void *buffer,
                                                                                  size_t size,
                                                                                  bool *allocationRangeCovered) {
    std::vector<NEO::SvmAllocationData *> allocDataArray;
    const char *baseAddress = reinterpret_cast<const char *>(buffer);
    // Check if the host buffer overlaps any existing allocation
    NEO::SvmAllocationData *beginAllocData = svmAllocsManager->getSVMAlloc(baseAddress);
    NEO::SvmAllocationData *endAllocData = svmAllocsManager->getSVMAlloc(baseAddress + size - 1);

    // Add the allocation that matches the beginning address
    if (beginAllocData) {
        allocDataArray.push_back(beginAllocData);
    }
    // Add the allocation that matches the end address range if there was no beginning allocation
    // or the beginning allocation does not match the ending allocation
    if (endAllocData) {
        if ((beginAllocData && (beginAllocData->gpuAllocations.getDefaultGraphicsAllocation() != endAllocData->gpuAllocations.getDefaultGraphicsAllocation())) ||
            !beginAllocData) {
            allocDataArray.push_back(endAllocData);
        }
    }

    // Return true if the whole range requested is covered by the same allocation
    if (beginAllocData && endAllocData &&
        (beginAllocData->gpuAllocations.getDefaultGraphicsAllocation() == endAllocData->gpuAllocations.getDefaultGraphicsAllocation())) {
        *allocationRangeCovered = true;
    } else {
        *allocationRangeCovered = false;
    }
    return allocDataArray;
}

void DriverHandleImp::createHostPointerManager() {
    hostPointerManager = std::make_unique<HostPointerManager>(getMemoryManager());
}

ze_result_t DriverHandleImp::importExternalPointer(void *ptr, size_t size) {
    if (hostPointerManager.get() != nullptr) {
        auto ret = hostPointerManager->createHostPointerMultiAllocation(this->devices,
                                                                        ptr,
                                                                        size);
        return ret;
    }

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DriverHandleImp::releaseImportedPointer(void *ptr) {
    if (hostPointerManager.get() != nullptr) {
        bool ret = hostPointerManager->freeHostPointerAllocation(ptr);
        return ret ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DriverHandleImp::getHostPointerBaseAddress(void *ptr, void **baseAddress) {
    if (hostPointerManager.get() != nullptr) {
        auto hostPointerData = hostPointerManager->getHostPointerAllocation(ptr);
        if (hostPointerData != nullptr) {
            if (baseAddress != nullptr) {
                *baseAddress = hostPointerData->basePtr;
            }
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

NEO::GraphicsAllocation *DriverHandleImp::findHostPointerAllocation(void *ptr, size_t size, uint32_t rootDeviceIndex) {
    if (hostPointerManager.get() != nullptr) {
        HostPointerData *hostData = hostPointerManager->getHostPointerAllocation(ptr);
        if (hostData != nullptr) {
            size_t foundEndSize = reinterpret_cast<size_t>(hostData->basePtr) + hostData->size;
            size_t inputEndSize = reinterpret_cast<size_t>(ptr) + size;
            if (foundEndSize >= inputEndSize) {
                return hostData->hostPtrAllocations.getGraphicsAllocation(rootDeviceIndex);
            }
            return nullptr;
        }

        if (NEO::DebugManager.flags.ForceHostPointerImport.get() == 1) {
            importExternalPointer(ptr, size);
            return hostPointerManager->getHostPointerAllocation(ptr)->hostPtrAllocations.getGraphicsAllocation(rootDeviceIndex);
        }
        return nullptr;
    }

    return nullptr;
}

NEO::GraphicsAllocation *DriverHandleImp::getDriverSystemMemoryAllocation(void *ptr,
                                                                          size_t size,
                                                                          uint32_t rootDeviceIndex,
                                                                          uintptr_t *gpuAddress) {
    NEO::SvmAllocationData *allocData = nullptr;
    bool allocFound = findAllocationDataForRange(ptr, size, &allocData);
    if (allocFound) {
        if (gpuAddress != nullptr) {
            *gpuAddress = reinterpret_cast<uintptr_t>(ptr);
        }
        return allocData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    }
    auto allocation = findHostPointerAllocation(ptr, size, rootDeviceIndex);
    if (allocation != nullptr) {
        if (gpuAddress != nullptr) {
            uintptr_t offset = reinterpret_cast<uintptr_t>(ptr) -
                               reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer());
            *gpuAddress = static_cast<uintptr_t>(allocation->getGpuAddress()) + offset;
        }
    }
    return allocation;
}

bool DriverHandleImp::isRemoteResourceNeeded(void *ptr, NEO::GraphicsAllocation *alloc, NEO::SvmAllocationData *allocData, Device *device) {
    return (alloc == nullptr || (allocData && ((allocData->gpuAllocations.getGraphicsAllocations().size() - 1) < device->getRootDeviceIndex())));
}

void *DriverHandleImp::importFdHandle(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, uint64_t handle, NEO::GraphicsAllocation **pAlloc) {
    NEO::osHandle osHandle = static_cast<NEO::osHandle>(handle);
    NEO::AllocationProperties unifiedMemoryProperties{neoDevice->getRootDeviceIndex(),
                                                      MemoryConstants::pageSize,
                                                      NEO::AllocationType::BUFFER,
                                                      neoDevice->getDeviceBitfield()};
    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();
    NEO::GraphicsAllocation *alloc =
        this->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandle,
                                                                           unifiedMemoryProperties,
                                                                           false,
                                                                           false);
    if (alloc == nullptr) {
        return nullptr;
    }

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(alloc);
    allocData.cpuAllocation = nullptr;
    allocData.size = alloc->getUnderlyingBufferSize();
    allocData.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    allocData.device = neoDevice;
    allocData.isImportedAllocation = true;
    if (flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        allocData.allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    if (flags & ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED) {
        allocData.allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    this->getSvmAllocsManager()->insertSVMAlloc(allocData);

    if (pAlloc) {
        *pAlloc = alloc;
    }

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

void *DriverHandleImp::importFdHandles(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, std::vector<NEO::osHandle> handles, NEO::GraphicsAllocation **pAlloc) {
    NEO::AllocationProperties unifiedMemoryProperties{neoDevice->getRootDeviceIndex(),
                                                      MemoryConstants::pageSize,
                                                      NEO::AllocationType::BUFFER,
                                                      neoDevice->getDeviceBitfield()};
    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();

    NEO::GraphicsAllocation *alloc =
        this->getMemoryManager()->createGraphicsAllocationFromMultipleSharedHandles(handles,
                                                                                    unifiedMemoryProperties,
                                                                                    false,
                                                                                    false);
    if (alloc == nullptr) {
        return nullptr;
    }

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(alloc);
    allocData.cpuAllocation = nullptr;
    allocData.size = alloc->getUnderlyingBufferSize();
    allocData.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    allocData.device = neoDevice;
    if (flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        allocData.allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    if (flags & ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED) {
        allocData.allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    this->getSvmAllocsManager()->insertSVMAlloc(allocData);

    if (pAlloc) {
        *pAlloc = alloc;
    }

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

NEO::GraphicsAllocation *DriverHandleImp::getPeerAllocation(Device *device,
                                                            NEO::SvmAllocationData *allocData,
                                                            void *basePtr,
                                                            uintptr_t *peerGpuAddress) {
    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    NEO::GraphicsAllocation *alloc = nullptr;

    NEO::SvmAllocationData *peerAllocData = nullptr;
    void *peerPtr = nullptr;

    std::unique_lock<NEO::SpinLock> lock(deviceImp->peerAllocationsMutex);

    auto iter = deviceImp->peerAllocations.allocations.find(basePtr);
    if (iter != deviceImp->peerAllocations.allocations.end()) {
        peerAllocData = &iter->second;
        alloc = peerAllocData->gpuAllocations.getDefaultGraphicsAllocation();
        UNRECOVERABLE_IF(alloc == nullptr);
        peerPtr = reinterpret_cast<void *>(alloc->getGpuAddress());
    } else {
        alloc = allocData->gpuAllocations.getDefaultGraphicsAllocation();
        UNRECOVERABLE_IF(alloc == nullptr);
        ze_ipc_memory_flags_t flags = {};
        uint32_t numHandles = alloc->getNumHandles();

        if (numHandles > 1) {
            UNRECOVERABLE_IF(numHandles == 0);
            std::vector<NEO::osHandle> handles;
            for (uint32_t i = 0; i < numHandles; i++) {
                int handle = static_cast<int>(alloc->peekInternalHandle(this->getMemoryManager(), i));
                handles.push_back(handle);
            }
            auto neoDevice = device->getNEODevice()->getRootDevice();
            peerPtr = this->importFdHandles(neoDevice, flags, handles, &alloc);
        } else {
            uint64_t handle = alloc->peekInternalHandle(this->getMemoryManager());
            peerPtr = this->importFdHandle(device->getNEODevice(), flags, handle, &alloc);
        }

        if (peerPtr == nullptr) {
            return nullptr;
        }

        peerAllocData = this->getSvmAllocsManager()->getSVMAlloc(peerPtr);
        deviceImp->peerAllocations.allocations.insert(std::make_pair(basePtr, *peerAllocData));
    }

    if (peerGpuAddress) {
        *peerGpuAddress = reinterpret_cast<uintptr_t>(peerPtr);
    }

    return alloc;
}

void *DriverHandleImp::importNTHandle(ze_device_handle_t hDevice, void *handle) {
    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();

    auto alloc = this->getMemoryManager()->createGraphicsAllocationFromNTHandle(handle, neoDevice->getRootDeviceIndex(), NEO::AllocationType::SHARED_BUFFER);

    if (alloc == nullptr) {
        return nullptr;
    }

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(alloc);
    allocData.cpuAllocation = nullptr;
    allocData.size = alloc->getUnderlyingBufferSize();
    allocData.memoryType = InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    allocData.device = neoDevice;

    this->getSvmAllocsManager()->insertSVMAlloc(allocData);

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

ze_result_t DriverHandleImp::checkMemoryAccessFromDevice(Device *device, const void *ptr) {
    auto allocation = svmAllocsManager->getSVMAlloc(ptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (allocation->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
        allocation->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY)
        return ZE_RESULT_SUCCESS;

    if (allocation->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex()) != nullptr) {
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t DriverHandleImp::fabricVertexGetExp(uint32_t *pCount, ze_fabric_vertex_handle_t *phVertices) {

    uint32_t deviceCount = 0;
    getDevice(&deviceCount, nullptr);

    if (*pCount == 0) {
        *pCount = deviceCount;
        return ZE_RESULT_SUCCESS;
    }

    std::vector<ze_device_handle_t> deviceHandles;
    deviceHandles.resize(deviceCount);
    getDevice(&deviceCount, deviceHandles.data());

    *pCount = std::min(deviceCount, *pCount);

    for (uint32_t index = 0; index < *pCount; index++) {
        auto deviceImp = static_cast<DeviceImp *>(deviceHandles[index]);
        phVertices[index] = deviceImp->fabricVertex->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
