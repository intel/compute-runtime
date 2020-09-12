/*
 * Copyright (C) 2019-2020 Intel Corporation
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

#include "driver_version_l0.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace L0 {

struct DriverHandleImp *GlobalDriver;

ze_result_t DriverHandleImp::createContext(const ze_context_desc_t *desc,
                                           ze_context_handle_t *phContext) {
    ContextImp *context = new ContextImp(this);
    if (nullptr == context) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *phContext = context->toHandle();

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
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DriverHandleImp::getMemAllocProperties(const void *ptr,
                                                   ze_memory_allocation_properties_t *pMemAllocProperties,
                                                   ze_device_handle_t *phDevice) {
    auto alloc = svmAllocsManager->getSVMAlloc(ptr);
    if (alloc) {
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
        return ZE_RESULT_SUCCESS;
    }
    pMemAllocProperties->type = ZE_MEMORY_TYPE_UNKNOWN;

    return ZE_RESULT_SUCCESS;
}

DriverHandleImp::~DriverHandleImp() {
    for (auto &device : this->devices) {
        if (device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->debugger.get() &&
            !device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->debugger->isLegacy()) {
            device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->debugger.reset(nullptr);
        }
        delete device;
    }
    if (this->svmAllocsManager) {
        delete this->svmAllocsManager;
        this->svmAllocsManager = nullptr;
    }
}

uint32_t DriverHandleImp::parseAffinityMask(std::vector<std::unique_ptr<NEO::Device>> &neoDevices) {
    std::vector<std::vector<bool>> affinityMaskBitSet(neoDevices.size());
    for (uint32_t i = 0; i < affinityMaskBitSet.size(); i++) {
        affinityMaskBitSet[i].resize(neoDevices[i]->getNumAvailableDevices());
    }

    size_t pos = 0;
    while (pos < this->affinityMaskString.size()) {
        size_t posNextDot = this->affinityMaskString.find_first_of(".", pos);
        size_t posNextComma = this->affinityMaskString.find_first_of(",", pos);
        std::string rootDeviceString = this->affinityMaskString.substr(pos, std::min(posNextDot, posNextComma) - pos);
        uint32_t rootDeviceIndex = static_cast<uint32_t>(std::stoul(rootDeviceString, nullptr, 0));
        if (rootDeviceIndex < neoDevices.size()) {
            pos += rootDeviceString.size();
            if (posNextDot != std::string::npos &&
                this->affinityMaskString.at(pos) == '.' && posNextDot < posNextComma) {
                pos++;
                std::string subDeviceString = this->affinityMaskString.substr(pos, posNextComma - pos);
                uint32_t subDeviceIndex = static_cast<uint32_t>(std::stoul(subDeviceString, nullptr, 0));
                if (subDeviceIndex < neoDevices[rootDeviceIndex]->getNumAvailableDevices()) {
                    affinityMaskBitSet[rootDeviceIndex][subDeviceIndex] = true;
                }
            } else {
                std::fill(affinityMaskBitSet[rootDeviceIndex].begin(),
                          affinityMaskBitSet[rootDeviceIndex].end(),
                          true);
            }
        }
        if (posNextComma == std::string::npos) {
            break;
        }
        pos = posNextComma + 1;
    }

    uint32_t offset = 0;
    uint32_t affinityMask = 0;
    for (uint32_t i = 0; i < affinityMaskBitSet.size(); i++) {
        for (uint32_t j = 0; j < affinityMaskBitSet[i].size(); j++) {
            if (affinityMaskBitSet[i][j] == true) {
                affinityMask |= (1UL << offset);
            }
            offset++;
        }
    }

    return affinityMask;
}

ze_result_t DriverHandleImp::initialize(std::vector<std::unique_ptr<NEO::Device>> neoDevices) {

    uint32_t affinityMask = std::numeric_limits<uint32_t>::max();

    if (this->affinityMaskString.length() > 0) {
        affinityMask = parseAffinityMask(neoDevices);
    }

    uint32_t currentMaskOffset = 0;
    for (auto &neoDevice : neoDevices) {
        if (!neoDevice->getHardwareInfo().capabilityTable.levelZeroSupported) {
            continue;
        }

        uint32_t currentDeviceMask = (affinityMask >> currentMaskOffset) & ((1UL << neoDevice->getNumAvailableDevices()) - 1);
        bool isDeviceExposed = currentDeviceMask ? true : false;

        currentMaskOffset += neoDevice->getNumAvailableDevices();
        if (!isDeviceExposed) {
            continue;
        }

        if (this->memoryManager == nullptr) {
            this->memoryManager = neoDevice->getMemoryManager();
            if (this->memoryManager == nullptr) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }

            this->svmAllocsManager = new NEO::SVMAllocsManager(memoryManager);
            if (this->svmAllocsManager == nullptr) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        if (enableProgramDebugging) {
            UNRECOVERABLE_IF(neoDevice->getDebugger() != nullptr && enableProgramDebugging);
            neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger = DebuggerL0::create(neoDevice.get());
        }

        auto device = Device::create(this, neoDevice.release(), currentDeviceMask, false);
        this->devices.push_back(device);
    }

    if (this->devices.size() == 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    this->numDevices = static_cast<uint32_t>(this->devices.size());

    extensionFunctionsLookupMap = getExtensionFunctionsLookupMap();

    uuidTimestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());

    return ZE_RESULT_SUCCESS;
}

DriverHandle *DriverHandle::create(std::vector<std::unique_ptr<NEO::Device>> devices, const L0EnvVariables &envVariables) {
    DriverHandleImp *driverHandle = new DriverHandleImp;
    UNRECOVERABLE_IF(nullptr == driverHandle);

    driverHandle->affinityMaskString = envVariables.affinityMask;
    driverHandle->enableProgramDebugging = envVariables.programDebugging;
    driverHandle->enableSysman = envVariables.sysman;

    ze_result_t res = driverHandle->initialize(std::move(devices));
    if (res != ZE_RESULT_SUCCESS) {
        delete driverHandle;
        return nullptr;
    }

    GlobalDriver = driverHandle;

    driverHandle->memoryManager->setForceNonSvmForExternalHostPtr(true);

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

} // namespace L0
