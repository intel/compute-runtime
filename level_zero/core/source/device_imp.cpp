/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device_imp.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "opencl/source/device/device_info.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/source/program/program.h"

#include "level_zero/core/source/builtin_functions_lib.h"
#include "level_zero/core/source/cmdlist.h"
#include "level_zero/core/source/cmdqueue.h"
#include "level_zero/core/source/driver_handle_imp.h"
#include "level_zero/core/source/event.h"
#include "level_zero/core/source/image.h"
#include "level_zero/core/source/memory_operations_helper.h"
#include "level_zero/core/source/module.h"
#include "level_zero/core/source/printf_handler.h"
#include "level_zero/core/source/sampler.h"
#include "level_zero/tools/source/metrics/metric.h"

#include "hw_helpers.h"

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
    if (NEO::DebugManager.flags.CreateMultipleRootDevices.get() > 0) {
        *value = true;
    }
    if (NEO::DebugManager.flags.CreateMultipleSubDevices.get() > 0) {
        *value = true;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::copyCommandList(ze_command_list_handle_t hCommandList,
                                       ze_command_list_handle_t *phCommandList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DeviceImp::createCommandList(const ze_command_list_desc_t *desc,
                                         ze_command_list_handle_t *commandList) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    *commandList = CommandList::create(productFamily, this);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                                  ze_command_list_handle_t *phCommandList) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    *phCommandList = CommandList::createImmediate(productFamily, this, desc, false);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createCommandQueue(const ze_command_queue_desc_t *desc,
                                          ze_command_queue_handle_t *commandQueue) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;

    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    *commandQueue = CommandQueue::create(productFamily, this, csr, desc);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createEventPool(const ze_event_pool_desc_t *desc,
                                       ze_event_pool_handle_t *eventPool) {
    *eventPool = EventPool::create(this, desc);
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createImage(const ze_image_desc_t *desc, ze_image_handle_t *phImage) {
    if (desc->format.layout >= ze_image_format_layout_t::ZE_IMAGE_FORMAT_LAYOUT_Y8) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    *phImage = Image::create(productFamily, this, desc);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createSampler(const ze_sampler_desc_t *desc,
                                     ze_sampler_handle_t *sampler) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    *sampler = Sampler::create(productFamily, this, desc);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                    ze_module_build_log_handle_t *buildLog) {
    ModuleBuildLog *moduleBuildLog = nullptr;

    if (buildLog) {
        moduleBuildLog = ModuleBuildLog::create();
        *buildLog = moduleBuildLog->toHandle();
    }
    auto modulePtr = Module::create(this, desc, neoDevice, moduleBuildLog);
    if (modulePtr == nullptr) {
        return ZE_RESULT_ERROR_MODULE_BUILD_FAILURE;
    }

    *module = modulePtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::evictImage(ze_image_handle_t hImage) {
    auto alloc = Image::fromHandle(hImage)->getAllocation();
    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->evict(*alloc);
    return changeMemoryOperationStatusToL0ResultType(success);
}

ze_result_t DeviceImp::evictMemory(void *ptr, size_t size) {
    auto alloc = getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr);
    if (alloc == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->evict(*alloc->gpuAllocation);
    return changeMemoryOperationStatusToL0ResultType(success);
}

ze_result_t DeviceImp::getComputeProperties(ze_device_compute_properties_t *pComputeProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();

    pComputeProperties->maxTotalGroupSize = static_cast<uint32_t>(deviceInfo.maxWorkGroupSize);

    pComputeProperties->maxGroupSizeX = static_cast<uint32_t>(deviceInfo.maxWorkItemSizes[0]);
    pComputeProperties->maxGroupSizeY = static_cast<uint32_t>(deviceInfo.maxWorkItemSizes[1]);
    pComputeProperties->maxGroupSizeZ = static_cast<uint32_t>(deviceInfo.maxWorkItemSizes[2]);

    pComputeProperties->maxGroupCountX = UINT32_MAX;
    pComputeProperties->maxGroupCountY = UINT32_MAX;
    pComputeProperties->maxGroupCountZ = UINT32_MAX;

    pComputeProperties->maxSharedLocalMemory = static_cast<uint32_t>(deviceInfo.localMemSize);

    pComputeProperties->numSubGroupSizes = static_cast<uint32_t>(deviceInfo.maxSubGroups.size());

    for (uint32_t i = 0; i < pComputeProperties->numSubGroupSizes; ++i) {
        pComputeProperties->subGroupSizes[i] = static_cast<uint32_t>(deviceInfo.maxSubGroups[i]);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getP2PProperties(ze_device_handle_t hPeerDevice,
                                        ze_device_p2p_properties_t *pP2PProperties) {
    pP2PProperties->accessSupported = true;
    pP2PProperties->atomicsSupported = false;

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

    pMemProperties->maxClockRate = deviceInfo.maxClockFrequency;
    pMemProperties->maxBusWidth = deviceInfo.addressBits;
    pMemProperties->totalSize = deviceInfo.globalMemSize;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) {
    pMemAccessProperties->hostAllocCapabilities =
        static_cast<ze_memory_access_capabilities_t>(ZE_MEMORY_ACCESS | ZE_MEMORY_ATOMIC_ACCESS);
    pMemAccessProperties->deviceAllocCapabilities =
        static_cast<ze_memory_access_capabilities_t>(ZE_MEMORY_ACCESS | ZE_MEMORY_ATOMIC_ACCESS);
    pMemAccessProperties->sharedSingleDeviceAllocCapabilities =
        static_cast<ze_memory_access_capabilities_t>(ZE_MEMORY_ACCESS | ZE_MEMORY_ATOMIC_ACCESS);
    pMemAccessProperties->sharedCrossDeviceAllocCapabilities =
        ze_memory_access_capabilities_t{};
    pMemAccessProperties->sharedSystemAllocCapabilities =
        ze_memory_access_capabilities_t{};

    return ZE_RESULT_SUCCESS;
}

static constexpr ze_fp_capabilities_t defaultFpFlags = static_cast<ze_fp_capabilities_t>(ZE_FP_CAPS_ROUND_TO_NEAREST |
                                                                                         ZE_FP_CAPS_ROUND_TO_ZERO |
                                                                                         ZE_FP_CAPS_ROUND_TO_INF |
                                                                                         ZE_FP_CAPS_INF_NAN |
                                                                                         ZE_FP_CAPS_DENORM |
                                                                                         ZE_FP_CAPS_FMA);

ze_result_t DeviceImp::getKernelProperties(ze_device_kernel_properties_t *pKernelProperties) {
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

    pKernelProperties->fp16Supported = true;
    pKernelProperties->int64AtomicsSupported = hardwareInfo.capabilityTable.ftrSupportsInteger64BitAtomics;
    pKernelProperties->fp64Supported = hardwareInfo.capabilityTable.ftrSupportsFP64;
    pKernelProperties->halfFpCapabilities = defaultFpFlags;
    pKernelProperties->singleFpCapabilities = hardwareInfo.capabilityTable.ftrSupports64BitMath ? ZE_FP_CAPS_ROUNDED_DIVIDE_SQRT : ZE_FP_CAPS_NONE;
    pKernelProperties->doubleFpCapabilities = hardwareInfo.capabilityTable.ftrSupportsFP64 ? defaultFpFlags : ZE_FP_CAPS_NONE;

    pKernelProperties->nativeKernelSupported.id[0] = 0;

    processAdditionalKernelProperties(hwHelper, pKernelProperties);

    pKernelProperties->maxArgumentsSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().maxParameterSize);

    pKernelProperties->printfBufferSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().printfBufferSize);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getProperties(ze_device_properties_t *pDeviceProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();
    const auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    pDeviceProperties->type = ZE_DEVICE_TYPE_GPU;

    pDeviceProperties->vendorId = deviceInfo.vendorId;

    pDeviceProperties->deviceId = hardwareInfo.platform.usDeviceID;

    uint32_t rootDeviceIndex = this->neoDevice->getRootDeviceIndex();

    memcpy_s(pDeviceProperties->uuid.id, sizeof(uint32_t), &pDeviceProperties->vendorId, sizeof(pDeviceProperties->vendorId));
    memcpy_s(pDeviceProperties->uuid.id + sizeof(uint32_t), sizeof(uint32_t), &pDeviceProperties->deviceId, sizeof(pDeviceProperties->deviceId));
    memcpy_s(pDeviceProperties->uuid.id + (2 * sizeof(uint32_t)), sizeof(uint32_t), &rootDeviceIndex, sizeof(rootDeviceIndex));

    pDeviceProperties->isSubdevice = isSubdevice;

    pDeviceProperties->subdeviceId = isSubdevice ? static_cast<NEO::SubDevice *>(neoDevice)->getSubDeviceIndex() : 0;

    pDeviceProperties->coreClockRate = deviceInfo.maxClockFrequency;

    pDeviceProperties->unifiedMemorySupported = true;

    pDeviceProperties->eccMemorySupported = this->neoDevice->getDeviceInfo().errorCorrectionSupport;

    pDeviceProperties->onDemandPageFaultsSupported = true;

    pDeviceProperties->maxCommandQueues = deviceInfo.maxOnDeviceQueues;

    pDeviceProperties->numAsyncComputeEngines = static_cast<uint32_t>(hwHelper.getGpgpuEngineInstances(hardwareInfo).size());

    pDeviceProperties->numAsyncCopyEngines = 1;

    pDeviceProperties->maxCommandQueuePriority = 0;

    pDeviceProperties->numThreadsPerEU = deviceInfo.numThreadsPerEU;

    pDeviceProperties->physicalEUSimdWidth = hwHelper.getMinimalSIMDSize();

    pDeviceProperties->numEUsPerSubslice = hardwareInfo.gtSystemInfo.MaxEuPerSubSlice;

    pDeviceProperties->numSubslicesPerSlice = hardwareInfo.gtSystemInfo.SubSliceCount / hardwareInfo.gtSystemInfo.SliceCount;

    pDeviceProperties->numSlices = hardwareInfo.gtSystemInfo.SliceCount * ((this->numSubDevices > 0) ? this->numSubDevices : 1);

    pDeviceProperties->timerResolution = this->neoDevice->getDeviceInfo().outProfilingTimerResolution;

    std::string name = "Intel(R) ";
    name += NEO::familyName[hardwareInfo.platform.eRenderCoreFamily];
    name += '\0';
    memcpy_s(pDeviceProperties->name, name.length(), name.c_str(), name.length());

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

ze_result_t DeviceImp::makeImageResident(ze_image_handle_t hImage) {
    auto alloc = Image::fromHandle(hImage)->getAllocation();
    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->makeResident(ArrayRef<NEO::GraphicsAllocation *>(&alloc, 1));
    return changeMemoryOperationStatusToL0ResultType(success);
}

ze_result_t DeviceImp::makeMemoryResident(void *ptr, size_t size) {
    auto alloc = getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr);
    if (alloc == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->makeResident(ArrayRef<NEO::GraphicsAllocation *>(&alloc->gpuAllocation, 1));
    return changeMemoryOperationStatusToL0ResultType(success);
}

ze_result_t DeviceImp::setIntermediateCacheConfig(ze_cache_config_t cacheConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DeviceImp::setLastLevelCacheConfig(ze_cache_config_t cacheConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t DeviceImp::getCacheProperties(ze_device_cache_properties_t *pCacheProperties) {
    const auto &hardwareInfo = this->getHwInfo();
    auto &hwHelper = NEO::HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    pCacheProperties->intermediateCacheControlSupported = false;

    pCacheProperties->intermediateCacheSize = getIntermediateCacheSize(hardwareInfo);

    pCacheProperties->intermediateCachelineSize = 0;

    pCacheProperties->lastLevelCacheSizeControlSupported = hwHelper.isL3Configurable(hardwareInfo);

    pCacheProperties->lastLevelCacheSize = static_cast<size_t>(hardwareInfo.gtSystemInfo.L3CacheSizeInKb * KB);

    pCacheProperties->lastLevelCachelineSize = this->neoDevice->getDeviceInfo().globalMemCachelineSize;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::imageGetProperties(const ze_image_desc_t *desc,
                                          ze_image_properties_t *pImageProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();

    if (deviceInfo.imageSupport) {
        pImageProperties->samplerFilterFlags = ZE_IMAGE_SAMPLER_FILTER_FLAGS_LINEAR;
    } else {
        pImageProperties->samplerFilterFlags = ZE_IMAGE_SAMPLER_FILTER_FLAGS_NONE;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getDeviceImageProperties(ze_device_image_properties_t *pDeviceImageProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();

    pDeviceImageProperties->supported = deviceInfo.imageSupport;
    pDeviceImageProperties->maxImageDims1D = static_cast<uint32_t>(deviceInfo.image2DMaxWidth);
    pDeviceImageProperties->maxImageDims2D = static_cast<uint32_t>(deviceInfo.image2DMaxHeight);
    pDeviceImageProperties->maxImageDims3D = static_cast<uint32_t>(deviceInfo.image3DMaxDepth);
    pDeviceImageProperties->maxImageBufferSize = this->neoDevice->getDeviceInfo().imageMaxBufferSize;
    pDeviceImageProperties->maxImageArraySlices = static_cast<uint32_t>(deviceInfo.imageMaxArraySize);
    pDeviceImageProperties->maxSamplers = this->neoDevice->getDeviceInfo().maxSamplers;
    pDeviceImageProperties->maxReadImageArgs = this->neoDevice->getDeviceInfo().maxReadImageArgs;
    pDeviceImageProperties->maxWriteImageArgs = this->neoDevice->getDeviceInfo().maxWriteImageArgs;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::systemBarrier() { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }

ze_result_t DeviceImp::activateMetricGroups(uint32_t count,
                                            zet_metric_group_handle_t *phMetricGroups) {
    return metricContext->activateMetricGroupsDeferred(count, phMetricGroups);
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

NEO::OSInterface &DeviceImp::getOsInterface() { return *neoDevice->getOSTime()->getOSInterface(); }

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

ze_result_t DeviceImp::registerCLMemory(cl_context context, cl_mem mem, void **ptr) {
    NEO::MemObj *memObj = static_cast<NEO::MemObj *>(mem);
    NEO::GraphicsAllocation *graphicsAllocation = memObj->getGraphicsAllocation();
    DEBUG_BREAK_IF(graphicsAllocation == nullptr);

    auto allocation = getDriverHandle()->allocateManagedMemoryFromHostPtr(
        this, graphicsAllocation->getUnderlyingBuffer(),
        graphicsAllocation->getUnderlyingBufferSize(), nullptr);

    *ptr = allocation->getUnderlyingBuffer();

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::registerCLProgram(cl_context context, cl_program program,
                                         ze_module_handle_t *phModule) {
    NEO::Program *neoProgram = static_cast<NEO::Program *>(program);

    if (neoProgram->getIsSpirV()) {
        size_t deviceBinarySize = 0;
        if (0 != neoProgram->getInfo(CL_PROGRAM_BINARY_SIZES, sizeof(deviceBinarySize), &deviceBinarySize, nullptr)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        std::vector<uint8_t> deviceBinary;
        deviceBinary.resize(deviceBinarySize);
        auto deviceBinaryPtr = deviceBinary.data();
        if (0 != neoProgram->getInfo(CL_PROGRAM_BINARIES, sizeof(void *), &deviceBinaryPtr, nullptr)) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        ze_module_desc_t module_desc;
        module_desc.version = ZE_MODULE_DESC_VERSION_CURRENT;
        module_desc.format = ZE_MODULE_FORMAT_NATIVE;
        module_desc.inputSize = deviceBinarySize;
        module_desc.pInputModule = deviceBinary.data();
        module_desc.pBuildFlags = nullptr;

        return createModule(&module_desc, phModule, nullptr);
    } else {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
}

ze_result_t DeviceImp::registerCLCommandQueue(cl_context context, cl_command_queue commandQueue,
                                              ze_command_queue_handle_t *phCommandQueue) {
    ze_command_queue_desc_t desc;
    desc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;
    desc.flags = ZE_COMMAND_QUEUE_FLAG_NONE;
    desc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    *phCommandQueue = CommandQueue::create(productFamily, this, csr, &desc);

    return ZE_RESULT_SUCCESS;
}

const NEO::HardwareInfo &DeviceImp::getHwInfo() const { return neoDevice->getHardwareInfo(); }

bool DeviceImp::isMultiDeviceCapable() const {
    return neoDevice->getNumAvailableDevices() > 1u;
}

Device *Device::create(DriverHandle *driverHandle, NEO::Device *neoDevice) {
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
    device->maxNumHwThreads = NEO::HwHelper::getMaxThreadsForVfe(neoDevice->getHardwareInfo());

    if (device->neoDevice->getNumAvailableDevices() == 1) {
        device->numSubDevices = 0;
    } else {
        device->numSubDevices = device->neoDevice->getNumAvailableDevices();
        for (uint32_t i = 0; i < device->numSubDevices; i++) {
            ze_device_handle_t subDevice = Device::create(driverHandle,
                                                          device->neoDevice->getDeviceById(i));
            if (subDevice == nullptr) {
                return nullptr;
            }
            reinterpret_cast<DeviceImp *>(subDevice)->isSubdevice = true;
            device->subDevices.push_back(static_cast<Device *>(subDevice));
        }
    }

    if (neoDevice->getCompilerInterface()) {
        device->getBuiltinFunctionsLib()->initFunctions();
        device->getBuiltinFunctionsLib()->initPageFaultFunction();
    }

    auto supportDualStorageSharedMemory = device->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(device->neoDevice->getRootDeviceIndex());
    if (NEO::DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get() != -1) {
        supportDualStorageSharedMemory = NEO::DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get();
    }

    if (supportDualStorageSharedMemory) {
        ze_command_queue_desc_t cmdQueueDesc;
        cmdQueueDesc.version = ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT;
        cmdQueueDesc.ordinal = 0;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        device->pageFaultCommandList =
            CommandList::createImmediate(
                device->neoDevice->getHardwareInfo().platform.eProductFamily, device, &cmdQueueDesc, true);
    }

    if (neoDevice->getDeviceInfo().debuggerActive) {
        auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();
        device->getSourceLevelDebugger()->notifyNewDevice(osInterface ? osInterface->getDeviceHandle() : 0);
    }

    if (neoDevice->getDeviceInfo().debuggerActive) {
        auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();

        auto debugSurface = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(
            {device->getRootDeviceIndex(),
             NEO::SipKernel::maxDbgSurfaceSize,
             NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY});
        device->setDebugSurface(debugSurface);

        device->getSourceLevelDebugger()
            ->notifyNewDevice(osInterface ? osInterface->getDeviceHandle() : 0);
    }

    return device;
}

DeviceImp::~DeviceImp() {
    for (uint32_t i = 0; i < this->numSubDevices; i++) {
        delete this->subDevices[i];
    }
    if (this->pageFaultCommandList) {
        this->pageFaultCommandList->destroy();
        this->pageFaultCommandList = nullptr;
    }
    metricContext.reset();
    builtins.reset();

    if (neoDevice->getDeviceInfo().debuggerActive) {
        getSourceLevelDebugger()->notifyDeviceDestruction();
        this->driverHandle->getMemoryManager()->freeGraphicsMemory(this->debugSurface);
        this->debugSurface = nullptr;
    }

    if (neoDevice) {
        neoDevice->decRefInternal();
    }
}

NEO::PreemptionMode DeviceImp::getDevicePreemptionMode() const {
    return neoDevice->getPreemptionMode();
}

const DeviceInfo &DeviceImp::getDeviceInfo() const {
    return neoDevice->getDeviceInfo();
}

NEO::Device *DeviceImp::getNEODevice() {
    return neoDevice;
}
} // namespace L0
