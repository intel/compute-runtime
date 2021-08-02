/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device_info.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/source/utilities/debug_settings_reader_creator.h"

#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/os_interface/ocl_reg_path.h"
#include "opencl/source/program/program.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/printf_handler/printf_handler.h"
#include "level_zero/core/source/sampler/sampler.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include "hw_helpers.h"

namespace NEO {
bool releaseFP64Override();
} // namespace NEO

namespace L0 {

uint32_t DeviceImp::getRootDeviceIndex() {
    return neoDevice->getRootDeviceIndex();
}

DriverHandle *DeviceImp::getDriverHandle() {
    return this->driverHandle;
}

void DeviceImp::setDriverHandle(DriverHandle *driverHandle) {
    this->driverHandle = driverHandle;
}

ze_result_t DeviceImp::canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value) {
    *value = false;

    DeviceImp *pPeerDevice = static_cast<DeviceImp *>(Device::fromHandle(hPeerDevice));
    if (this->getNEODevice()->getRootDeviceIndex() == pPeerDevice->getNEODevice()->getRootDeviceIndex()) {
        *value = true;
    }

    if (NEO::DebugManager.flags.EnableCrossDeviceAccess.get() == 1) {
        *value = true;
    }

    if (NEO::DebugManager.flags.EnableCrossDeviceAccess.get() == 0) {
        *value = false;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createCommandList(const ze_command_list_desc_t *desc,
                                         ze_command_list_handle_t *commandList) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    uint32_t engineGroupIndex = desc->commandQueueGroupOrdinal;
    mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    *commandList = CommandList::create(productFamily, this, static_cast<NEO::EngineGroupType>(engineGroupIndex), desc->flags, returnValue);

    return returnValue;
}

ze_result_t DeviceImp::createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                                  ze_command_list_handle_t *phCommandList) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    uint32_t engineGroupIndex = desc->ordinal;
    mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    *phCommandList = CommandList::createImmediate(productFamily, this, desc, false, static_cast<NEO::EngineGroupType>(engineGroupIndex), returnValue);

    return returnValue;
}

ze_result_t DeviceImp::createCommandQueue(const ze_command_queue_desc_t *desc,
                                          ze_command_queue_handle_t *commandQueue) {
    auto &platform = neoDevice->getHardwareInfo().platform;

    NEO::CommandStreamReceiver *csr = nullptr;
    uint32_t engineGroupIndex = desc->ordinal;
    mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
    if (desc->priority == ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW) {
        getCsrForLowPriority(&csr);
    } else {
        auto ret = getCsrForOrdinalAndIndex(&csr, desc->ordinal, desc->index);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
    }

    UNRECOVERABLE_IF(csr == nullptr);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    auto &hwHelper = NEO::HwHelper::get(platform.eRenderCoreFamily);
    bool isCopyOnly = hwHelper.isCopyOnlyEngineType(static_cast<NEO::EngineGroupType>(engineGroupIndex));

    *commandQueue = CommandQueue::create(platform.eProductFamily, this, csr, desc, isCopyOnly, false, returnValue);

    return returnValue;
}

ze_result_t DeviceImp::getCommandQueueGroupProperties(uint32_t *pCount,
                                                      ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    NEO::Device *activeDevice = getActiveDevice();
    auto &engineGroups = activeDevice->getEngineGroups();
    auto numEngineGroups = static_cast<uint32_t>(std::count_if(std::begin(engineGroups), std::end(engineGroups), [](const auto &engines) {
        return !engines.empty();
    }));

    if (*pCount == 0) {
        *pCount = numEngineGroups;
        return ZE_RESULT_SUCCESS;
    }

    *pCount = std::min(numEngineGroups, *pCount);
    for (uint32_t i = 0, engineGroupCount = 0;
         i < static_cast<uint32_t>(NEO::EngineGroupType::MaxEngineGroups) && engineGroupCount < *pCount;
         i++) {

        if (engineGroups[i].empty()) {
            continue;
        }
        const auto &hardwareInfo = this->neoDevice->getHardwareInfo();
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::RenderCompute)) {
            pCommandQueueGroupProperties[engineGroupCount].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE |
                                                                   ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY |
                                                                   ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS |
                                                                   ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS;
            pCommandQueueGroupProperties[engineGroupCount].maxMemoryFillPatternSize = std::numeric_limits<size_t>::max();
        }
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::Compute)) {
            pCommandQueueGroupProperties[engineGroupCount].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE |
                                                                   ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY |
                                                                   ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS;
            pCommandQueueGroupProperties[engineGroupCount].maxMemoryFillPatternSize = std::numeric_limits<size_t>::max();
        }
        if (i == static_cast<uint32_t>(NEO::EngineGroupType::Copy)) {
            auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
            pCommandQueueGroupProperties[engineGroupCount].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
            pCommandQueueGroupProperties[engineGroupCount].maxMemoryFillPatternSize = hwHelper.getMaxFillPaternSizeForCopyEngine();
        }
        auto &l0HwHelper = L0HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
        l0HwHelper.setAdditionalGroupProperty(pCommandQueueGroupProperties[engineGroupCount], i);
        pCommandQueueGroupProperties[engineGroupCount].numQueues = static_cast<uint32_t>(engineGroups[i].size());
        engineGroupCount++;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createImage(const ze_image_desc_t *desc, ze_image_handle_t *phImage) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    Image *pImage = nullptr;
    auto result = Image::create(productFamily, this, desc, &pImage);
    if (result == ZE_RESULT_SUCCESS) {
        *phImage = pImage->toHandle();
    }

    return result;
}

