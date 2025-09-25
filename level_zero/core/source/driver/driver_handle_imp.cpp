/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/usm_pool_params.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/logger.h"
#include "shared/source/utilities/staging_buffer_manager.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/driver/extension_function_address.h"
#include "level_zero/core/source/driver/extension_injector.h"
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/default_descriptors.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/semaphore/external_semaphore_imp.h"
#include "level_zero/driver_experimental/zex_common.h"

#include "driver_version.h"

#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

namespace L0 {

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
    context->initDeviceHandles(numDevices, phDevices);
    if (numDevices == 0) {
        for (auto device : this->devices) {
            auto neoDevice = device->getNEODevice();
            context->getDevices().insert(std::make_pair(neoDevice->getRootDeviceIndex(), device->toHandle()));
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(),
                                             neoDevice->getDeviceBitfield()});
            context->addDeviceHandle(device->toHandle());
        }
    } else {
        for (uint32_t i = 0; i < numDevices; i++) {
            auto neoDevice = Device::fromHandle(phDevices[i])->getNEODevice();
            context->getDevices().insert(std::make_pair(neoDevice->getRootDeviceIndex(), phDevices[i]));
            context->rootDeviceIndices.pushUnique(neoDevice->getRootDeviceIndex());
            context->deviceBitfields.insert({neoDevice->getRootDeviceIndex(),
                                             neoDevice->getDeviceBitfield()});
        }
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

NEO::StagingBufferManager *DriverHandleImp::getStagingBufferManager() {
    return this->stagingBufferManager.get();
}

ze_result_t DriverHandleImp::getApiVersion(ze_api_version_t *version) {
    *version = static_cast<ze_api_version_t>(ZE_MAKE_VERSION(NEO_L0_VERSION_MAJOR, NEO_L0_VERSION_MINOR));
    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::getProperties(ze_driver_properties_t *properties) {
    uint32_t versionBuild = static_cast<uint32_t>(NEO_VERSION_BUILD);

    properties->driverVersion = DriverHandleImp::initialDriverVersionValue + versionBuild;
    if (NEO::debugManager.flags.OverrideDriverVersion.get() > -1) {
        properties->driverVersion = static_cast<uint32_t>(NEO::debugManager.flags.OverrideDriverVersion.get());
    }

    uint64_t uniqueId = (properties->driverVersion) | (uuidTimestamp & 0xFFFFFFFF00000000);
    memcpy_s(properties->uuid.id, sizeof(uniqueId), &uniqueId, sizeof(uniqueId));

    auto pNext = reinterpret_cast<ze_base_properties_t *>(properties->pNext);
    if (properties->stype != ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES) {
        pNext = nullptr;
    }
    while (pNext) {
        if (pNext->stype == ZE_STRUCTURE_TYPE_DRIVER_DDI_HANDLES_EXT_PROPERTIES && NEO::debugManager.flags.EnableDdiHandlesExtension.get()) {
            ze_driver_ddi_handles_ext_properties_t *pDdiHandlesExtProperties = reinterpret_cast<ze_driver_ddi_handles_ext_properties_t *>(pNext);
            pDdiHandlesExtProperties->flags = ze_driver_ddi_handle_ext_flag_t::ZE_DRIVER_DDI_HANDLE_EXT_FLAG_DDI_HANDLE_EXT_SUPPORTED;
        }
        pNext = reinterpret_cast<ze_base_properties_t *>(pNext->pNext);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::getIPCProperties(ze_driver_ipc_properties_t *pIPCProperties) {
    pIPCProperties->flags = ZE_IPC_PROPERTY_FLAG_MEMORY;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::getExtensionFunctionAddress(const char *pFuncName, void **pfunc) {
    *pfunc = ExtensionFunctionAddressHelper::getExtensionFunctionAddress(pFuncName);
    if (*pfunc) {
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t DriverHandleImp::getExtensionProperties(uint32_t *pCount,
                                                    ze_driver_extension_properties_t *pExtensionProperties) {

    std::vector<std::pair<std::string, uint32_t>> additionalExtensions;

    bool isBfloat16Supported = false;
    bool isBindlessHeapsSupported = false;
    for (const auto device : devices) {
        if (device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper()) {
            if (device->getNEODevice()->getRootDeviceEnvironment().getReleaseHelper()->isBFloat16ConversionSupported()) {
                isBfloat16Supported = true;
            }
        }
        if (device->getNEODevice()->getRootDeviceEnvironment().getBindlessHeapsHelper()) {
            isBindlessHeapsSupported = true;
        }
        if (isBfloat16Supported && isBindlessHeapsSupported) {
            break;
        }
    }

    if (isBindlessHeapsSupported) {
        additionalExtensions.emplace_back(ZE_BINDLESS_IMAGE_EXP_NAME, ZE_BINDLESS_IMAGE_EXP_VERSION_CURRENT);
    }

    if (isBfloat16Supported) {
        additionalExtensions.emplace_back(ZE_BFLOAT16_CONVERSIONS_EXT_NAME, ZE_BFLOAT16_CONVERSIONS_EXT_VERSION_1_0);
    }

    devices[0]->getL0GfxCoreHelper().appendPlatformSpecificExtensions(additionalExtensions, devices[0]->getProductHelper(), devices[0]->getHwInfo());

    if (devices[0]->getL0GfxCoreHelper().synchronizedDispatchSupported() && devices[0]->isImplicitScalingCapable()) {
        additionalExtensions.emplace_back(ZE_SYNCHRONIZED_DISPATCH_EXP_NAME, ZE_SYNCHRONIZED_DISPATCH_EXP_VERSION_CURRENT);
    }

    if (devices[0]->getProductHelper().isInterruptSupported()) {
        additionalExtensions.emplace_back(ZEX_INTEL_EVENT_SYNC_MODE_EXP_NAME, ZEX_INTEL_EVENT_SYNC_MODE_EXP_VERSION_CURRENT);
    }

    NEO::OSInterface *osInterface = devices[0]->getOsInterface();
    if (osInterface && osInterface->getDriverModel()->getDriverModelType() == NEO::DriverModelType::drm) {
        additionalExtensions.emplace_back(ZE_CACHE_RESERVATION_EXT_NAME, ZE_CACHE_RESERVATION_EXT_VERSION_CURRENT);
    }

    ExtensionInjectorHelper::addAdditionalExtensions(additionalExtensions, devices[0]);

    auto extensionCount = static_cast<uint32_t>(this->extensionsSupported.size() + additionalExtensions.size());

    if (nullptr == pExtensionProperties) {
        *pCount = extensionCount;
        return ZE_RESULT_SUCCESS;
    }

    *pCount = std::min(extensionCount, *pCount);

    for (uint32_t i = 0; i < *pCount; i++) {
        const auto &extension = (i < this->extensionsSupported.size()) ? this->extensionsSupported[i] : additionalExtensions[i - this->extensionsSupported.size()];
        strncpy_s(pExtensionProperties[i].name, ZE_MAX_EXTENSION_NAME,
                  extension.first.c_str(), extension.first.length());
        pExtensionProperties[i].version = extension.second;
    }

    return ZE_RESULT_SUCCESS;
}

DriverHandleImp::~DriverHandleImp() {
    for (auto &device : this->devices) {
        // release temporary pointers before default context destruction
        device->bcsSplitReleaseResources();
    }

    if (this->defaultContext) {
        L0::Context::fromHandle(this->defaultContext)->destroy();
        this->defaultContext = nullptr;
    }
    if (this->externalSemaphoreController) {
        this->externalSemaphoreController.reset();
    }

    if (memoryManager != nullptr) {
        memoryManager->peekExecutionEnvironment().prepareForCleanup();
        if (this->svmAllocsManager) {
            this->svmAllocsManager->cleanupUSMAllocCaches();
            this->usmHostMemAllocPool.cleanup();
        }
    }

    this->stagingBufferManager.reset();
    for (auto &device : this->devices) {
        if (device->getBuiltinFunctionsLib()) {
            device->getBuiltinFunctionsLib()->ensureInitCompletion();
        }
        delete device;
    }

    for (auto &fabricVertex : this->fabricVertices) {
        delete fabricVertex;
    }
    this->fabricVertices.clear();

    for (auto &edge : this->fabricEdges) {
        delete edge;
    }
    this->fabricEdges.clear();

    for (auto &edge : this->fabricIndirectEdges) {
        delete edge;
    }
    this->fabricIndirectEdges.clear();

    if (this->svmAllocsManager) {
        delete this->svmAllocsManager;
        this->svmAllocsManager = nullptr;
    }
}

void DriverHandleImp::updateRootDeviceBitFields(std::unique_ptr<NEO::Device> &neoDevice) {
    const auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
    auto entry = this->deviceBitfields.find(rootDeviceIndex);
    UNRECOVERABLE_IF(entry == this->deviceBitfields.end());
    entry->second = neoDevice->getDeviceBitfield();
}

ze_result_t DriverHandleImp::initialize(std::vector<std::unique_ptr<NEO::Device>> neoDevices) {
    this->pid = NEO::SysCalls::getCurrentProcessId();

    for (auto &neoDevice : neoDevices) {
        ze_result_t returnValue = ZE_RESULT_SUCCESS;

        if (this->memoryManager == nullptr) {
            this->memoryManager = neoDevice->getMemoryManager();
            if (this->memoryManager == nullptr) {
                return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
            }
        }

        const auto rootDeviceIndex = neoDevice->getRootDeviceIndex();

        this->rootDeviceIndices.pushUnique(rootDeviceIndex);

        this->deviceBitfields.insert({rootDeviceIndex, neoDevice->getDeviceBitfield()});

        auto pNeoDevice = neoDevice.release();

        auto device = Device::create(this, pNeoDevice, false, &returnValue);
        this->devices.push_back(device);

        if (returnValue != ZE_RESULT_SUCCESS) {
            return returnValue;
        }
    }

    this->numDevices = static_cast<uint32_t>(this->devices.size());

    if (this->numDevices == 0) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    this->svmAllocsManager = new NEO::SVMAllocsManager(memoryManager);
    if (this->svmAllocsManager == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }
    if (this->numDevices == 1) {
        this->svmAllocsManager->initUsmAllocationsCaches(*this->devices[0]->getNEODevice());
    }
    this->initHostUsmAllocPool();
    for (auto &device : this->devices) {
        this->initDeviceUsmAllocPool(*device->getNEODevice(), this->numDevices > 1);
    }

    uuidTimestamp = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());

    if (NEO::debugManager.flags.EnableHostPointerImport.get() != 0) {
        createHostPointerManager();
    }

    for (auto &device : this->devices) {
        if (device->getBuiltinFunctionsLib()) {
            device->getBuiltinFunctionsLib()->ensureInitCompletion();
        }
    }

    setupDevicesToExpose();
    uint32_t deviceIdentifier = 0u;
    for (auto &deviceToExpose : this->devicesToExpose) {
        Device::fromHandle(deviceToExpose)->setIdentifier(deviceIdentifier++);
    }
    createContext(&DefaultDescriptors::contextDesc, static_cast<uint32_t>(this->devicesToExpose.size()), this->devicesToExpose.data(), &defaultContext);
    this->stagingBufferManager = std::make_unique<NEO::StagingBufferManager>(svmAllocsManager, this->rootDeviceIndices, this->deviceBitfields, false);
    return ZE_RESULT_SUCCESS;
}

DriverHandle *DriverHandle::create(std::vector<std::unique_ptr<NEO::Device>> devices, const L0EnvVariables &envVariables, ze_result_t *returnValue) {
    DriverHandleImp *driverHandle = new DriverHandleImp;
    UNRECOVERABLE_IF(nullptr == driverHandle);

    driverHandle->enableProgramDebugging = static_cast<NEO::DebuggingMode>(envVariables.programDebugging);
    driverHandle->enableSysman = envVariables.sysman;
    driverHandle->enablePciIdDeviceOrder = envVariables.pciIdDeviceOrder;

    ze_result_t res = driverHandle->initialize(std::move(devices));
    if (res != ZE_RESULT_SUCCESS) {
        delete driverHandle;
        *returnValue = res;
        return nullptr;
    }

    driverHandle->getMemoryManager()->setForceNonSvmForExternalHostPtr(true);

    return driverHandle;
}

void DriverHandleImp::initHostUsmAllocPool() {
    auto usmHostAllocPoolingEnabled = NEO::ApiSpecificConfig::isHostUsmPoolingEnabled();
    for (auto device : this->devices) {
        usmHostAllocPoolingEnabled &= device->getNEODevice()->getProductHelper().isHostUsmPoolAllocatorSupported() &&
                                      nullptr == device->getL0Debugger() &&
                                      NEO::DeviceFactory::isHwModeSelected();
    }
    auto poolParams = NEO::UsmPoolParams::getUsmPoolParams(this->devices[0]->getNEODevice()->getGfxCoreHelper());
    if (NEO::debugManager.flags.EnableHostUsmAllocationPool.get() != -1) {
        usmHostAllocPoolingEnabled = NEO::debugManager.flags.EnableHostUsmAllocationPool.get() > 0;
        poolParams.poolSize = NEO::debugManager.flags.EnableHostUsmAllocationPool.get() * MemoryConstants::megaByte;
    }
    if (usmHostAllocPoolingEnabled) {
        NEO::SVMAllocsManager::UnifiedMemoryProperties memoryProperties(InternalMemoryType::hostUnifiedMemory, MemoryConstants::pageSize2M,
                                                                        rootDeviceIndices, deviceBitfields);
        usmHostMemAllocPool.initialize(svmAllocsManager, memoryProperties, poolParams.poolSize, poolParams.minServicedSize, poolParams.maxServicedSize);
    }
}

void DriverHandleImp::initDeviceUsmAllocPool(NEO::Device &device, bool multiDevice) {
    bool useUsmPoolManager = false;
    if (NEO::debugManager.flags.EnableUsmAllocationPoolManager.get() != -1) {
        useUsmPoolManager = !!NEO::debugManager.flags.EnableUsmAllocationPoolManager.get();
    }
    auto &hwInfo = device.getHardwareInfo();
    auto &l0GfxCoreHelper = device.getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    const bool compressionEnabledByDefault = l0GfxCoreHelper.usmCompressionSupported(hwInfo) && l0GfxCoreHelper.forceDefaultUsmCompressionSupport();
    NEO::SVMAllocsManager::UnifiedMemoryProperties poolMemoryProperties(InternalMemoryType::deviceUnifiedMemory,
                                                                        MemoryConstants::pageSize2M,
                                                                        rootDeviceIndices,
                                                                        deviceBitfields);
    poolMemoryProperties.device = &device;
    poolMemoryProperties.allocationFlags.flags.compressedHint = compressionEnabledByDefault;

    bool enabled = NEO::ApiSpecificConfig::isDeviceUsmPoolingEnabled() &&
                   device.getProductHelper().isDeviceUsmPoolAllocatorSupported() &&
                   nullptr == device.getL0Debugger() &&
                   NEO::DeviceFactory::isHwModeSelected() &&
                   !multiDevice;
    auto poolParams = NEO::UsmPoolParams::getUsmPoolParams(device.getGfxCoreHelper());
    if (NEO::debugManager.flags.EnableDeviceUsmAllocationPool.get() != -1) {
        enabled = NEO::debugManager.flags.EnableDeviceUsmAllocationPool.get() > 0;
        poolParams.poolSize = NEO::debugManager.flags.EnableDeviceUsmAllocationPool.get() * MemoryConstants::megaByte;
    }

    if (enabled) {
        if (useUsmPoolManager) {
            device.resetUsmAllocationPoolManager(new NEO::UsmMemAllocPoolsManager(InternalMemoryType::deviceUnifiedMemory, rootDeviceIndices, deviceBitfields, &device));
            device.getUsmMemAllocPoolsManager()->initialize(this->svmAllocsManager);
        } else {
            device.resetUsmAllocationPool(new NEO::UsmMemAllocPool);
            device.getUsmMemAllocPool()->initialize(this->svmAllocsManager, poolMemoryProperties, poolParams.poolSize, poolParams.minServicedSize, poolParams.maxServicedSize);
        }
    }
}

void DriverHandleImp::setupDevicesToExpose() {

    // If the user has requested FLAT or COMBINED device hierarchy model, then report all the sub devices as devices.
    bool exposeSubDevices = (this->devices.size() && this->devices[0]->getNEODevice()->getExecutionEnvironment()->getDeviceHierarchyMode() != NEO::DeviceHierarchyMode::composite);

    uint32_t numDevices = 0;
    if (exposeSubDevices) {
        for (auto &device : this->devices) {

            if (device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->isExposeSingleDeviceMode()) {
                numDevices += 1;
                continue;
            }

            auto deviceImpl = static_cast<DeviceImp *>(device);
            numDevices += (deviceImpl->numSubDevices > 0 ? deviceImpl->numSubDevices : 1u);
        }
    } else {
        numDevices = this->numDevices;
    }
    this->devicesToExpose.clear();
    this->devicesToExpose.reserve(numDevices);

    for (auto device : devices) {

        auto deviceImpl = static_cast<DeviceImp *>(device);
        if (deviceImpl->numSubDevices > 0 && exposeSubDevices) {

            if (device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->isExposeSingleDeviceMode()) {
                this->devicesToExpose.push_back(device);
                continue;
            }

            for (auto subdevice : deviceImpl->subDevices) {
                this->devicesToExpose.push_back(subdevice);
            }
        } else {
            this->devicesToExpose.push_back(device);
        }
    }
}

ze_result_t DriverHandleImp::getDevice(uint32_t *pCount, ze_device_handle_t *phDevices) {
    uint32_t numDevices = static_cast<uint32_t>(this->devicesToExpose.size());
    if (*pCount == 0) {
        *pCount = numDevices;
        return ZE_RESULT_SUCCESS;
    }

    if (phDevices == nullptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    auto numDevicesToReturn = std::min(numDevices, *pCount);

    memcpy_s(phDevices, numDevicesToReturn * sizeof(ze_device_handle_t), this->devicesToExpose.data(), numDevicesToReturn * sizeof(ze_device_handle_t));

    *pCount = numDevicesToReturn;
    return ZE_RESULT_SUCCESS;
}

bool DriverHandleImp::findAllocationDataForRange(const void *buffer,
                                                 size_t size,
                                                 NEO::SvmAllocationData *&allocData) {
    uint64_t baseAddress = castToUint64(buffer);
    NEO::SvmAllocationData *beginAllocData = svmAllocsManager->getSVMAlloc(buffer);
    if (!beginAllocData) {
        allocData = nullptr;
        return false;
    }
    // for size <= 1 we do not need to do bounds checking as pointer is confirmed to be valid
    if (size <= 1 || (baseAddress + size <= beginAllocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress() + beginAllocData->size)) {
        allocData = beginAllocData;
        return true;
    }

    // we have base allocation, but it doesn't cover whole size, it means we deal with virtual memory pointers, check if we have whole range covered

    bool allocationRangeCovered = false;
    // If memory accessed is part of a virtual reservation, then return the first allocation data within the range.
    auto allocDataVec = findAllocationsWithinRange(buffer, size, &allocationRangeCovered);
    for (const auto &mappedAllocationData : allocDataVec) {
        if (mappedAllocationData->virtualReservationData) {
            allocData = mappedAllocationData;
            return true;
        }
    }
    return false;
}

std::vector<NEO::SvmAllocationData *> DriverHandleImp::findAllocationsWithinRange(const void *buffer,
                                                                                  size_t size,
                                                                                  bool *allocationRangeCovered) {
    std::vector<NEO::SvmAllocationData *> allocDataArray;
    const char *baseAddress = reinterpret_cast<const char *>(buffer);
    // Check if the host buffer overlaps any existing allocation
    NEO::SvmAllocationData *beginAllocData = svmAllocsManager->getSVMAlloc(buffer);
    NEO::SvmAllocationData *endAllocData = svmAllocsManager->getSVMAlloc(static_cast<const void *>(baseAddress + size - 1));

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

        if (NEO::debugManager.flags.ForceHostPointerImport.get() == 1) {
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
    bool allocFound = findAllocationDataForRange(ptr, size, allocData);
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

void *DriverHandleImp::importFdHandle(NEO::Device *neoDevice,
                                      ze_ipc_memory_flags_t flags,
                                      uint64_t handle,
                                      NEO::AllocationType allocationType,
                                      void *basePointer,
                                      NEO::GraphicsAllocation **pAlloc,
                                      NEO::SvmAllocationData &mappedPeerAllocData) {
    NEO::MemoryManager::OsHandleData osHandleData{handle};
    NEO::AllocationProperties unifiedMemoryProperties{neoDevice->getRootDeviceIndex(),
                                                      MemoryConstants::pageSize,
                                                      allocationType,
                                                      neoDevice->getDeviceBitfield()};
    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();
    bool isHostIpcAllocation = (allocationType == NEO::AllocationType::bufferHostMemory) ? true : false;
    NEO::GraphicsAllocation *alloc =
        this->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData,
                                                                           unifiedMemoryProperties,
                                                                           false,
                                                                           isHostIpcAllocation,
                                                                           false,
                                                                           basePointer);
    if (alloc == nullptr) {
        return nullptr;
    }

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    NEO::SvmAllocationData *allocDataTmp = nullptr;
    if (basePointer) {
        allocDataTmp = &mappedPeerAllocData;
        allocDataTmp->mappedAllocData = true;
    } else {
        allocDataTmp = &allocData;
        allocDataTmp->mappedAllocData = false;
    }
    allocDataTmp->gpuAllocations.addAllocation(alloc);
    allocDataTmp->cpuAllocation = nullptr;
    allocDataTmp->size = alloc->getUnderlyingBufferSize();
    allocDataTmp->memoryType =
        isHostIpcAllocation ? InternalMemoryType::hostUnifiedMemory : InternalMemoryType::deviceUnifiedMemory;
    allocDataTmp->device = neoDevice;
    allocDataTmp->isImportedAllocation = true;
    alloc->setIsImported();
    allocDataTmp->setAllocId(++this->getSvmAllocsManager()->allocationsCounter);
    if (flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        allocDataTmp->allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    if (flags & ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED) {
        allocDataTmp->allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    if (!basePointer) {
        this->getSvmAllocsManager()->insertSVMAlloc(allocData);
    }
    if (pAlloc) {
        *pAlloc = alloc;
    }

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

void *DriverHandleImp::importFdHandles(NEO::Device *neoDevice, ze_ipc_memory_flags_t flags, const std::vector<NEO::osHandle> &handles, void *basePtr, NEO::GraphicsAllocation **pAlloc, NEO::SvmAllocationData &mappedPeerAllocData) {
    NEO::AllocationProperties unifiedMemoryProperties{neoDevice->getRootDeviceIndex(),
                                                      MemoryConstants::pageSize,
                                                      NEO::AllocationType::buffer,
                                                      neoDevice->getDeviceBitfield()};
    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();

    NEO::GraphicsAllocation *alloc =
        this->getMemoryManager()->createGraphicsAllocationFromMultipleSharedHandles(handles,
                                                                                    unifiedMemoryProperties,
                                                                                    false,
                                                                                    false,
                                                                                    false,
                                                                                    basePtr);
    if (alloc == nullptr) {
        return nullptr;
    }

    NEO::SvmAllocationData *allocDataTmp = nullptr;
    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());

    if (basePtr) {
        allocDataTmp = &mappedPeerAllocData;
        allocDataTmp->mappedAllocData = true;
    } else {
        allocDataTmp = &allocData;
        allocDataTmp->mappedAllocData = false;
    }

    allocDataTmp->gpuAllocations.addAllocation(alloc);
    allocDataTmp->cpuAllocation = nullptr;
    allocDataTmp->size = alloc->getUnderlyingBufferSize();
    allocDataTmp->memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocDataTmp->device = neoDevice;
    allocDataTmp->isImportedAllocation = true;
    alloc->setIsImported();

    allocDataTmp->setAllocId(++this->getSvmAllocsManager()->allocationsCounter);

    if (flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        allocDataTmp->allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    if (flags & ZE_IPC_MEMORY_FLAG_BIAS_UNCACHED) {
        allocDataTmp->allocationFlagsProperty.flags.locallyUncachedResource = 1;
    }

    if (!basePtr) {
        this->getSvmAllocsManager()->insertSVMAlloc(allocData);
    }

    if (pAlloc) {
        *pAlloc = alloc;
    }

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

bool DriverHandleImp::isRemoteImageNeeded(Image *image, Device *device) {
    return (image->getAllocation()->getRootDeviceIndex() != device->getRootDeviceIndex());
}

ze_result_t DriverHandleImp::getPeerImage(Device *device, Image *image, Image **peerImage) {
    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    auto imageAllocPtr = reinterpret_cast<const void *>(image->getAllocation()->getGpuAddress());

    std::unique_lock<NEO::SpinLock> lock(deviceImp->peerImageAllocationsMutex);

    if (deviceImp->peerImageAllocations.find(imageAllocPtr) != deviceImp->peerImageAllocations.end()) {
        *peerImage = deviceImp->peerImageAllocations[imageAllocPtr];
    } else {
        uint64_t handle = 0;

        int ret = image->getAllocation()->peekInternalHandle(this->getMemoryManager(), handle);
        if (ret < 0) {
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }

        ze_image_desc_t desc = image->getImageDesc();
        ze_external_memory_import_fd_t externalMemoryImportDesc = {};

        externalMemoryImportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
        externalMemoryImportDesc.fd = static_cast<int>(handle);
        externalMemoryImportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
        externalMemoryImportDesc.pNext = nullptr;
        desc.pNext = &externalMemoryImportDesc;

        auto productFamily = device->getNEODevice()->getHardwareInfo().platform.eProductFamily;
        ze_result_t result = Image::create(productFamily, device, &desc, peerImage);

        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        deviceImp->peerImageAllocations.insert(std::make_pair(imageAllocPtr, *peerImage));
    }

    return ZE_RESULT_SUCCESS;
}

NEO::GraphicsAllocation *DriverHandleImp::getPeerAllocation(Device *device,
                                                            NEO::SvmAllocationData *allocData,
                                                            void *basePtr,
                                                            uintptr_t *peerGpuAddress,
                                                            NEO::SvmAllocationData **peerAllocData) {
    return getPeerAllocation(device, static_cast<DeviceImp *>(device)->peerAllocations, allocData, basePtr, peerGpuAddress, peerAllocData);
}

NEO::GraphicsAllocation *DriverHandleImp::getCounterPeerAllocation(Device *device, NEO::GraphicsAllocation &graphicsAllocation) {
    NEO::SvmAllocationData allocData(graphicsAllocation.getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(&graphicsAllocation);

    return getPeerAllocation(device, static_cast<DeviceImp *>(device)->peerCounterAllocations, &allocData, reinterpret_cast<void *>(graphicsAllocation.getGpuAddress()), nullptr, nullptr);
}

NEO::GraphicsAllocation *DriverHandleImp::getPeerAllocation(Device *device,
                                                            NEO::SVMAllocsManager::MapBasedAllocationTracker &storage,
                                                            NEO::SvmAllocationData *allocData,
                                                            void *basePtr,
                                                            uintptr_t *peerGpuAddress,
                                                            NEO::SvmAllocationData **peerAllocData) {
    NEO::GraphicsAllocation *alloc = nullptr;
    void *peerMapAddress = basePtr;
    void *peerPtr = nullptr;

    NEO::SvmAllocationData *peerAllocDataInternal = nullptr;

    std::unique_lock<NEO::SpinLock> lock(storage.mutex);

    auto iter = storage.allocations.find(basePtr);
    if (iter != storage.allocations.end()) {
        peerAllocDataInternal = &iter->second;
        alloc = peerAllocDataInternal->gpuAllocations.getDefaultGraphicsAllocation();
        UNRECOVERABLE_IF(alloc == nullptr);
        peerPtr = reinterpret_cast<void *>(alloc->getGpuAddress());
    } else {
        alloc = allocData->gpuAllocations.getDefaultGraphicsAllocation();
        UNRECOVERABLE_IF(alloc == nullptr);
        ze_ipc_memory_flags_t flags = {};
        uint32_t numHandles = alloc->getNumHandles();

        // Don't attempt to use the peerMapAddress for reserved memory due to the limitations in the address reserved.
        if (allocData->memoryType == InternalMemoryType::reservedDeviceMemory) {
            peerMapAddress = nullptr;
        }

        uint32_t peerAllocRootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();
        if (numHandles > 1) {
            peerAllocRootDeviceIndex = device->getNEODevice()->getRootDevice()->getRootDeviceIndex();
        }
        NEO::SvmAllocationData allocDataInternal(peerAllocRootDeviceIndex);

        if (numHandles > 1) {
            UNRECOVERABLE_IF(numHandles == 0);
            std::vector<NEO::osHandle> handles;
            for (uint32_t i = 0; i < numHandles; i++) {
                uint64_t handle = 0;
                int ret = alloc->peekInternalHandle(this->getMemoryManager(), i, handle);
                if (ret < 0) {
                    return nullptr;
                }
                handles.push_back(static_cast<NEO::osHandle>(handle));
            }
            auto neoDevice = device->getNEODevice()->getRootDevice();
            peerPtr = this->importFdHandles(neoDevice, flags, handles, peerMapAddress, &alloc, allocDataInternal);
        } else {
            uint64_t handle = 0;
            int ret = alloc->peekInternalHandle(this->getMemoryManager(), handle);
            if (ret < 0) {
                return nullptr;
            }
            peerPtr = this->importFdHandle(device->getNEODevice(),
                                           flags,
                                           handle,
                                           NEO::AllocationType::buffer,
                                           peerMapAddress,
                                           &alloc,
                                           allocDataInternal);
        }

        if (peerPtr == nullptr) {
            return nullptr;
        }

        peerAllocDataInternal = &allocDataInternal;
        if (peerMapAddress == nullptr) {
            peerAllocDataInternal = this->getSvmAllocsManager()->getSVMAlloc(peerPtr);
        }
        storage.allocations.insert(std::make_pair(basePtr, *peerAllocDataInternal));
        // Point to the new peer Alloc Data after it is recreated in the peer allocations map
        if (peerMapAddress) {
            peerAllocDataInternal = &storage.allocations.at(basePtr);
        }
    }

    if (peerAllocData) {
        *peerAllocData = peerAllocDataInternal;
    }

    if (peerGpuAddress) {
        *peerGpuAddress = reinterpret_cast<uintptr_t>(peerPtr);
    }

    return alloc;
}

void *DriverHandleImp::importNTHandle(ze_device_handle_t hDevice, void *handle, NEO::AllocationType allocationType, uint32_t parentProcessId) {
    auto neoDevice = Device::fromHandle(hDevice)->getNEODevice();

    bool isHostIpcAllocation = (allocationType == NEO::AllocationType::bufferHostMemory) ? true : false;

    NEO::MemoryManager::OsHandleData osHandleData{handle};
    osHandleData.parentProcessId = parentProcessId;
    NEO::AllocationProperties properties{neoDevice->getRootDeviceIndex(),
                                         MemoryConstants::pageSize,
                                         allocationType,
                                         neoDevice->getDeviceBitfield()};
    auto alloc = this->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);

    if (alloc == nullptr) {
        return nullptr;
    }

    NEO::SvmAllocationData allocData(neoDevice->getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(alloc);
    allocData.cpuAllocation = nullptr;
    allocData.size = alloc->getUnderlyingBufferSize();
    allocData.memoryType =
        isHostIpcAllocation ? InternalMemoryType::hostUnifiedMemory : InternalMemoryType::deviceUnifiedMemory;
    allocData.device = neoDevice;
    allocData.isImportedAllocation = true;
    alloc->setIsImported();

    allocData.setAllocId(++this->getSvmAllocsManager()->allocationsCounter);

    this->getSvmAllocsManager()->insertSVMAlloc(allocData);

    return reinterpret_cast<void *>(alloc->getGpuAddress());
}

void DriverHandleImp::initializeVertexes() {
    for (auto &device : this->devices) {
        auto deviceImpl = static_cast<DeviceImp *>(device);
        auto fabricVertex = FabricVertex::createFromDevice(device);
        if (fabricVertex == nullptr) {
            continue;
        }
        deviceImpl->setFabricVertex(fabricVertex);
        this->fabricVertices.push_back(fabricVertex);
    }

    FabricEdge::createEdgesFromVertices(this->fabricVertices, this->fabricEdges, this->fabricIndirectEdges);
}

ze_result_t DriverHandleImp::fabricVertexGetExp(uint32_t *pCount, ze_fabric_vertex_handle_t *phVertices) {

    if (fabricVertices.empty()) {
        this->initializeVertexes();
    }

    bool exposeSubDevices = this->devices[0]->getNEODevice()->getExecutionEnvironment()->getDeviceHierarchyMode() != NEO::DeviceHierarchyMode::composite;

    if (*pCount == 0) {
        if (exposeSubDevices) {
            for (auto &vertex : this->fabricVertices) {
                *pCount += std::max(static_cast<uint32_t>(vertex->subVertices.size()), 1u);
            }
        } else {
            *pCount = static_cast<uint32_t>(this->fabricVertices.size());
        }
        return ZE_RESULT_SUCCESS;
    }

    uint32_t i = 0;
    for (auto vertex : this->fabricVertices) {
        if (vertex->subVertices.size() > 0 && exposeSubDevices) {
            for (auto subVertex : vertex->subVertices) {
                phVertices[i++] = subVertex->toHandle();
                if (i == *pCount) {
                    return ZE_RESULT_SUCCESS;
                }
            }
        } else {
            phVertices[i++] = vertex->toHandle();
            if (i == *pCount) {
                return ZE_RESULT_SUCCESS;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::fabricEdgeGetExp(ze_fabric_vertex_handle_t hVertexA, ze_fabric_vertex_handle_t hVertexB,
                                              uint32_t *pCount, ze_fabric_edge_handle_t *phEdges) {

    FabricVertex *queryVertexA = FabricVertex::fromHandle(hVertexA);
    FabricVertex *queryVertexB = FabricVertex::fromHandle(hVertexB);
    uint32_t maxEdges = 0, edgeUpdateIndex = 0;
    bool updateEdges = false;

    if (*pCount == 0) {
        maxEdges = static_cast<uint32_t>(fabricEdges.size() + fabricIndirectEdges.size());
    } else {
        maxEdges = std::min<uint32_t>(*pCount, static_cast<uint32_t>(fabricEdges.size() + fabricIndirectEdges.size()));
    }

    if (phEdges != nullptr) {
        updateEdges = true;
    }

    for (const auto &edge : fabricEdges) {
        if (edgeUpdateIndex >= maxEdges) {
            break;
        }
        // Direct physical fabric connections are bi-directional
        if ((edge->vertexA == queryVertexA && edge->vertexB == queryVertexB) ||
            (edge->vertexA == queryVertexB && edge->vertexB == queryVertexA)) {

            if (updateEdges == true) {
                phEdges[edgeUpdateIndex] = edge->toHandle();
            }
            ++edgeUpdateIndex;
        }
    }

    for (const auto &edge : fabricIndirectEdges) {
        if (edgeUpdateIndex >= maxEdges) {
            break;
        }
        // Logical multi-hop edges might not be symmetric
        if (edge->vertexA == queryVertexA && edge->vertexB == queryVertexB) {
            if (updateEdges == true) {
                phEdges[edgeUpdateIndex] = edge->toHandle();
            }
            ++edgeUpdateIndex;
        }
    }

    *pCount = edgeUpdateIndex;
    return ZE_RESULT_SUCCESS;
}

uint32_t DriverHandleImp::getEventMaxPacketCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const {
    uint32_t maxCount = 0;

    if (numDevices == 0) {
        for (auto device : this->devices) {
            auto deviceMaxCount = device->getEventMaxPacketCount();
            maxCount = std::max(maxCount, deviceMaxCount);
        }
    } else {
        for (uint32_t i = 0; i < numDevices; i++) {
            auto deviceMaxCount = Device::fromHandle(deviceHandles[i])->getEventMaxPacketCount();
            maxCount = std::max(maxCount, deviceMaxCount);
        }
    }

    return maxCount;
}

uint32_t DriverHandleImp::getEventMaxKernelCount(uint32_t numDevices, ze_device_handle_t *deviceHandles) const {
    uint32_t maxCount = 0;

    if (numDevices == 0) {
        for (auto device : this->devices) {
            auto deviceMaxCount = device->getEventMaxKernelCount();
            maxCount = std::max(maxCount, deviceMaxCount);
        }
    } else {
        for (uint32_t i = 0; i < numDevices; i++) {
            auto deviceMaxCount = Device::fromHandle(deviceHandles[i])->getEventMaxKernelCount();
            maxCount = std::max(maxCount, deviceMaxCount);
        }
    }

    return maxCount;
}

int DriverHandleImp::setErrorDescription(const std::string &str) {
    return this->devices[0]->getNEODevice()->getExecutionEnvironment()->setErrorDescription(str);
}

ze_result_t DriverHandleImp::getErrorDescription(const char **ppString) {
    this->devices[0]->getNEODevice()->getExecutionEnvironment()->getErrorDescription(ppString);
    return ZE_RESULT_SUCCESS;
}

ze_result_t DriverHandleImp::clearErrorDescription() {
    return static_cast<ze_result_t>(this->devices[0]->getNEODevice()->getExecutionEnvironment()->clearErrorDescription());
}

} // namespace L0
