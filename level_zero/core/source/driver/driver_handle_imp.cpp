/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/debugger/debugger_l0.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"

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

    *phContext = context->toHandle();

    if (numDevices == 0) {
        context->getDevices().resize(numDevices);
        context->getDevices() = this->devices;
    } else {
        for (uint32_t i = 0; i < numDevices; i++) {
            context->getDevices().push_back(Device::fromHandle(phDevices[i]));
        }
    }

    for (auto device : context->getDevices()) {
        auto neoDevice = device->getNEODevice();
        context->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        context->subDeviceBitfields.insert({neoDevice->getRootDeviceIndex(),
                                            neoDevice->getDeviceBitfield()});
    }

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
    *version = ZE_API_VERSION_1_0;
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

inline ze_memory_type_t parseUSMType(InternalMemoryType memoryType) {
    switch (memoryType) {
    case InternalMemoryType::SHARED_UNIFIED_MEMORY:
        return ZE_MEMORY_TYPE_SHARED;
    case InternalMemoryType::DEVICE_UNIFIED_MEMORY:
        return ZE_MEMORY_TYPE_DEVICE;
    case InternalMemoryType::HOST_UNIFIED_MEMORY:
        return ZE_MEMORY_TYPE_HOST;
    default:
        return ZE_MEMORY_TYPE_UNKNOWN;
    }

    return ZE_MEMORY_TYPE_UNKNOWN;
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
                  extension.first.c_str(), extension.first.length() + 1);
        pExtensionProperties[i].version = extension.second;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::getMemAllocProperties(const void *ptr,
                                                   ze_memory_allocation_properties_t *pMemAllocProperties,
                                                   ze_device_handle_t *phDevice) {
    auto alloc = svmAllocsManager->getSVMAlloc(ptr);
    if (nullptr == alloc) {
        pMemAllocProperties->type = ZE_MEMORY_TYPE_UNKNOWN;
        return ZE_RESULT_SUCCESS;
    }

    pMemAllocProperties->type = parseUSMType(alloc->memoryType);
    pMemAllocProperties->id = alloc->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();

    if (phDevice != nullptr) {
        if (alloc->device == nullptr) {
            *phDevice = nullptr;
        } else {
            auto device = static_cast<NEO::Device *>(alloc->device)->getSpecializedDevice<DeviceImp>();
            DEBUG_BREAK_IF(device == nullptr);
            *phDevice = device->toHandle();
        }
    }

    if (pMemAllocProperties->pNext) {
        ze_base_properties_t *extendedProperties =
            reinterpret_cast<ze_base_properties_t *>(pMemAllocProperties->pNext);
        if (extendedProperties->stype == ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD) {
            ze_external_memory_export_fd_t *extendedMemoryExportProperties =
                reinterpret_cast<ze_external_memory_export_fd_t *>(extendedProperties);
            if (extendedMemoryExportProperties->flags & ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
            }
            if (pMemAllocProperties->type != ZE_MEMORY_TYPE_DEVICE) {
                return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }
            uint64_t handle = alloc->gpuAllocations.getDefaultGraphicsAllocation()->peekInternalHandle(this->getMemoryManager());
            extendedMemoryExportProperties->fd = static_cast<int>(handle);
        }
    }

    return ZE_RESULT_SUCCESS;
}

DriverHandleImp::~DriverHandleImp() {
    for (auto &device : this->devices) {
        delete device;
    }
    if (this->svmAllocsManager) {
        delete this->svmAllocsManager;
        this->svmAllocsManager = nullptr;
    }
}

ze_result_t DriverHandleImp::initialize(std::vector<std::unique_ptr<NEO::Device>> neoDevices) {
    if (enablePciIdDeviceOrder) {
        sortNeoDevices(neoDevices);
    }

    bool multiOsContextDriver = false;
    for (auto &neoDevice : neoDevices) {
        if (!neoDevice->getHardwareInfo().capabilityTable.levelZeroSupported) {
            continue;
        }

        if (this->memoryManager == nullptr) {
            this->memoryManager = neoDevice->getMemoryManager();
            if (this->memoryManager == nullptr) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        if (enableProgramDebugging) {
            if (neoDevice->getDebugger() != nullptr) {
                NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                                      "%s", "Source Level Debugger cannot be used with Environment Variable enabling program debugging.\n");
                UNRECOVERABLE_IF(neoDevice->getDebugger() != nullptr && enableProgramDebugging);
            }
            neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger = DebuggerL0::create(neoDevice.get());
        }

        this->rootDeviceIndices.insert(neoDevice->getRootDeviceIndex());
        this->deviceBitfields.insert({neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield()});

        auto pNeoDevice = neoDevice.release();
        auto device = Device::create(this, pNeoDevice, pNeoDevice->getExecutionEnvironment()->rootDeviceEnvironments[pNeoDevice->getRootDeviceIndex()]->deviceAffinityMask, false);
        this->devices.push_back(device);

        multiOsContextDriver |= device->isMultiDeviceCapable();
    }

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

    if (NEO::DebugManager.flags.EnableHostPointerImport.get() == 1) {
        createHostPointerManager();
    }

    return ZE_RESULT_SUCCESS;
}

DriverHandle *DriverHandle::create(std::vector<std::unique_ptr<NEO::Device>> devices, const L0EnvVariables &envVariables) {
    DriverHandleImp *driverHandle = new DriverHandleImp;
    UNRECOVERABLE_IF(nullptr == driverHandle);

    driverHandle->enableProgramDebugging = envVariables.programDebugging;
    driverHandle->enableSysman = envVariables.sysman;
    driverHandle->enablePciIdDeviceOrder = envVariables.pciIdDeviceOrder;
    ze_result_t res = driverHandle->initialize(std::move(devices));
    if (res != ZE_RESULT_SUCCESS) {
        delete driverHandle;
        return nullptr;
    }

    GlobalDriver = driverHandle;

    driverHandle->getMemoryManager()->setForceNonSvmForExternalHostPtr(true);

    return driverHandle;
}

ze_result_t DriverHandleImp::getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) {
    if (*pCount == 0) {
        *pCount = this->numDevices;
        return ZE_RESULT_SUCCESS;
    }

    if (phDevices == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < *pCount; i++) {
        phDevices[i] = this->devices[i];
    }

    return ZE_RESULT_SUCCESS;
}

bool DriverHandleImp::findAllocationDataForRange(const void *buffer,
                                                 size_t size,
                                                 NEO::SvmAllocationData **allocData) {
    // Make sure the host buffer does not overlap any existing allocation
    const char *baseAddress = reinterpret_cast<const char *>(buffer);
    NEO::SvmAllocationData *beginAllocData = svmAllocsManager->getSVMAlloc(baseAddress);
    NEO::SvmAllocationData *endAllocData = svmAllocsManager->getSVMAlloc(baseAddress + size - 1);

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

ze_result_t DriverHandleImp::createEventPool(const ze_event_pool_desc_t *desc,
                                             uint32_t numDevices,
                                             ze_device_handle_t *phDevices,
                                             ze_event_pool_handle_t *phEventPool) {
    EventPool *eventPool = EventPool::create(this, numDevices, phDevices, desc);

    if (eventPool == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *phEventPool = eventPool->toHandle();

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::openEventPoolIpcHandle(ze_ipc_event_pool_handle_t hIpc,
                                                    ze_event_pool_handle_t *phEventPool) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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

} // namespace L0