ze_result_t DeviceImp::createSampler(const ze_sampler_desc_t *desc,
                                     ze_sampler_handle_t *sampler) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    *sampler = Sampler::create(productFamily, this, desc);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                    ze_module_build_log_handle_t *buildLog, ModuleType type) {
    ModuleBuildLog *moduleBuildLog = nullptr;

    if (buildLog) {
        moduleBuildLog = ModuleBuildLog::create();
        *buildLog = moduleBuildLog->toHandle();
    }
    auto modulePtr = Module::create(this, desc, moduleBuildLog, type);
    if (modulePtr == nullptr) {
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }

    *module = modulePtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getComputeProperties(ze_device_compute_properties_t *pComputeProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();

    pComputeProperties->maxTotalGroupSize = static_cast<uint32_t>(deviceInfo.maxWorkGroupSize);

    pComputeProperties->maxGroupSizeX = static_cast<uint32_t>(deviceInfo.maxWorkItemSizes[0]);
    pComputeProperties->maxGroupSizeY = static_cast<uint32_t>(deviceInfo.maxWorkItemSizes[1]);
    pComputeProperties->maxGroupSizeZ = static_cast<uint32_t>(deviceInfo.maxWorkItemSizes[2]);

    pComputeProperties->maxGroupCountX = std::numeric_limits<uint32_t>::max();
    pComputeProperties->maxGroupCountY = std::numeric_limits<uint32_t>::max();
    pComputeProperties->maxGroupCountZ = std::numeric_limits<uint32_t>::max();

    pComputeProperties->maxSharedLocalMemory = static_cast<uint32_t>(deviceInfo.localMemSize);

    pComputeProperties->numSubGroupSizes = static_cast<uint32_t>(deviceInfo.maxSubGroups.size());

    for (uint32_t i = 0; i < pComputeProperties->numSubGroupSizes; ++i) {
        pComputeProperties->subGroupSizes[i] = static_cast<uint32_t>(deviceInfo.maxSubGroups[i]);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getP2PProperties(ze_device_handle_t hPeerDevice,
                                        ze_device_p2p_properties_t *pP2PProperties) {
    pP2PProperties->flags = 0;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getMemoryProperties(uint32_t *pCount, ze_device_memory_properties_t *pMemProperties) {
    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > 1) {
        *pCount = 1;
    }

    if (nullptr == pMemProperties) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const auto &deviceInfo = this->neoDevice->getDeviceInfo();

    strcpy_s(pMemProperties->name, ZE_MAX_DEVICE_NAME, getHwHelper().getDeviceMemoryName().c_str());
    auto &hwInfo = this->getHwInfo();
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    pMemProperties->maxClockRate = hwInfoConfig.getDeviceMemoryMaxClkRate(&hwInfo);
    pMemProperties->maxBusWidth = deviceInfo.addressBits;
    pMemProperties->totalSize = deviceInfo.globalMemSize;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) {
    auto &hwInfo = this->getHwInfo();
    auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    pMemAccessProperties->hostAllocCapabilities =
        static_cast<ze_memory_access_cap_flags_t>(hwInfoConfig.getHostMemCapabilities(&hwInfo));
    pMemAccessProperties->deviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC;
    pMemAccessProperties->sharedSingleDeviceAllocCapabilities =
        ZE_MEMORY_ACCESS_CAP_FLAG_RW | ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC;
    pMemAccessProperties->sharedCrossDeviceAllocCapabilities = 0;
    pMemAccessProperties->sharedSystemAllocCapabilities = 0;

    return ZE_RESULT_SUCCESS;
}

static constexpr ze_device_fp_flags_t defaultFpFlags = static_cast<ze_device_fp_flags_t>(ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST |
                                                                                         ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO |
                                                                                         ZE_DEVICE_FP_FLAG_ROUND_TO_INF |
                                                                                         ZE_DEVICE_FP_FLAG_INF_NAN |
                                                                                         ZE_DEVICE_FP_FLAG_DENORM |
                                                                                         ZE_DEVICE_FP_FLAG_FMA);

ze_result_t DeviceImp::getKernelProperties(ze_device_module_properties_t *pKernelProperties) {
    const auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();
    auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    std::string ilVersion = deviceInfo.ilVersion;
    size_t majorVersionPos = ilVersion.find('_');
    size_t minorVersionPos = ilVersion.find('.');

    if (majorVersionPos != std::string::npos && minorVersionPos != std::string::npos) {
        uint32_t majorSpirvVersion = static_cast<uint32_t>(std::stoul(ilVersion.substr(majorVersionPos + 1, minorVersionPos)));
        uint32_t minorSpirvVersion = static_cast<uint32_t>(std::stoul(ilVersion.substr(minorVersionPos + 1)));
        pKernelProperties->spirvVersionSupported = ZE_MAKE_VERSION(majorSpirvVersion, minorSpirvVersion);
    } else {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    pKernelProperties->flags = ZE_DEVICE_MODULE_FLAG_FP16;
    if (hardwareInfo.capabilityTable.ftrSupportsInteger64BitAtomics) {
        pKernelProperties->flags |= ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS;
    }
    pKernelProperties->fp16flags = defaultFpFlags;
    pKernelProperties->fp32flags = defaultFpFlags;

    if (NEO::DebugManager.flags.OverrideDefaultFP64Settings.get() == 1) {
        pKernelProperties->flags |= ZE_DEVICE_MODULE_FLAG_FP64;
        pKernelProperties->fp64flags = defaultFpFlags | ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT;
        pKernelProperties->fp32flags |= ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT;
    } else {
        pKernelProperties->fp64flags = 0;
        if (hardwareInfo.capabilityTable.ftrSupportsFP64) {
            pKernelProperties->flags |= ZE_DEVICE_MODULE_FLAG_FP64;
            pKernelProperties->fp64flags |= defaultFpFlags;
            if (hardwareInfo.capabilityTable.ftrSupports64BitMath) {
                pKernelProperties->fp64flags |= ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT;
                pKernelProperties->fp32flags |= ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT;
            }
        }
    }

    pKernelProperties->nativeKernelSupported.id[0] = 0;

    processAdditionalKernelProperties(hwHelper, pKernelProperties);

    pKernelProperties->maxArgumentsSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().maxParameterSize);

    pKernelProperties->printfBufferSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().printfBufferSize);

    if (pKernelProperties->pNext) {
        ze_base_desc_t *extendedProperties = reinterpret_cast<ze_base_desc_t *>(pKernelProperties->pNext);
        if (extendedProperties->stype == ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES) {
            ze_float_atomic_ext_properties_t *floatProperties =
                reinterpret_cast<ze_float_atomic_ext_properties_t *>(extendedProperties);
            auto &hwInfo = this->getHwInfo();
            auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
            hwInfoConfig.getKernelExtendedProperties(&floatProperties->fp16Flags,
                                                     &floatProperties->fp32Flags,
                                                     &floatProperties->fp64Flags);
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getProperties(ze_device_properties_t *pDeviceProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();
    const auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    pDeviceProperties->type = ZE_DEVICE_TYPE_GPU;

    pDeviceProperties->vendorId = deviceInfo.vendorId;

    pDeviceProperties->deviceId = hardwareInfo.platform.usDeviceID;

    pDeviceProperties->flags = 0u;

    uint32_t rootDeviceIndex = this->neoDevice->getRootDeviceIndex();

    memset(pDeviceProperties->uuid.id, 0, ZE_MAX_DEVICE_UUID_SIZE);
    memcpy_s(pDeviceProperties->uuid.id, sizeof(uint32_t), &pDeviceProperties->vendorId, sizeof(pDeviceProperties->vendorId));
    memcpy_s(pDeviceProperties->uuid.id + sizeof(uint32_t), sizeof(uint32_t), &pDeviceProperties->deviceId, sizeof(pDeviceProperties->deviceId));
    memcpy_s(pDeviceProperties->uuid.id + (2 * sizeof(uint32_t)), sizeof(uint32_t), &rootDeviceIndex, sizeof(rootDeviceIndex));

    pDeviceProperties->subdeviceId = isSubdevice ? static_cast<NEO::SubDevice *>(neoDevice)->getSubDeviceIndex() : 0;

    pDeviceProperties->coreClockRate = deviceInfo.maxClockFrequency;

    pDeviceProperties->maxMemAllocSize = this->neoDevice->getDeviceInfo().maxMemAllocSize;

    pDeviceProperties->maxCommandQueuePriority = 0;

    pDeviceProperties->maxHardwareContexts = 1024 * 64;

    pDeviceProperties->numThreadsPerEU = deviceInfo.numThreadsPerEU;

    pDeviceProperties->physicalEUSimdWidth = hwHelper.getMinimalSIMDSize();

    pDeviceProperties->numEUsPerSubslice = hardwareInfo.gtSystemInfo.MaxEuPerSubSlice;

    if (NEO::DebugManager.flags.DebugApiUsed.get() == 1) {
        pDeviceProperties->numSubslicesPerSlice = hardwareInfo.gtSystemInfo.MaxSubSlicesSupported / hardwareInfo.gtSystemInfo.MaxSlicesSupported;
    } else {
        pDeviceProperties->numSubslicesPerSlice = hardwareInfo.gtSystemInfo.SubSliceCount / hardwareInfo.gtSystemInfo.SliceCount;
    }

    pDeviceProperties->numSlices = hardwareInfo.gtSystemInfo.SliceCount * ((this->numSubDevices > 0) ? this->numSubDevices : 1);

    if ((NEO::DebugManager.flags.UseCyclesPerSecondTimer.get() == 1) ||
        (pDeviceProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2)) {
        pDeviceProperties->timerResolution = this->neoDevice->getDeviceInfo().outProfilingTimerClock;
    } else {
        pDeviceProperties->timerResolution = this->neoDevice->getDeviceInfo().outProfilingTimerResolution;
    }

    pDeviceProperties->timestampValidBits = hardwareInfo.capabilityTable.timestampValidBits;

    pDeviceProperties->kernelTimestampValidBits = hardwareInfo.capabilityTable.kernelTimestampValidBits;

    if (hardwareInfo.capabilityTable.isIntegratedDevice) {
        pDeviceProperties->flags |= ZE_DEVICE_PROPERTY_FLAG_INTEGRATED;
    }

    if (isSubdevice) {
        pDeviceProperties->flags |= ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
    }

    if (this->neoDevice->getDeviceInfo().errorCorrectionSupport) {
        pDeviceProperties->flags |= ZE_DEVICE_PROPERTY_FLAG_ECC;
    }

    if (hardwareInfo.capabilityTable.supportsOnDemandPageFaults) {
        pDeviceProperties->flags |= ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING;
    }

    memset(pDeviceProperties->name, 0, ZE_MAX_DEVICE_NAME);

    std::string name = getNEODevice()->getDeviceInfo().name;
    memcpy_s(pDeviceProperties->name, name.length(), name.c_str(), name.length());

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getExternalMemoryProperties(ze_device_external_memory_properties_t *pExternalMemoryProperties) {
    pExternalMemoryProperties->imageExportTypes = 0u;
    pExternalMemoryProperties->imageImportTypes = 0u;
    pExternalMemoryProperties->memoryAllocationExportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    pExternalMemoryProperties->memoryAllocationImportTypes = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getGlobalTimestamps(uint64_t *hostTimestamp, uint64_t *deviceTimestamp) {
    NEO::TimeStampData queueTimeStamp;
    bool retVal = this->neoDevice->getOSTime()->getCpuGpuTime(&queueTimeStamp);
    if (!retVal)
        return ZE_RESULT_ERROR_DEVICE_LOST;

    *deviceTimestamp = queueTimeStamp.GPUTimeStamp;

    retVal = this->neoDevice->getOSTime()->getCpuTime(hostTimestamp);
    if (!retVal)
        return ZE_RESULT_ERROR_DEVICE_LOST;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices) {
    if (*pCount == 0) {
        *pCount = this->numSubDevices;
        return ZE_RESULT_SUCCESS;
    }

    if (phSubdevices == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (*pCount > this->numSubDevices) {
        *pCount = this->numSubDevices;
    }

    for (uint32_t i = 0; i < *pCount; i++) {
        phSubdevices[i] = this->subDevices[i];
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getCacheProperties(uint32_t *pCount, ze_device_cache_properties_t *pCacheProperties) {
    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > 1) {
        *pCount = 1;
    }

    const auto &hardwareInfo = this->getHwInfo();
    pCacheProperties[0].cacheSize = hardwareInfo.gtSystemInfo.L3BankCount * 128 * KB;
    pCacheProperties[0].flags = 0;

    if (pCacheProperties->pNext) {
        auto extendedProperties = reinterpret_cast<ze_device_cache_properties_t *>(pCacheProperties->pNext);
        if (extendedProperties->stype == ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC) {
            auto cacheReservationProperties = reinterpret_cast<ze_cache_reservation_ext_desc_t *>(extendedProperties);
            cacheReservationProperties->maxCacheReservationSize = cacheReservation->getMaxCacheReservationSize();
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::reserveCache(size_t cacheLevel, size_t cacheReservationSize) {
    if (cacheReservation->getMaxCacheReservationSize() == 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (cacheLevel == 0) {
        cacheLevel = 3;
    }

    auto result = cacheReservation->reserveCache(cacheLevel, cacheReservationSize);
    if (result == false) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) {
    if (cacheReservation->getMaxCacheReservationSize() == 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (cacheRegion == ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_REGION_DEFAULT) {
        cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_ZE_CACHE_NON_RESERVED_REGION;
    }

    auto result = cacheReservation->setCacheAdvice(ptr, regionSize, cacheRegion);
    if (result == false) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::imageGetProperties(const ze_image_desc_t *desc,
                                          ze_image_properties_t *pImageProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();

    if (deviceInfo.imageSupport) {
        pImageProperties->samplerFilterFlags = ZE_IMAGE_SAMPLER_FILTER_FLAG_LINEAR;
    } else {
        pImageProperties->samplerFilterFlags = 0;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();

    if (deviceInfo.imageSupport) {
        pDeviceImageProperties->maxImageDims1D = static_cast<uint32_t>(deviceInfo.image2DMaxWidth);
        pDeviceImageProperties->maxImageDims2D = static_cast<uint32_t>(deviceInfo.image2DMaxHeight);
        pDeviceImageProperties->maxImageDims3D = static_cast<uint32_t>(deviceInfo.image3DMaxDepth);
        pDeviceImageProperties->maxImageBufferSize = deviceInfo.imageMaxBufferSize;
        pDeviceImageProperties->maxImageArraySlices = static_cast<uint32_t>(deviceInfo.imageMaxArraySize);
        pDeviceImageProperties->maxSamplers = deviceInfo.maxSamplers;
        pDeviceImageProperties->maxReadImageArgs = deviceInfo.maxReadImageArgs;
        pDeviceImageProperties->maxWriteImageArgs = deviceInfo.maxWriteImageArgs;
    } else {
        pDeviceImageProperties->maxImageDims1D = 0u;
        pDeviceImageProperties->maxImageDims2D = 0u;
        pDeviceImageProperties->maxImageDims3D = 0u;
        pDeviceImageProperties->maxImageBufferSize = 0u;
        pDeviceImageProperties->maxImageArraySlices = 0u;
        pDeviceImageProperties->maxSamplers = 0u;
        pDeviceImageProperties->maxReadImageArgs = 0u;
        pDeviceImageProperties->maxWriteImageArgs = 0u;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getDebugProperties(zet_device_debug_properties_t *pDebugProperties) {
    bool isDebugAttachAvailable = getOsInterface().isDebugAttachAvailable();
    if (isDebugAttachAvailable && !isSubdevice) {
        pDebugProperties->flags = zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH;
    } else {
        pDebugProperties->flags = 0;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::systemBarrier() { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }

ze_result_t DeviceImp::activateMetricGroups(uint32_t count,
                                            zet_metric_group_handle_t *phMetricGroups) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    if (this->isMultiDeviceCapable()) {
        for (auto subDevice : this->subDevices) {
            result = subDevice->getMetricContext().activateMetricGroupsDeferred(count, phMetricGroups);
            if (result != ZE_RESULT_SUCCESS)
                break;
        }
    } else {
        result = metricContext->activateMetricGroupsDeferred(count, phMetricGroups);
    }
    return result;
}

void *DeviceImp::getExecEnvironment() { return execEnvironment; }

BuiltinFunctionsLib *DeviceImp::getBuiltinFunctionsLib() { return builtins.get(); }

uint32_t DeviceImp::getMOCS(bool l3enabled, bool l1enabled) {
    return getHwHelper().getMocsIndex(*getNEODevice()->getGmmHelper(), l3enabled, l1enabled) << 1;
}

NEO::HwHelper &DeviceImp::getHwHelper() {
    const auto &hardwareInfo = neoDevice->getHardwareInfo();
    return NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
}

NEO::OSInterface &DeviceImp::getOsInterface() { return *neoDevice->getRootDeviceEnvironment().osInterface; }

uint32_t DeviceImp::getPlatformInfo() const {
    const auto &hardwareInfo = neoDevice->getHardwareInfo();
    return hardwareInfo.platform.eRenderCoreFamily;
}

MetricContext &DeviceImp::getMetricContext() { return *metricContext; }

void DeviceImp::activateMetricGroups() {
    if (metricContext != nullptr) {
        metricContext->activateMetricGroups();
    }
}
uint32_t DeviceImp::getMaxNumHwThreads() const { return maxNumHwThreads; }

const NEO::HardwareInfo &DeviceImp::getHwInfo() const { return neoDevice->getHardwareInfo(); }

Device *Device::create(DriverHandle *driverHandle, NEO::Device *neoDevice, uint32_t currentDeviceMask, bool isSubDevice, ze_result_t *returnValue) {
    auto device = new DeviceImp;
    UNRECOVERABLE_IF(device == nullptr);

    device->setDriverHandle(driverHandle);
    neoDevice->setSpecializedDevice(device);

    device->neoDevice = neoDevice;
    neoDevice->incRefInternal();

    device->execEnvironment = (void *)neoDevice->getExecutionEnvironment();
    device->metricContext = MetricContext::create(*device);
    device->builtins = BuiltinFunctionsLib::create(
        device, neoDevice->getBuiltIns());
    device->cacheReservation = CacheReservation::create(*device);
    device->maxNumHwThreads = NEO::HwHelper::getMaxThreadsForVfe(neoDevice->getHardwareInfo());

    const bool allocateDebugSurface = (device->getL0Debugger() || neoDevice->getDeviceInfo().debuggerActive) && !isSubDevice;
    NEO::GraphicsAllocation *debugSurface = nullptr;

    if (allocateDebugSurface) {
        debugSurface = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
            {device->getRootDeviceIndex(), true,
             NEO::SipKernel::maxDbgSurfaceSize,
             NEO::GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA,
             false,
             false,
             device->getNEODevice()->getDeviceBitfield()});
        device->setDebugSurface(debugSurface);
    }

    if (device->neoDevice->getNumAvailableDevices() == 1) {
        device->numSubDevices = 0;
    } else {
        for (uint32_t i = 0; i < device->neoDevice->getNumAvailableDevices(); i++) {

            if (!((1UL << i) & currentDeviceMask)) {
                continue;
            }

            ze_device_handle_t subDevice = Device::create(driverHandle,
                                                          device->neoDevice->getDeviceById(i),
                                                          0,
                                                          true, returnValue);
            if (subDevice == nullptr) {
                return nullptr;
            }
            static_cast<DeviceImp *>(subDevice)->isSubdevice = true;
            static_cast<DeviceImp *>(subDevice)->setDebugSurface(debugSurface);
            device->subDevices.push_back(static_cast<Device *>(subDevice));
        }
        device->numSubDevices = static_cast<uint32_t>(device->subDevices.size());
    }

    if (neoDevice->getCompilerInterface()) {
        auto &hwInfo = neoDevice->getHardwareInfo();
        if (neoDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread || neoDevice->getDebugger()) {
            bool ret = NEO::SipKernel::initSipKernel(NEO::SipKernel::getSipKernelType(*neoDevice), *neoDevice);
            UNRECOVERABLE_IF(!ret);

            auto &stateSaveAreaHeader = NEO::SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();
            if (debugSurface && stateSaveAreaHeader.size() > 0) {
                auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
                NEO::MemoryTransferHelper::transferMemoryToAllocation(hwHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *debugSurface),
                                                                      *neoDevice, debugSurface, 0, stateSaveAreaHeader.data(),
                                                                      stateSaveAreaHeader.size());
            }
        }
    } else {
        *returnValue = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    auto supportDualStorageSharedMemory = neoDevice->getMemoryManager()->isLocalMemorySupported(device->neoDevice->getRootDeviceIndex());
    if (NEO::DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get() != -1) {
        supportDualStorageSharedMemory = NEO::DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get();
    }

    if (supportDualStorageSharedMemory) {
        ze_command_queue_desc_t cmdQueueDesc = {};
        cmdQueueDesc.ordinal = 0;
        cmdQueueDesc.index = 0;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        ze_result_t resultValue = ZE_RESULT_SUCCESS;
        device->pageFaultCommandList =
            CommandList::createImmediate(
                device->neoDevice->getHardwareInfo().platform.eProductFamily, device, &cmdQueueDesc, true, NEO::EngineGroupType::RenderCompute, resultValue);
    }

    if (device->getSourceLevelDebugger()) {
        auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();
        device->getSourceLevelDebugger()
            ->notifyNewDevice(osInterface ? osInterface->getDriverModel()->getDeviceHandle() : 0);
    }

    if (static_cast<DriverHandleImp *>(driverHandle)->enableSysman && !isSubDevice) {
        device->setSysmanHandle(L0::SysmanDeviceHandleContext::init(device->toHandle()));
    }

    return device;
}

void DeviceImp::releaseResources() {
    if (resourcesReleased) {
        return;
    }
    if (neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.get() &&
        !neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger->isLegacy()) {
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.reset(nullptr);
    }
    for (uint32_t i = 0; i < this->numSubDevices; i++) {
        delete this->subDevices[i];
    }
    this->numSubDevices = 0;

    if (this->pageFaultCommandList) {
        this->pageFaultCommandList->destroy();
        this->pageFaultCommandList = nullptr;
    }
    metricContext.reset();
    builtins.reset();
    cacheReservation.reset();

    if (getSourceLevelDebugger()) {
        getSourceLevelDebugger()->notifyDeviceDestruction();
    }

    if (!isSubdevice) {
        if (this->debugSurface) {
            this->neoDevice->getMemoryManager()->freeGraphicsMemory(this->debugSurface);
            this->debugSurface = nullptr;
        }
    }

    if (neoDevice) {
        neoDevice->decRefInternal();
        neoDevice = nullptr;
    }

    resourcesReleased = true;
}

DeviceImp::~DeviceImp() {
    releaseResources();

    if (!isSubdevice) {
        if (pSysmanDevice != nullptr) {
            delete pSysmanDevice;
            pSysmanDevice = nullptr;
        }
    }
}

NEO::PreemptionMode DeviceImp::getDevicePreemptionMode() const {
    return neoDevice->getPreemptionMode();
}

const NEO::DeviceInfo &DeviceImp::getDeviceInfo() const {
    return neoDevice->getDeviceInfo();
}

NEO::Device *DeviceImp::getNEODevice() {
    return neoDevice;
}

NEO::GraphicsAllocation *DeviceImp::allocateManagedMemoryFromHostPtr(void *buffer, size_t size, struct CommandList *commandList) {
    char *baseAddress = reinterpret_cast<char *>(buffer);
    NEO::GraphicsAllocation *allocation = nullptr;
    bool allocFound = false;
    std::vector<NEO::SvmAllocationData *> allocDataArray = driverHandle->findAllocationsWithinRange(buffer, size, &allocFound);
    if (allocFound) {
        return allocDataArray[0]->gpuAllocations.getGraphicsAllocation(getRootDeviceIndex());
    }

    if (!allocDataArray.empty()) {
        UNRECOVERABLE_IF(commandList == nullptr);
        for (auto allocData : allocDataArray) {
            allocation = allocData->gpuAllocations.getGraphicsAllocation(getRootDeviceIndex());
            char *allocAddress = reinterpret_cast<char *>(allocation->getGpuAddress());
            size_t allocSize = allocData->size;

            driverHandle->getSvmAllocsManager()->removeSVMAlloc(*allocData);
            neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
            commandList->eraseDeallocationContainerEntry(allocation);
            commandList->eraseResidencyContainerEntry(allocation);

            if (allocAddress < baseAddress) {
                buffer = reinterpret_cast<void *>(allocAddress);
                baseAddress += size;
                size = ptrDiff(baseAddress, allocAddress);
                baseAddress = reinterpret_cast<char *>(buffer);
            } else {
                allocAddress += allocSize;
                baseAddress += size;
                if (allocAddress > baseAddress) {
                    baseAddress = reinterpret_cast<char *>(buffer);
                    size = ptrDiff(allocAddress, baseAddress);
                } else {
                    baseAddress = reinterpret_cast<char *>(buffer);
                }
            }
        }
    }

    allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {getRootDeviceIndex(), false, size, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, neoDevice->getDeviceBitfield()},
        buffer);

    if (allocation == nullptr) {
        return allocation;
    }

    NEO::SvmAllocationData allocData(getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(allocation);
    allocData.cpuAllocation = nullptr;
    allocData.size = size;
    allocData.memoryType = InternalMemoryType::NOT_SPECIFIED;
    allocData.device = nullptr;
    driverHandle->getSvmAllocsManager()->insertSVMAlloc(allocData);

    return allocation;
}

NEO::GraphicsAllocation *DeviceImp::allocateMemoryFromHostPtr(const void *buffer, size_t size) {
    NEO::AllocationProperties properties = {getRootDeviceIndex(), false, size,
                                            NEO::GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR,
                                            false, neoDevice->getDeviceBitfield()};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = true;
    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties,
                                                                                          buffer);
    if (allocation == nullptr) {
        allocation = neoDevice->getMemoryManager()->allocateInternalGraphicsMemoryWithHostCopy(neoDevice->getRootDeviceIndex(),
                                                                                               neoDevice->getDeviceBitfield(),
                                                                                               buffer,
                                                                                               size);
    }

    return allocation;
}

ze_result_t DeviceImp::getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index) {
    if (ordinal >= static_cast<uint32_t>(NEO::EngineGroupType::MaxEngineGroups)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    uint32_t engineGroupIndex = ordinal;
    auto ret = mapOrdinalForAvailableEngineGroup(&engineGroupIndex);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }
    NEO::Device *activeDevice = getActiveDevice();
    if (index >= activeDevice->getEngineGroups()[engineGroupIndex].size()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *csr = activeDevice->getEngineGroups()[engineGroupIndex][index].commandStreamReceiver;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getCsrForLowPriority(NEO::CommandStreamReceiver **csr) {
    NEO::Device *activeDevice = getActiveDevice();
    for (auto &it : activeDevice->getEngines()) {
        if (it.osContext->isLowPriority()) {
            *csr = it.commandStreamReceiver;
            return ZE_RESULT_SUCCESS;
        }
    }
    // if the code falls through, we have no low priority context created by neoDevice.
    UNRECOVERABLE_IF(true);
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t DeviceImp::mapOrdinalForAvailableEngineGroup(uint32_t *ordinal) {
    NEO::Device *activeDevice = getActiveDevice();
    const auto &engines = activeDevice->getEngineGroups();
    uint32_t numNonEmptyGroups = 0;
    uint32_t i = 0;
    for (; i < engines.size() && numNonEmptyGroups <= *ordinal; i++) {
        if (!engines[i].empty()) {
            numNonEmptyGroups++;
        }
    }
    if (*ordinal + 1 > numNonEmptyGroups) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    *ordinal = i - 1;
    return ZE_RESULT_SUCCESS;
};

DebugSession *DeviceImp::getDebugSession(const zet_debug_config_t &config) {
    return debugSession.get();
}

DebugSession *DeviceImp::createDebugSession(const zet_debug_config_t &config, ze_result_t &result) {
    if (!this->isSubdevice) {
        auto session = DebugSession::create(config, this, result);
        debugSession.reset(session);
    } else {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return debugSession.get();
}

bool DeviceImp::toPhysicalSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &deviceIndex) {
    auto hwInfo = neoDevice->getRootDeviceEnvironment().getHardwareInfo();
    uint32_t subDeviceCount = NEO::HwHelper::getSubDevicesCount(hwInfo);
    auto deviceBitfield = neoDevice->getDeviceBitfield();

    if (topologyMap.size() == subDeviceCount && !isSubdevice) {
        uint32_t sliceId = slice;
        for (uint32_t i = 0; i < topologyMap.size(); i++) {
            if (sliceId < topologyMap.at(i).sliceIndices.size()) {
                slice = topologyMap.at(i).sliceIndices[sliceId];
                deviceIndex = i;
                return true;
            }
            sliceId = sliceId - static_cast<uint32_t>(topologyMap.at(i).sliceIndices.size());
        }
    } else if (isSubdevice) {
        UNRECOVERABLE_IF(!deviceBitfield.any());
        uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));

        if (topologyMap.find(subDeviceIndex) != topologyMap.end()) {
            if (slice < topologyMap.at(subDeviceIndex).sliceIndices.size()) {
                deviceIndex = subDeviceIndex;
                slice = topologyMap.at(subDeviceIndex).sliceIndices[slice];
                return true;
            }
        }
    }

    return false;
}

bool DeviceImp::toApiSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t deviceIndex) {
    auto deviceBitfield = neoDevice->getDeviceBitfield();

    if (isSubdevice) {
        UNRECOVERABLE_IF(!deviceBitfield.any());
        deviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    }

    if (topologyMap.find(deviceIndex) != topologyMap.end()) {
        uint32_t apiSliceId = 0;
        if (!isSubdevice) {
            for (uint32_t devId = 0; devId < deviceIndex; devId++) {
                apiSliceId += static_cast<uint32_t>(topologyMap.at(devId).sliceIndices.size());
            }
        }

        for (uint32_t i = 0; i < topologyMap.at(deviceIndex).sliceIndices.size(); i++) {
            if (static_cast<uint32_t>(topologyMap.at(deviceIndex).sliceIndices[i]) == slice) {
                apiSliceId += i;
                slice = apiSliceId;
                return true;
            }
        }
    }

    return false;
}

} // namespace L0
