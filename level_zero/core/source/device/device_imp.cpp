/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"

#include "shared/source/assert_handler/assert_handler.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/device/device_info.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/default_descriptors.h"
#include "level_zero/core/source/helpers/properties_parser.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_build_log.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"
#include "level_zero/core/source/printf_handler/printf_handler.h"
#include "level_zero/core/source/sampler/sampler.h"
#include "level_zero/driver_experimental/zex_graph.h"
#include "level_zero/driver_experimental/zex_module.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/ze_intel_gpu.h"

#include "encode_surface_state_args.h"

#include <algorithm>
#include <array>

namespace L0 {

DeviceImp::DeviceImp() {
    bcsSplit = std::make_unique<BcsSplit>(*this);
};

void DeviceImp::bcsSplitReleaseResources() {
    bcsSplit->releaseResources();
}

void DeviceImp::setDriverHandle(DriverHandle *driverHandle) {
    this->driverHandle = driverHandle;
}

ze_result_t DeviceImp::getStatus() {
    if (this->resourcesReleased) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }
    auto engines = neoDevice->getAllEngines();
    for (auto &engine : engines) {
        auto csr = engine.commandStreamReceiver;
        if (csr->isGpuHangDetected()) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
    }
    return ZE_RESULT_SUCCESS;
}

bool DeviceImp::submitCopyForP2P(DeviceImp *peerDevice, ze_result_t &ret) {
    auto canAccessPeer = false;
    ze_command_list_handle_t commandList = nullptr;
    ze_command_list_desc_t listDescriptor = {};
    listDescriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    listDescriptor.pNext = nullptr;
    listDescriptor.flags = 0;
    listDescriptor.commandQueueGroupOrdinal = 0;

    ze_command_queue_handle_t commandQueue = nullptr;
    ze_command_queue_desc_t queueDescriptor = {};
    queueDescriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDescriptor.pNext = nullptr;
    queueDescriptor.flags = 0;
    queueDescriptor.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDescriptor.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDescriptor.ordinal = 0;
    queueDescriptor.index = 0;

    ret = this->createInternalCommandList(&listDescriptor, &commandList);
    UNRECOVERABLE_IF(ret != ZE_RESULT_SUCCESS);
    ret = this->createInternalCommandQueue(&queueDescriptor, &commandQueue);
    UNRECOVERABLE_IF(ret != ZE_RESULT_SUCCESS);

    auto driverHandle = this->getDriverHandle();
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driverHandle);

    ze_context_handle_t context;
    ze_context_desc_t contextDesc = {};
    contextDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
    driverHandleImp->createContext(&contextDesc, 0u, nullptr, &context);
    ContextImp *contextImp = static_cast<ContextImp *>(context);

    void *memory = nullptr;
    void *peerMemory = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    ze_device_mem_alloc_desc_t peerDeviceDesc = {};
    peerDeviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    peerDeviceDesc.ordinal = 0;
    peerDeviceDesc.flags = 0;
    peerDeviceDesc.pNext = nullptr;

    contextImp->allocDeviceMem(this->toHandle(), &deviceDesc, 8, 1, &memory);
    contextImp->allocDeviceMem(peerDevice->toHandle(), &peerDeviceDesc, 8, 1, &peerMemory);

    CmdListMemoryCopyParams memoryCopyParams = {};
    ret = L0::CommandList::fromHandle(commandList)->appendMemoryCopy(peerMemory, memory, 8, nullptr, 0, nullptr, memoryCopyParams);
    L0::CommandList::fromHandle(commandList)->close();

    if (ret == ZE_RESULT_SUCCESS) {
        ret = L0::CommandQueue::fromHandle(commandQueue)->executeCommandLists(1, &commandList, nullptr, true, nullptr, nullptr);
        if (ret == ZE_RESULT_SUCCESS) {

            ret = L0::CommandQueue::fromHandle(commandQueue)->synchronize(std::numeric_limits<uint64_t>::max());
            if (ret == ZE_RESULT_SUCCESS) {
                canAccessPeer = true;
            }
        }
    }

    contextImp->freeMem(peerMemory);
    contextImp->freeMem(memory);

    L0::CommandList::fromHandle(commandList)->destroy();
    L0::CommandQueue::fromHandle(commandQueue)->destroy();
    L0::Context::fromHandle(context)->destroy();

    if (ret != ZE_RESULT_ERROR_DEVICE_LOST) {
        ret = ZE_RESULT_SUCCESS;
    }

    return canAccessPeer;
}

ze_result_t DeviceImp::canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value) {
    bool canAccess = false;
    bool retVal = neoDevice->canAccessPeer(queryPeerAccess, fromHandle(hPeerDevice)->getNEODevice(), canAccess);

    *value = canAccess;
    return retVal ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_DEVICE_LOST;
}

bool DeviceImp::queryPeerAccess(NEO::Device &device, NEO::Device &peerDevice, bool &canAccess) {
    ze_result_t retVal = ZE_RESULT_SUCCESS;

    auto csr = device.getInternalEngine().commandStreamReceiver;
    if (!csr->isHardwareMode()) {
        canAccess = false;
        return false;
    }

    auto deviceImp = device.getSpecializedDevice<DeviceImp>();
    auto peerDeviceImp = peerDevice.getSpecializedDevice<DeviceImp>();

    uint32_t latency = std::numeric_limits<uint32_t>::max();
    uint32_t bandwidth = 0;
    ze_result_t result = deviceImp->queryFabricStats(peerDeviceImp, latency, bandwidth);
    if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE || bandwidth == 0) {
        canAccess = deviceImp->submitCopyForP2P(peerDeviceImp, retVal);
    } else {
        canAccess = true;
    }

    return retVal == ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createCommandList(const ze_command_list_desc_t *desc,
                                         ze_command_list_handle_t *commandList) {
    if (!this->isQueueGroupOrdinalValid(desc->commandQueueGroupOrdinal)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    uint32_t index = 0;
    uint32_t commandQueueGroupOrdinal = desc->commandQueueGroupOrdinal;
    NEO::SynchronizedDispatchMode syncDispatchMode = NEO::SynchronizedDispatchMode::disabled;
    bool copyOffloadHint = false;
    adjustCommandQueueDesc(commandQueueGroupOrdinal, index);

    NEO::EngineGroupType engineGroupType = getEngineGroupTypeForOrdinal(commandQueueGroupOrdinal);

    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    DeviceImp::CmdListCreateFunPtrT createCommandList = &CommandList::create;

    auto pNext = reinterpret_cast<const ze_base_desc_t *>(desc->pNext);

    while (pNext) {
        auto syncDispatchModeVal = getSyncDispatchMode(pNext);
        if (syncDispatchModeVal.has_value()) {
            syncDispatchMode = syncDispatchModeVal.value();
        }

        auto newCreateFunc = getCmdListCreateFunc(pNext);
        if (newCreateFunc) {
            createCommandList = newCreateFunc;
        }

        if (static_cast<uint32_t>(pNext->stype) == ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES) {
            copyOffloadHint = reinterpret_cast<const zex_intel_queue_copy_operations_offload_hint_exp_desc_t *>(pNext)->copyOffloadEnabled;
        }

        pNext = reinterpret_cast<const ze_base_desc_t *>(pNext->pNext);
    }

    *commandList = createCommandList(productFamily, this, engineGroupType, desc->flags, returnValue, false);

    if (returnValue != ZE_RESULT_SUCCESS) {
        return returnValue;
    }

    auto cmdList = static_cast<L0::CommandListImp *>(CommandList::fromHandle(*commandList));
    UNRECOVERABLE_IF(cmdList == nullptr);

    cmdList->setOrdinal(desc->commandQueueGroupOrdinal);

    if (syncDispatchMode != NEO::SynchronizedDispatchMode::disabled) {
        if (cmdList->isInOrderExecutionEnabled()) {
            cmdList->enableSynchronizedDispatch(syncDispatchMode);
        } else {
            returnValue = ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    auto &productHelper = getProductHelper();

    const bool copyOffloadAllowed = cmdList->isInOrderExecutionEnabled() && !getGfxCoreHelper().crossEngineCacheFlushRequired() &&
                                    (getL0GfxCoreHelper().getDefaultCopyOffloadMode(productHelper.useAdditionalBlitProperties()) != CopyOffloadModes::dualStream);

    if (copyOffloadHint && copyOffloadAllowed) {
        cmdList->enableCopyOperationOffload();
    }

    if (returnValue != ZE_RESULT_SUCCESS) {
        cmdList->destroy();
        cmdList = nullptr;
        *commandList = nullptr;
    }

    return returnValue;
}

ze_result_t DeviceImp::createInternalCommandList(const ze_command_list_desc_t *desc,
                                                 ze_command_list_handle_t *commandList) {
    NEO::EngineGroupType engineGroupType = getInternalEngineGroupType();

    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    *commandList = CommandList::create(productFamily, this, engineGroupType, desc->flags, returnValue, true);
    return returnValue;
}

ze_result_t DeviceImp::createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                                  ze_command_list_handle_t *phCommandList) {

    ze_command_queue_desc_t commandQueueDesc = defaultIntelCommandQueueDesc;

    if (desc) {
        commandQueueDesc = *desc;
    }

    if (!this->isQueueGroupOrdinalValid(commandQueueDesc.ordinal)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    adjustCommandQueueDesc(commandQueueDesc.ordinal, commandQueueDesc.index);

    NEO::EngineGroupType engineGroupType = getEngineGroupTypeForOrdinal(commandQueueDesc.ordinal);

    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    *phCommandList = CommandList::createImmediate(productFamily, this, &commandQueueDesc, false, engineGroupType, returnValue);
    if (returnValue == ZE_RESULT_SUCCESS) {
        CommandList::fromHandle(*phCommandList)->setOrdinal(commandQueueDesc.ordinal);
    }

    return returnValue;
}

void DeviceImp::adjustCommandQueueDesc(uint32_t &ordinal, uint32_t &index) {
    auto nodeOrdinal = NEO::debugManager.flags.NodeOrdinal.get();
    if (nodeOrdinal != -1) {
        const NEO::HardwareInfo &hwInfo = neoDevice->getHardwareInfo();
        const NEO::GfxCoreHelper &gfxCoreHelper = neoDevice->getGfxCoreHelper();
        auto &engineGroups = getActiveDevice()->getRegularEngineGroups();

        auto engineGroupType = gfxCoreHelper.getEngineGroupType(static_cast<aub_stream::EngineType>(nodeOrdinal), NEO::EngineUsage::regular, hwInfo);
        uint32_t currentEngineIndex = 0u;
        for (const auto &engine : engineGroups) {
            if (engine.engineGroupType == engineGroupType) {
                ordinal = currentEngineIndex;
                break;
            }
            currentEngineIndex++;
        }
        currentEngineIndex = 0u;
        for (const auto &engine : engineGroups[ordinal].engines) {
            if (engine.getEngineType() == static_cast<aub_stream::EngineType>(nodeOrdinal)) {
                index = currentEngineIndex;
                break;
            }
            currentEngineIndex++;
        }
    }
}

ze_result_t DeviceImp::createCommandQueue(const ze_command_queue_desc_t *desc,
                                          ze_command_queue_handle_t *commandQueue) {
    auto &platform = neoDevice->getHardwareInfo().platform;

    NEO::CommandStreamReceiver *csr = nullptr;

    ze_command_queue_desc_t commandQueueDesc = *desc;
    adjustCommandQueueDesc(commandQueueDesc.ordinal, commandQueueDesc.index);

    if (!this->isQueueGroupOrdinalValid(commandQueueDesc.ordinal)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    bool isCopyOnly = NEO::EngineHelper::isCopyOnlyEngineType(getEngineGroupTypeForOrdinal(commandQueueDesc.ordinal));

    auto queueProperties = CommandQueue::extractQueueProperties(*desc);

    if (queueProperties.interruptHint && !neoDevice->getProductHelper().isInterruptSupported()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto ret = getCsrForOrdinalAndIndex(&csr, commandQueueDesc.ordinal, commandQueueDesc.index, commandQueueDesc.priority, queueProperties.priorityLevel, queueProperties.interruptHint);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    UNRECOVERABLE_IF(csr == nullptr);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    *commandQueue = CommandQueue::create(platform.eProductFamily, this, csr, &commandQueueDesc, isCopyOnly, false, false, returnValue);
    return returnValue;
}

ze_result_t DeviceImp::createInternalCommandQueue(const ze_command_queue_desc_t *desc,
                                                  ze_command_queue_handle_t *commandQueue) {
    auto &platform = neoDevice->getHardwareInfo().platform;

    auto internalEngine = this->getActiveDevice()->getInternalEngine();
    auto csr = internalEngine.commandStreamReceiver;
    auto engineGroupType = getInternalEngineGroupType();
    auto isCopyOnly = NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType);

    UNRECOVERABLE_IF(csr == nullptr);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    *commandQueue = CommandQueue::create(platform.eProductFamily, this, csr, desc, isCopyOnly, true, false, returnValue);
    return returnValue;
}

void DeviceImp::populateSubDeviceCopyEngineGroups() {
    NEO::Device *activeDevice = this->getActiveDevice();
    if (this->isImplicitScalingCapable() == false || activeDevice->getNumSubDevices() == 0) {
        return;
    }

    for (auto &subDevice : activeDevice->getSubDevices()) {
        if (subDevice) {

            NEO::Device *activeSubDevice = subDevice;

            auto &subDeviceEngineGroups = activeSubDevice->getRegularEngineGroups();
            uint32_t numSubDeviceEngineGroups = static_cast<uint32_t>(subDeviceEngineGroups.size());

            for (uint32_t subDeviceQueueGroupsIter = 0; subDeviceQueueGroupsIter < numSubDeviceEngineGroups; subDeviceQueueGroupsIter++) {
                if (NEO::EngineHelper::isCopyOnlyEngineType(subDeviceEngineGroups[subDeviceQueueGroupsIter].engineGroupType)) {
                    subDeviceCopyEngineGroups.push_back(subDeviceEngineGroups[subDeviceQueueGroupsIter]);
                }
            }

            if (!activeDevice->getRootDeviceEnvironment().isExposeSingleDeviceMode()) {
                break;
            }
        }
    }
}

uint32_t DeviceImp::getCopyQueueGroupsFromSubDevice(uint32_t numberOfSubDeviceCopyEngineGroupsRequested,
                                                    ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    uint32_t numSubDeviceCopyEngineGroups = static_cast<uint32_t>(this->subDeviceCopyEngineGroups.size());

    if (pCommandQueueGroupProperties == nullptr) {
        return numSubDeviceCopyEngineGroups;
    }

    auto &rootDeviceEnv = this->neoDevice->getRootDeviceEnvironment();
    auto &l0GfxCoreHelper = rootDeviceEnv.getHelper<L0GfxCoreHelper>();
    auto &productHelper = rootDeviceEnv.getHelper<NEO::ProductHelper>();

    uint32_t subDeviceQueueGroupsIter = 0;
    for (; subDeviceQueueGroupsIter < std::min(numSubDeviceCopyEngineGroups, numberOfSubDeviceCopyEngineGroupsRequested); subDeviceQueueGroupsIter++) {
        pCommandQueueGroupProperties[subDeviceQueueGroupsIter].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
        pCommandQueueGroupProperties[subDeviceQueueGroupsIter].maxMemoryFillPatternSize = productHelper.getMaxFillPatternSizeForCopyEngine();

        l0GfxCoreHelper.setAdditionalGroupProperty(pCommandQueueGroupProperties[subDeviceQueueGroupsIter], this->subDeviceCopyEngineGroups[subDeviceQueueGroupsIter]);
        pCommandQueueGroupProperties[subDeviceQueueGroupsIter].numQueues = static_cast<uint32_t>(this->subDeviceCopyEngineGroups[subDeviceQueueGroupsIter].engines.size());
    }

    return subDeviceQueueGroupsIter;
}

uint32_t DeviceImp::getCopyEngineOrdinal() const {
    auto retVal = tryGetCopyEngineOrdinal();
    UNRECOVERABLE_IF(!retVal.has_value());
    return retVal.value();
}

std::optional<uint32_t> DeviceImp::tryGetCopyEngineOrdinal() const {
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    uint32_t i = 0;
    for (; i < static_cast<uint32_t>(engineGroups.size()); i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            return i;
        }
    }

    if (this->subDeviceCopyEngineGroups.size() != 0) {
        return i;
    }
    return std::nullopt;
}

ze_result_t DeviceImp::getCommandQueueGroupProperties(uint32_t *pCount,
                                                      ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    NEO::Device *activeDevice = getActiveDevice();
    auto &engineGroups = activeDevice->getRegularEngineGroups();
    uint32_t numEngineGroups = static_cast<uint32_t>(engineGroups.size());

    uint32_t numSubDeviceCopyEngineGroups = getCopyQueueGroupsFromSubDevice(std::numeric_limits<uint32_t>::max(), nullptr);

    uint32_t totalEngineGroups = numEngineGroups + numSubDeviceCopyEngineGroups;

    uint32_t additionalEngines = getAdditionalEngines(std::numeric_limits<uint32_t>::max(), nullptr);
    totalEngineGroups += additionalEngines;

    if (*pCount == 0) {
        *pCount = totalEngineGroups;
        return ZE_RESULT_SUCCESS;
    }

    auto &rootDeviceEnv = this->neoDevice->getRootDeviceEnvironment();
    auto &l0GfxCoreHelper = rootDeviceEnv.getHelper<L0GfxCoreHelper>();
    auto &productHelper = rootDeviceEnv.getHelper<NEO::ProductHelper>();

    *pCount = std::min(totalEngineGroups, *pCount);
    for (uint32_t i = 0; i < std::min(numEngineGroups, *pCount); i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::renderCompute) {
            pCommandQueueGroupProperties[i].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS;
            pCommandQueueGroupProperties[i].maxMemoryFillPatternSize = std::numeric_limits<size_t>::max();
        }
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::compute) {
            pCommandQueueGroupProperties[i].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS;
            pCommandQueueGroupProperties[i].maxMemoryFillPatternSize = std::numeric_limits<size_t>::max();
        }
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::copy) {
            pCommandQueueGroupProperties[i].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
            pCommandQueueGroupProperties[i].maxMemoryFillPatternSize = productHelper.getMaxFillPatternSizeForCopyEngine();
        }
        l0GfxCoreHelper.setAdditionalGroupProperty(pCommandQueueGroupProperties[i], engineGroups[i]);
        pCommandQueueGroupProperties[i].numQueues = static_cast<uint32_t>(engineGroups[i].engines.size());
    }

    if (*pCount > numEngineGroups) {
        uint32_t remainingCopyEngineGroups = *pCount - numEngineGroups - additionalEngines;
        getCopyQueueGroupsFromSubDevice(remainingCopyEngineGroups, &pCommandQueueGroupProperties[numEngineGroups]);
    }

    if (*pCount > (numEngineGroups + numSubDeviceCopyEngineGroups)) {
        getAdditionalEngines(additionalEngines, &pCommandQueueGroupProperties[numEngineGroups + numSubDeviceCopyEngineGroups]);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createImage(const ze_image_desc_t *desc, ze_image_handle_t *phImage) {
    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    Image *pImage = nullptr;

    if (neoDevice->getDeviceInfo().imageSupport == false) {
        *phImage = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

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
    if (*sampler == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    } else {
        return ZE_RESULT_SUCCESS;
    }
}

ze_result_t DeviceImp::createModule(const ze_module_desc_t *desc, ze_module_handle_t *module,
                                    ze_module_build_log_handle_t *buildLog, ModuleType type) {
    ModuleBuildLog *moduleBuildLog = nullptr;

    if (buildLog) {
        moduleBuildLog = ModuleBuildLog::create();
        *buildLog = moduleBuildLog->toHandle();
    }

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto modulePtr = Module::create(this, desc, moduleBuildLog, type, &result);
    if (modulePtr != nullptr) {
        *module = modulePtr;
    }

    return result;
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

NEO::EngineGroupType DeviceImp::getInternalEngineGroupType() {
    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto internalEngine = this->getActiveDevice()->getInternalEngine();
    auto internalEngineType = internalEngine.getEngineType();
    auto internalEngineUsage = internalEngine.getEngineUsage();
    return gfxCoreHelper.getEngineGroupType(internalEngineType, internalEngineUsage, getHwInfo());
}

void DeviceImp::getP2PPropertiesDirectFabricConnection(DeviceImp *peerDeviceImp,
                                                       ze_device_p2p_bandwidth_exp_properties_t *bandwidthPropertiesDesc) {

    auto driverHandleImp = static_cast<DriverHandleImp *>(getDriverHandle());
    if (driverHandleImp->fabricVertices.empty()) {
        driverHandleImp->initializeVertexes();
    }

    if (this->fabricVertex != nullptr && peerDeviceImp->fabricVertex != nullptr) {
        uint32_t directEdgeCount = 0;

        driverHandleImp->fabricEdgeGetExp(this->fabricVertex,
                                          peerDeviceImp->fabricVertex,
                                          &directEdgeCount,
                                          nullptr);
        if (directEdgeCount > 0) {
            std::vector<ze_fabric_edge_handle_t> edges(directEdgeCount);
            driverHandleImp->fabricEdgeGetExp(this->fabricVertex,
                                              peerDeviceImp->fabricVertex,
                                              &directEdgeCount,
                                              edges.data());

            for (const auto &edge : edges) {
                auto fabricEdge = FabricEdge::fromHandle(edge);
                ze_fabric_edge_exp_properties_t edgeProperties{};
                fabricEdge->getProperties(&edgeProperties);

                if (strstr(edgeProperties.model, "XeLink") != nullptr) {
                    bandwidthPropertiesDesc->logicalBandwidth = edgeProperties.bandwidth;
                    bandwidthPropertiesDesc->physicalBandwidth = edgeProperties.bandwidth;
                    bandwidthPropertiesDesc->bandwidthUnit = edgeProperties.bandwidthUnit;

                    bandwidthPropertiesDesc->logicalLatency = edgeProperties.latency;
                    bandwidthPropertiesDesc->physicalLatency = edgeProperties.latency;
                    bandwidthPropertiesDesc->latencyUnit = edgeProperties.latencyUnit;
                    break;
                }
            }
        }
    }
}

ze_result_t DeviceImp::getP2PProperties(ze_device_handle_t hPeerDevice,
                                        ze_device_p2p_properties_t *pP2PProperties) {

    DeviceImp *peerDevice = static_cast<DeviceImp *>(Device::fromHandle(hPeerDevice));
    if (this->getNEODevice()->getRootDeviceIndex() == peerDevice->getNEODevice()->getRootDeviceIndex()) {
        pP2PProperties->flags = ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS | ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS;
    } else {
        ze_bool_t peerAccessAvaiable = false;
        canAccessPeer(hPeerDevice, &peerAccessAvaiable);
        if (peerAccessAvaiable) {
            pP2PProperties->flags = ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS;
            ze_device_p2p_bandwidth_exp_properties_t p2pBandwidthProperties{};
            getP2PPropertiesDirectFabricConnection(peerDevice, &p2pBandwidthProperties);
            if (std::max(p2pBandwidthProperties.physicalBandwidth, p2pBandwidthProperties.logicalBandwidth) > 0u) {
                pP2PProperties->flags |= ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS;
            }
        }
    }

    if (pP2PProperties->pNext) {
        ze_base_desc_t *extendedDesc = reinterpret_cast<ze_base_desc_t *>(pP2PProperties->pNext);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_DEVICE_P2P_BANDWIDTH_EXP_PROPERTIES) {
            ze_device_p2p_bandwidth_exp_properties_t *bandwidthPropertiesDesc =
                reinterpret_cast<ze_device_p2p_bandwidth_exp_properties_t *>(extendedDesc);

            bandwidthPropertiesDesc->logicalBandwidth = 0;
            bandwidthPropertiesDesc->physicalBandwidth = 0;
            bandwidthPropertiesDesc->bandwidthUnit = ZE_BANDWIDTH_UNIT_UNKNOWN;

            bandwidthPropertiesDesc->logicalLatency = 0;
            bandwidthPropertiesDesc->physicalLatency = 0;
            bandwidthPropertiesDesc->latencyUnit = ZE_LATENCY_UNIT_UNKNOWN;

            getP2PPropertiesDirectFabricConnection(peerDevice, bandwidthPropertiesDesc);
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getRootDevice(ze_device_handle_t *phRootDevice) {
    // Given FLAT device Hierarchy mode, then nullptr is returned for the root device since no traversal is allowed.
    if (this->neoDevice->getExecutionEnvironment()->getDeviceHierarchyMode() == NEO::DeviceHierarchyMode::flat) {
        *phRootDevice = nullptr;
        return ZE_RESULT_SUCCESS;
    }
    *phRootDevice = this->rootDevice;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getPciProperties(ze_pci_ext_properties_t *pPciProperties) {
    if (!driverInfo) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    auto pciBusInfo = driverInfo->getPciBusInfo();
    auto isPciValid = [&](auto pci) -> bool {
        return (pci.pciDomain != NEO::PhysicalDevicePciBusInfo::invalidValue &&
                pci.pciBus != NEO::PhysicalDevicePciBusInfo::invalidValue &&
                pci.pciDevice != NEO::PhysicalDevicePciBusInfo::invalidValue &&
                pci.pciFunction != NEO::PhysicalDevicePciBusInfo::invalidValue);
    };
    if (!isPciValid(pciBusInfo)) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    pPciProperties->address = {pciBusInfo.pciDomain, pciBusInfo.pciBus,
                               pciBusInfo.pciDevice, pciBusInfo.pciFunction};
    pPciProperties->maxSpeed = pciMaxSpeed;
    return ZE_RESULT_SUCCESS;
}

static const std::map<uint32_t, const char *> memoryTypeNames = {
    {0, "DDR"},
    {1, "DDR"},
    {2, "HBM"},
    {3, "HBM"},
    {4, "DDR"},
    {5, "HBM"},
    {6, "DDR"},
    {7, "DDR"},
    {8, "HBM"},
    {9, "HBM"},
};

const char *DeviceImp::getDeviceMemoryName() {
    auto it = memoryTypeNames.find(getHwInfo().gtSystemInfo.MemoryType);
    if (it != memoryTypeNames.end()) {
        return it->second;
    }
    UNRECOVERABLE_IF(true);
    return "unknown memory type";
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
    auto &hwInfo = this->getHwInfo();
    auto &productHelper = this->getProductHelper();
    strcpy_s(pMemProperties->name, ZE_MAX_DEVICE_NAME, getDeviceMemoryName());
    auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();
    pMemProperties->maxClockRate = productHelper.getDeviceMemoryMaxClkRate(hwInfo, osInterface, 0);
    pMemProperties->maxBusWidth = deviceInfo.addressBits;

    if (this->isImplicitScalingCapable() ||
        this->getNEODevice()->getNumGenericSubDevices() == 0) {
        pMemProperties->totalSize = deviceInfo.globalMemSize;
    } else {
        pMemProperties->totalSize = deviceInfo.globalMemSize / this->numSubDevices;
    }

    pMemProperties->flags = 0;

    ze_base_properties_t *pNext = static_cast<ze_base_properties_t *>(pMemProperties->pNext);
    while (pNext) {

        if (pNext->stype == ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES) {
            auto extendedProperties = reinterpret_cast<ze_device_memory_ext_properties_t *>(pNext);

            // GT_MEMORY_TYPES map to ze_device_memory_ext_type_t
            const std::array<ze_device_memory_ext_type_t, 10> sysInfoMemType = {
                ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4,
                ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5,
                ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
                ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
                ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6,
                ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
                ZE_DEVICE_MEMORY_EXT_TYPE_DDR5,
                ZE_DEVICE_MEMORY_EXT_TYPE_GDDR7,
                ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
                ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
            };

            UNRECOVERABLE_IF(hwInfo.gtSystemInfo.MemoryType >= sizeof(sysInfoMemType));
            extendedProperties->type = sysInfoMemType[hwInfo.gtSystemInfo.MemoryType];

            uint32_t enabledSubDeviceCount = 1;
            if (this->isImplicitScalingCapable()) {
                enabledSubDeviceCount = static_cast<uint32_t>(neoDevice->getDeviceBitfield().count());
            }
            extendedProperties->physicalSize = productHelper.getDeviceMemoryPhysicalSizeInBytes(osInterface, 0) * enabledSubDeviceCount;
            const uint64_t bandwidthInBytesPerSecond = productHelper.getDeviceMemoryMaxBandWidthInBytesPerSecond(hwInfo, osInterface, 0) * enabledSubDeviceCount;

            // Convert to nano-seconds range
            extendedProperties->readBandwidth = static_cast<uint32_t>(bandwidthInBytesPerSecond * 1e-9);
            extendedProperties->writeBandwidth = extendedProperties->readBandwidth;
            extendedProperties->bandwidthUnit = ZE_BANDWIDTH_UNIT_BYTES_PER_NANOSEC;
        }
        getAdditionalMemoryExtProperties(pNext, this->getNEODevice()->getHardwareInfo());
        pNext = static_cast<ze_base_properties_t *>(pNext->pNext);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getMemoryAccessProperties(ze_device_memory_access_properties_t *pMemAccessProperties) {
    auto &hwInfo = this->getHwInfo();
    auto &productHelper = this->getProductHelper();

    pMemAccessProperties->hostAllocCapabilities =
        static_cast<ze_memory_access_cap_flags_t>(productHelper.getHostMemCapabilities(&hwInfo));

    pMemAccessProperties->deviceAllocCapabilities =
        static_cast<ze_memory_access_cap_flags_t>(productHelper.getDeviceMemCapabilities());

    auto memoryManager{this->getDriverHandle()->getMemoryManager()};
    const bool isKmdMigrationAvailable{memoryManager->isKmdMigrationAvailable(this->getRootDeviceIndex())};
    pMemAccessProperties->sharedSingleDeviceAllocCapabilities =
        static_cast<ze_memory_access_cap_flags_t>(productHelper.getSingleDeviceSharedMemCapabilities(isKmdMigrationAvailable));

    auto defaultContext = static_cast<ContextImp *>(getDriverHandle()->getDefaultContext());
    auto multiDeviceWithSingleRoot = defaultContext->rootDeviceIndices.size() == 1 && (defaultContext->getNumDevices() > 1 || isSubdevice);

    pMemAccessProperties->sharedCrossDeviceAllocCapabilities = {};
    if (multiDeviceWithSingleRoot || (isKmdMigrationAvailable &&
                                      memoryManager->hasPageFaultsEnabled(*this->getNEODevice()) &&
                                      NEO::debugManager.flags.EnableConcurrentSharedCrossP2PDeviceAccess.get() == 1)) {
        pMemAccessProperties->sharedCrossDeviceAllocCapabilities = ZE_MEMORY_ACCESS_CAP_FLAG_RW |
                                                                   ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT |
                                                                   ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC |
                                                                   ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC;
    }

    pMemAccessProperties->sharedSystemAllocCapabilities =
        static_cast<ze_memory_access_cap_flags_t>(productHelper.getSharedSystemMemCapabilities(&hwInfo));

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
    auto &compilerProductHelper = this->neoDevice->getCompilerProductHelper();
    const auto &deviceInfo = this->getDeviceInfo();
    const auto &productHelper = this->getProductHelper();
    auto releaseHelper = this->neoDevice->getReleaseHelper();

    std::string ilVersion = deviceInfo.ilVersion;
    size_t majorVersionPos = ilVersion.find('_');
    size_t minorVersionPos = ilVersion.find('.');

    if (majorVersionPos != std::string::npos && minorVersionPos != std::string::npos) {
        uint32_t majorSpirvVersion = static_cast<uint32_t>(std::stoul(ilVersion.substr(majorVersionPos + 1)));
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

    if (NEO::debugManager.flags.OverrideDefaultFP64Settings.get() == 1) {
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
        } else if (hardwareInfo.capabilityTable.ftrSupportsFP64Emulation) {
            if (neoDevice->getExecutionEnvironment()->isFP64EmulationEnabled()) {
                pKernelProperties->flags |= ZE_DEVICE_MODULE_FLAG_FP64;
                pKernelProperties->fp64flags = defaultFpFlags | ZE_DEVICE_FP_FLAG_SOFT_FLOAT;
            }
        }
    }

    pKernelProperties->nativeKernelSupported.id[0] = 0;
    pKernelProperties->flags |= ZE_DEVICE_MODULE_FLAG_DP4A;
    pKernelProperties->maxArgumentsSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().maxParameterSize);
    pKernelProperties->printfBufferSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().printfBufferSize);

    void *pNext = pKernelProperties->pNext;
    while (pNext) {
        ze_base_desc_t *extendedProperties = reinterpret_cast<ze_base_desc_t *>(pNext);
        if (extendedProperties->stype == ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES) {
            ze_float_atomic_ext_properties_t *floatProperties =
                reinterpret_cast<ze_float_atomic_ext_properties_t *>(extendedProperties);
            compilerProductHelper.getKernelFp16AtomicCapabilities(releaseHelper, floatProperties->fp16Flags);
            compilerProductHelper.getKernelFp32AtomicCapabilities(floatProperties->fp32Flags);
            compilerProductHelper.getKernelFp64AtomicCapabilities(floatProperties->fp64Flags);
            static_assert(ZE_DEVICE_FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE == FpAtomicExtFlags::globalLoadStore, "Mismatch between internal and API - specific capabilities.");
            static_assert(ZE_DEVICE_FP_ATOMIC_EXT_FLAG_GLOBAL_ADD == FpAtomicExtFlags::globalAdd, "Mismatch between internal and API - specific capabilities.");
            static_assert(ZE_DEVICE_FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX == FpAtomicExtFlags::globalMinMax, "Mismatch between internal and API - specific capabilities.");
            static_assert(ZE_DEVICE_FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE == FpAtomicExtFlags::localLoadStore, "Mismatch between internal and API - specific capabilities.");
            static_assert(ZE_DEVICE_FP_ATOMIC_EXT_FLAG_LOCAL_ADD == FpAtomicExtFlags::localAdd, "Mismatch between internal and API - specific capabilities.");
            static_assert(ZE_DEVICE_FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX == FpAtomicExtFlags::localMinMax, "Mismatch between internal and API - specific capabilities.");
        } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_SCHEDULING_HINT_EXP_PROPERTIES) {
            ze_scheduling_hint_exp_properties_t *hintProperties =
                reinterpret_cast<ze_scheduling_hint_exp_properties_t *>(extendedProperties);
            auto supportedThreadArbitrationPolicies = productHelper.getKernelSupportedThreadArbitrationPolicies();
            hintProperties->schedulingHintFlags = 0;
            for (auto &p : supportedThreadArbitrationPolicies) {
                switch (p) {
                case NEO::ThreadArbitrationPolicy::AgeBased:
                    hintProperties->schedulingHintFlags |= ZE_SCHEDULING_HINT_EXP_FLAG_OLDEST_FIRST;
                    break;
                case NEO::ThreadArbitrationPolicy::RoundRobin:
                    hintProperties->schedulingHintFlags |= ZE_SCHEDULING_HINT_EXP_FLAG_ROUND_ROBIN;
                    break;
                case NEO::ThreadArbitrationPolicy::RoundRobinAfterDependency:
                    hintProperties->schedulingHintFlags |= ZE_SCHEDULING_HINT_EXP_FLAG_STALL_BASED_ROUND_ROBIN;
                    break;
                }
            }
        } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_RAYTRACING_EXT_PROPERTIES) {
            ze_device_raytracing_ext_properties_t *rtProperties =
                reinterpret_cast<ze_device_raytracing_ext_properties_t *>(extendedProperties);

            if (releaseHelper && releaseHelper->isRayTracingSupported()) {
                rtProperties->flags = ZE_DEVICE_RAYTRACING_EXT_FLAG_RAYQUERY;
                rtProperties->maxBVHLevels = NEO::RayTracingHelper::maxBvhLevels;

                if (NEO::debugManager.flags.SetMaxBVHLevels.get() != -1) {
                    rtProperties->maxBVHLevels = static_cast<uint32_t>(NEO::debugManager.flags.SetMaxBVHLevels.get());
                }

            } else {
                rtProperties->flags = 0;
                rtProperties->maxBVHLevels = 0;
            }
        } else if (extendedProperties->stype == ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES) {
            ze_intel_device_module_dp_exp_properties_t *dpProperties =
                reinterpret_cast<ze_intel_device_module_dp_exp_properties_t *>(extendedProperties);
            dpProperties->flags = 0u;
            dpProperties->flags |= ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A;

            auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();
            auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
            if (compilerProductHelper.isDotProductAccumulateSystolicSupported(releaseHelper)) {
                dpProperties->flags |= ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS;
            }
        } else if (static_cast<uint32_t>(extendedProperties->stype) == ZEX_STRUCTURE_DEVICE_MODULE_REGISTER_FILE_EXP) {
            zex_device_module_register_file_exp_t *properties = reinterpret_cast<zex_device_module_register_file_exp_t *>(extendedProperties);

            const auto supportedNumGrfs = this->getProductHelper().getSupportedNumGrfs(this->getNEODevice()->getReleaseHelper());

            const auto registerFileSizesCount = static_cast<uint32_t>(supportedNumGrfs.size());

            if (properties->registerFileSizes == nullptr) {
                properties->registerFileSizesCount = registerFileSizesCount;
            } else {
                properties->registerFileSizesCount = std::min(properties->registerFileSizesCount, registerFileSizesCount);
                const auto sizeToCopy = sizeof(*properties->registerFileSizes) * properties->registerFileSizesCount;
                memcpy_s(properties->registerFileSizes, sizeToCopy, supportedNumGrfs.data(), sizeToCopy);
            }
        } else {
            getExtendedDeviceModuleProperties(extendedProperties);
        }

        pNext = const_cast<void *>(extendedProperties->pNext);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getVectorWidthPropertiesExt(uint32_t *pCount, ze_device_vector_width_properties_ext_t *pVectorWidthProperties) {
    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }
    if (pVectorWidthProperties == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
    if (*pCount > 1) {
        *pCount = 1;
    }
    auto &gfxCoreHelper = this->neoDevice->getGfxCoreHelper();
    auto vectorWidthSize = gfxCoreHelper.getMinimalSIMDSize();
    pVectorWidthProperties[0].vector_width_size = vectorWidthSize;
    pVectorWidthProperties[0].preferred_vector_width_char = gfxCoreHelper.getPreferredVectorWidthChar(vectorWidthSize);
    pVectorWidthProperties[0].preferred_vector_width_short = gfxCoreHelper.getPreferredVectorWidthShort(vectorWidthSize);
    pVectorWidthProperties[0].preferred_vector_width_int = gfxCoreHelper.getPreferredVectorWidthInt(vectorWidthSize);
    pVectorWidthProperties[0].preferred_vector_width_long = gfxCoreHelper.getPreferredVectorWidthLong(vectorWidthSize);
    pVectorWidthProperties[0].preferred_vector_width_float = gfxCoreHelper.getPreferredVectorWidthFloat(vectorWidthSize);
    pVectorWidthProperties[0].preferred_vector_width_half = gfxCoreHelper.getPreferredVectorWidthHalf(vectorWidthSize);
    pVectorWidthProperties[0].native_vector_width_char = gfxCoreHelper.getNativeVectorWidthChar(vectorWidthSize);
    pVectorWidthProperties[0].native_vector_width_short = gfxCoreHelper.getNativeVectorWidthShort(vectorWidthSize);
    pVectorWidthProperties[0].native_vector_width_int = gfxCoreHelper.getNativeVectorWidthInt(vectorWidthSize);
    pVectorWidthProperties[0].native_vector_width_long = gfxCoreHelper.getNativeVectorWidthLong(vectorWidthSize);
    pVectorWidthProperties[0].native_vector_width_float = gfxCoreHelper.getNativeVectorWidthFloat(vectorWidthSize);
    pVectorWidthProperties[0].native_vector_width_half = gfxCoreHelper.getNativeVectorWidthHalf(vectorWidthSize);

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getProperties(ze_device_properties_t *pDeviceProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();
    const auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    auto &gfxCoreHelper = this->neoDevice->getGfxCoreHelper();
    const auto &l0GfxCoreHelper = this->getL0GfxCoreHelper();
    auto releaseHelper = this->neoDevice->getReleaseHelper();

    pDeviceProperties->type = ZE_DEVICE_TYPE_GPU;

    pDeviceProperties->vendorId = deviceInfo.vendorId;

    pDeviceProperties->deviceId = hardwareInfo.platform.usDeviceID;

    pDeviceProperties->flags = 0u;

    std::array<uint8_t, NEO::ProductHelper::uuidSize> deviceUuid;
    if (this->neoDevice->getUuid(deviceUuid) == false) {
        this->neoDevice->generateUuid(deviceUuid);
    }
    std::copy_n(std::begin(deviceUuid), ZE_MAX_DEVICE_UUID_SIZE, std::begin(pDeviceProperties->uuid.id));

    pDeviceProperties->subdeviceId = isSubdevice ? static_cast<NEO::SubDevice *>(neoDevice)->getSubDeviceIndex() : 0;

    pDeviceProperties->coreClockRate = deviceInfo.maxClockFrequency;

    pDeviceProperties->maxMemAllocSize = this->neoDevice->getDeviceInfo().maxMemAllocSize;

    pDeviceProperties->maxCommandQueuePriority = 0;

    pDeviceProperties->maxHardwareContexts = 1024 * 64;

    pDeviceProperties->numThreadsPerEU = deviceInfo.numThreadsPerEU;

    pDeviceProperties->physicalEUSimdWidth = gfxCoreHelper.getMinimalSIMDSize();

    pDeviceProperties->numEUsPerSubslice = hardwareInfo.gtSystemInfo.MaxEuPerSubSlice;

    if (NEO::debugManager.flags.DebugApiUsed.get() == 1) {
        pDeviceProperties->numSubslicesPerSlice = hardwareInfo.gtSystemInfo.MaxSubSlicesSupported / hardwareInfo.gtSystemInfo.MaxSlicesSupported;
    } else {
        pDeviceProperties->numSubslicesPerSlice = NEO::getNumSubSlicesPerSlice(hardwareInfo);
    }

    pDeviceProperties->numSlices = hardwareInfo.gtSystemInfo.SliceCount;

    if (isImplicitScalingCapable()) {
        pDeviceProperties->numSlices *= neoDevice->getNumGenericSubDevices();
    }

    if ((NEO::debugManager.flags.UseCyclesPerSecondTimer.get() == 1) ||
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

    if (isSubdevice && this->neoDevice->getExecutionEnvironment()->getDeviceHierarchyMode() != NEO::DeviceHierarchyMode::flat) {
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
    if (NEO::debugManager.flags.OverrideDeviceName.get() != "unk") {
        name.assign(NEO::debugManager.flags.OverrideDeviceName.get().c_str());
    } else if (driverInfo) {
        name.assign(driverInfo->getDeviceName(name).c_str());
    }
    memcpy_s(pDeviceProperties->name, ZE_MAX_DEVICE_NAME, name.c_str(), name.length() + 1);

    ze_base_properties_t *extendedProperties = reinterpret_cast<ze_base_properties_t *>(pDeviceProperties->pNext);

    bool isDevicePropertiesStypeValid = (pDeviceProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES || pDeviceProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2) ? true : false;

    if (isDevicePropertiesStypeValid) {
        while (extendedProperties) {
            if (extendedProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES) {
                ze_device_luid_ext_properties_t *deviceLuidProperties = reinterpret_cast<ze_device_luid_ext_properties_t *>(extendedProperties);
                deviceLuidProperties->nodeMask = queryDeviceNodeMask();
                ze_result_t result = queryDeviceLuid(deviceLuidProperties);
                if (result != ZE_RESULT_SUCCESS) {
                    return result;
                }
            } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_EU_COUNT_EXT) {
                ze_eu_count_ext_t *zeEuCountDesc = reinterpret_cast<ze_eu_count_ext_t *>(extendedProperties);
                uint32_t numTotalEUs = hardwareInfo.gtSystemInfo.EUCount;

                if (isImplicitScalingCapable()) {
                    numTotalEUs *= neoDevice->getNumGenericSubDevices();
                }
                zeEuCountDesc->numTotalEUs = numTotalEUs;
            } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT) {
                ze_device_ip_version_ext_t *zeDeviceIpVersion = reinterpret_cast<ze_device_ip_version_ext_t *>(extendedProperties);
                NEO::Device *activeDevice = getActiveDevice();
                auto &compilerProductHelper = activeDevice->getCompilerProductHelper();
                zeDeviceIpVersion->ipVersion = hardwareInfo.ipVersionOverrideExposedToTheApplication.value;
                if (0 == zeDeviceIpVersion->ipVersion) {
                    zeDeviceIpVersion->ipVersion = compilerProductHelper.getHwIpVersion(hardwareInfo);
                }
            } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_PROPERTIES) {
                ze_event_query_kernel_timestamps_ext_properties_t *kernelTimestampExtProperties = reinterpret_cast<ze_event_query_kernel_timestamps_ext_properties_t *>(extendedProperties);
                kernelTimestampExtProperties->flags = ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_FLAG_KERNEL | ZE_EVENT_QUERY_KERNEL_TIMESTAMPS_EXT_FLAG_SYNCHRONIZED;
            } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXP_PROPERTIES) {
                ze_rtas_device_exp_properties_t *rtasProperties = reinterpret_cast<ze_rtas_device_exp_properties_t *>(extendedProperties);
                rtasProperties->flags = 0;
                rtasProperties->rtasFormat = l0GfxCoreHelper.getSupportedRTASFormatExp();
                rtasProperties->rtasBufferAlignment = 128;

                if (releaseHelper && releaseHelper->isRayTracingSupported()) {
                    auto driverHandle = this->getDriverHandle();
                    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driverHandle);

                    ze_result_t result = driverHandleImp->loadRTASLibrary();
                    if (result != ZE_RESULT_SUCCESS) {
                        rtasProperties->rtasFormat = ZE_RTAS_FORMAT_EXP_INVALID;
                    }
                }
            } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_RTAS_DEVICE_EXT_PROPERTIES) {
                ze_rtas_device_ext_properties_t *rtasProperties = reinterpret_cast<ze_rtas_device_ext_properties_t *>(extendedProperties);
                rtasProperties->flags = 0;
                rtasProperties->rtasFormat = l0GfxCoreHelper.getSupportedRTASFormatExt();
                rtasProperties->rtasBufferAlignment = 128;

                if (releaseHelper && releaseHelper->isRayTracingSupported()) {
                    auto driverHandle = this->getDriverHandle();
                    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driverHandle);

                    ze_result_t result = driverHandleImp->loadRTASLibrary();
                    if (result != ZE_RESULT_SUCCESS) {
                        rtasProperties->rtasFormat = ZE_RTAS_FORMAT_EXT_INVALID;
                    }
                }
            } else if (static_cast<uint32_t>(extendedProperties->stype) == ZE_INTEL_STRUCTURE_TYPE_DEVICE_COMMAND_LIST_WAIT_ON_MEMORY_DATA_SIZE_EXP_DESC) {
                auto cmdListWaitOnMemDataSize = reinterpret_cast<ze_intel_device_command_list_wait_on_memory_data_size_exp_desc_t *>(extendedProperties);
                cmdListWaitOnMemDataSize->cmdListWaitOnMemoryDataSizeInBytes = l0GfxCoreHelper.getCmdListWaitOnMemoryDataSize();
            } else if (static_cast<uint32_t>(extendedProperties->stype) == ZE_STRUCTURE_TYPE_INTEL_DEVICE_MEDIA_EXP_PROPERTIES) {
                auto deviceMediaProperties = reinterpret_cast<ze_intel_device_media_exp_properties_t *>(extendedProperties);
                deviceMediaProperties->numDecoderCores = 0;
                deviceMediaProperties->numEncoderCores = 0;
            } else if (extendedProperties->stype == ZE_INTEL_DEVICE_BLOCK_ARRAY_EXP_PROPERTIES) {
                ze_intel_device_block_array_exp_flags_t supportMatrix{0};
                supportMatrix |= getProductHelper().supports2DBlockStore() ? ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_STORE : 0;
                supportMatrix |= getProductHelper().supports2DBlockLoad() ? ZE_INTEL_DEVICE_EXP_FLAG_2D_BLOCK_LOAD : 0;
                auto blockTransposeProps = reinterpret_cast<ze_intel_device_block_array_exp_properties_t *>(extendedProperties);
                blockTransposeProps->flags = supportMatrix;
            } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXP_PROPERTIES) {
                ze_mutable_command_list_exp_properties_t *mclProperties = reinterpret_cast<ze_mutable_command_list_exp_properties_t *>(extendedProperties);
                mclProperties->mutableCommandListFlags = 0;
                mclProperties->mutableCommandFlags = getL0GfxCoreHelper().getCmdListUpdateCapabilities(this->getNEODevice()->getRootDeviceEnvironment());
            } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_RECORD_REPLAY_GRAPH_EXP_PROPERTIES) {
                auto recordReplayGraphProperties = reinterpret_cast<ze_record_replay_graph_exp_properties_t *>(extendedProperties);
                recordReplayGraphProperties->graphFlags = getL0GfxCoreHelper().getRecordReplayGraphCapabilities(this->getNEODevice()->getRootDeviceEnvironment());
            }
            getAdditionalExtProperties(extendedProperties);
            extendedProperties = static_cast<ze_base_properties_t *>(extendedProperties->pNext);
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getGlobalTimestamps(uint64_t *hostTimestamp, uint64_t *deviceTimestamp) {

    bool method = 0;
    if (NEO::debugManager.flags.EnableGlobalTimestampViaSubmission.get() != -1) {
        method = NEO::debugManager.flags.EnableGlobalTimestampViaSubmission.get();
    }

    if (method == 0) {
        auto ret = getGlobalTimestampsUsingOsInterface(hostTimestamp, deviceTimestamp);
        if (ret != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
            return ret;
        }
    }

    return getGlobalTimestampsUsingSubmission(hostTimestamp, deviceTimestamp);
}

ze_result_t DeviceImp::getGlobalTimestampsUsingSubmission(uint64_t *hostTimestamp, uint64_t *deviceTimestamp) {
    std::lock_guard<std::mutex> lock(globalTimestampMutex);

    if (this->globalTimestampAllocation == nullptr) {
        ze_command_queue_desc_t queueDescriptor = {};
        queueDescriptor.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        queueDescriptor.pNext = nullptr;
        queueDescriptor.flags = 0;
        queueDescriptor.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        queueDescriptor.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDescriptor.ordinal = 0;
        queueDescriptor.index = 0;
        this->createCommandListImmediate(&queueDescriptor, &this->globalTimestampCommandList);

        RootDeviceIndicesContainer rootDeviceIndices;
        rootDeviceIndices.pushUnique(this->getNEODevice()->getRootDeviceIndex());

        std::map<uint32_t, NEO::DeviceBitfield> deviceBitfields;
        deviceBitfields.insert({this->getNEODevice()->getRootDeviceIndex(), this->getNEODevice()->getDeviceBitfield()});

        NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory,
                                                                               sizeof(uint64_t),
                                                                               rootDeviceIndices,
                                                                               deviceBitfields);

        this->globalTimestampAllocation = this->getDriverHandle()->getSvmAllocsManager()->createHostUnifiedMemoryAllocation(sizeof(uint64_t),
                                                                                                                            unifiedMemoryProperties);
        if (this->globalTimestampAllocation == nullptr) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
    }

    // Get CPU time first here to be used for averaging later
    uint64_t hostTimestampForAvg = 0;
    bool cpuHostTimeRetVal = this->neoDevice->getOSTime()->getCpuTimeHost(&hostTimestampForAvg);

    auto ret = L0::CommandList::fromHandle(this->globalTimestampCommandList)->appendWriteGlobalTimestamp((uint64_t *)this->globalTimestampAllocation, nullptr, 0, nullptr);
    if (ret != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    // Copy over the device timestamp data
    *deviceTimestamp = *static_cast<uint64_t *>(this->globalTimestampAllocation);

    // get CPU time
    cpuHostTimeRetVal |= this->neoDevice->getOSTime()->getCpuTimeHost(hostTimestamp);
    if (cpuHostTimeRetVal) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    // Average the host timestamp
    *hostTimestamp = (*hostTimestamp + hostTimestampForAvg) / 2;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getGlobalTimestampsUsingOsInterface(uint64_t *hostTimestamp, uint64_t *deviceTimestamp) {

    NEO::TimeStampData queueTimeStamp;
    NEO::TimeQueryStatus retVal = this->neoDevice->getOSTime()->getGpuCpuTime(&queueTimeStamp, true);
    if (retVal == NEO::TimeQueryStatus::deviceLost) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    } else if (retVal == NEO::TimeQueryStatus::unsupportedFeature) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    *deviceTimestamp = queueTimeStamp.gpuTimeStamp;
    *hostTimestamp = queueTimeStamp.cpuTimeinNS;

    if (NEO::debugManager.flags.PrintGlobalTimestampInNs.get()) {
        const auto &capabilityTable = this->neoDevice->getHardwareInfo().capabilityTable;
        const auto validBits = std::min(capabilityTable.timestampValidBits, capabilityTable.kernelTimestampValidBits);
        uint64_t kernelTimestampMaxValueInCycles = std::numeric_limits<uint64_t>::max();
        if (validBits < 64u) {
            kernelTimestampMaxValueInCycles = (1ull << validBits) - 1;
        }
        const uint64_t deviceTsinNs = (*deviceTimestamp & kernelTimestampMaxValueInCycles) * this->neoDevice->getDeviceInfo().outProfilingTimerResolution;
        NEO::printDebugString(true, stdout,
                              "Host timestamp in ns : %llu | Device timestamp in ns : %llu\n", *hostTimestamp, deviceTsinNs);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getSubDevices(uint32_t *pCount, ze_device_handle_t *phSubdevices) {
    // Given FLAT device Hierarchy mode, then a count of 0 is returned since no traversal is allowed.
    if (this->neoDevice->getExecutionEnvironment()->getDeviceHierarchyMode() == NEO::DeviceHierarchyMode::flat ||
        this->neoDevice->getRootDeviceEnvironment().isExposeSingleDeviceMode()) {
        *pCount = 0;
        return ZE_RESULT_SUCCESS;
    }
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
    uint32_t subDeviceCount = std::max(this->numSubDevices, 1u);
    pCacheProperties[0].cacheSize = hardwareInfo.gtSystemInfo.L3CacheSizeInKb * subDeviceCount * MemoryConstants::kiloByte;
    pCacheProperties[0].flags = 0;

    if (pCacheProperties->pNext) {
        auto extendedProperties = reinterpret_cast<ze_device_cache_properties_t *>(pCacheProperties->pNext);
        if (extendedProperties->stype == ZE_STRUCTURE_TYPE_CACHE_RESERVATION_EXT_DESC) {
            constexpr size_t cacheLevel{3U};
            auto cacheReservationProperties = reinterpret_cast<ze_cache_reservation_ext_desc_t *>(extendedProperties);
            cacheReservationProperties->maxCacheReservationSize = cacheReservation->getMaxCacheReservationSize(cacheLevel);
        } else if (extendedProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_CACHELINE_SIZE_EXT) {
            auto cacheLineSizeProperties = reinterpret_cast<ze_device_cache_line_size_ext_t *>(extendedProperties);
            auto &productHelper = neoDevice->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
            cacheLineSizeProperties->cacheLineSize = productHelper.getCacheLineSize();
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::reserveCache(size_t cacheLevel, size_t cacheReservationSize) {
    auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();
    if (!osInterface || osInterface->getDriverModel()->getDriverModelType() != NEO::DriverModelType::drm) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (cacheLevel == 0U) {
        cacheLevel = 3U;
    }

    if (cacheLevel != 3U) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (cacheReservation->getMaxCacheReservationSize(cacheLevel) == 0U) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto result = cacheReservation->reserveCache(cacheLevel, cacheReservationSize);
    if (result == false) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::setCacheAdvice(void *ptr, size_t regionSize, ze_cache_ext_region_t cacheRegion) {
    auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();
    if (!osInterface || osInterface->getDriverModel()->getDriverModelType() != NEO::DriverModelType::drm) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (cacheRegion == ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_DEFAULT) {
        cacheRegion = ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_NON_RESERVED;
    }

    const auto cacheLevel{[cacheRegion]() {
        switch (cacheRegion) {
        case ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_RESERVED:
        case ze_cache_ext_region_t::ZE_CACHE_EXT_REGION_NON_RESERVED:
            return 3U;
        default:
            UNRECOVERABLE_IF(true);
        }
    }()};

    if (cacheReservation->getMaxCacheReservationSize(cacheLevel) == 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
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

        ze_base_properties_t *extendedProperties = reinterpret_cast<ze_base_properties_t *>(pDeviceImageProperties->pNext);
        while (extendedProperties) {
            if (extendedProperties->stype == ZE_STRUCTURE_TYPE_PITCHED_ALLOC_DEVICE_EXP_PROPERTIES) {
                ze_device_pitched_alloc_exp_properties_t *properties = reinterpret_cast<ze_device_pitched_alloc_exp_properties_t *>(extendedProperties);
                properties->maxImageLinearHeight = deviceInfo.image2DMaxHeight;
                properties->maxImageLinearWidth = deviceInfo.image2DMaxWidth;
            }
            extendedProperties = reinterpret_cast<ze_base_properties_t *>(extendedProperties->pNext);
        }
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
    bool isDebugAttachAvailable = getOsInterface() ? getOsInterface()->isDebugAttachAvailable() : false;
    pDebugProperties->flags = 0;

    auto &hwInfo = neoDevice->getHardwareInfo();
    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        isDebugAttachAvailable = false;
    }
    if (isDebugAttachAvailable) {
        auto &stateSaveAreaHeader = NEO::SipKernel::getDebugSipKernel(*this->getNEODevice()).getStateSaveAreaHeader();

        if (stateSaveAreaHeader.size() == 0) {
            PRINT_DEBUGGER_INFO_LOG("Context state save area header missing", "");
        } else {
            bool tileAttach = NEO::debugManager.flags.ExperimentalEnableTileAttach.get();

            if ((isSubdevice && tileAttach) || !isSubdevice) {
                pDebugProperties->flags = zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH;
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::systemBarrier() { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }

ze_result_t DeviceImp::activateMetricGroupsDeferred(uint32_t count,
                                                    zet_metric_group_handle_t *phMetricGroups) {

    if (!metricContext->areMetricGroupsFromSameDeviceHierarchy(count, phMetricGroups)) {
        METRICS_LOG_ERR("%s", "Mix of root device and sub-device metric group handle is not allowed");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto status = ZE_RESULT_SUCCESS;
    status = metricContext->activateMetricGroupsPreferDeferred(count, phMetricGroups);
    return status;
}

BuiltinFunctionsLib *DeviceImp::getBuiltinFunctionsLib() { return builtins.get(); }

uint32_t DeviceImp::getMOCS(bool l3enabled, bool l1enabled) {
    return getGfxCoreHelper().getMocsIndex(*getNEODevice()->getGmmHelper(), l3enabled, l1enabled) << 1;
}

const NEO::GfxCoreHelper &DeviceImp::getGfxCoreHelper() {
    return this->neoDevice->getGfxCoreHelper();
}

const L0GfxCoreHelper &DeviceImp::getL0GfxCoreHelper() {
    return this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
}

const NEO::ProductHelper &DeviceImp::getProductHelper() {
    return this->neoDevice->getProductHelper();
}

const NEO::CompilerProductHelper &DeviceImp::getCompilerProductHelper() {
    return this->neoDevice->getCompilerProductHelper();
}

NEO::OSInterface *DeviceImp::getOsInterface() { return neoDevice->getRootDeviceEnvironment().osInterface.get(); }

uint32_t DeviceImp::getPlatformInfo() const {
    const auto &hardwareInfo = neoDevice->getHardwareInfo();
    return hardwareInfo.platform.eRenderCoreFamily;
}

MetricDeviceContext &DeviceImp::getMetricDeviceContext() { return *metricContext; }

void DeviceImp::activateMetricGroups() {
    if (metricContext != nullptr) {
        if (metricContext->isImplicitScalingCapable()) {
            for (uint32_t i = 0; i < numSubDevices; i++) {
                subDevices[i]->getMetricDeviceContext().activateMetricGroups();
            }
        } else {
            metricContext->activateMetricGroups();
        }
    }
}
uint32_t DeviceImp::getMaxNumHwThreads() const { return maxNumHwThreads; }

const NEO::HardwareInfo &DeviceImp::getHwInfo() const { return neoDevice->getHardwareInfo(); }

// Use this method to reinitialize L0::Device *device, that was created during zeInit, with the help of Device::create
Device *Device::deviceReinit(DriverHandle *driverHandle, L0::Device *device, std::unique_ptr<NEO::Device> &neoDevice, ze_result_t *returnValue) {
    auto pNeoDevice = neoDevice.release();

    return Device::create(driverHandle, pNeoDevice, false, returnValue, device);
}

Device *Device::create(DriverHandle *driverHandle, NEO::Device *neoDevice, bool isSubDevice, ze_result_t *returnValue) {
    return Device::create(driverHandle, neoDevice, isSubDevice, returnValue, nullptr);
}

Device *Device::create(DriverHandle *driverHandle, NEO::Device *neoDevice, bool isSubDevice, ze_result_t *returnValue, L0::Device *deviceL0) {
    L0::DeviceImp *device = nullptr;
    if (deviceL0 == nullptr) {
        device = new DeviceImp;
    } else {
        device = static_cast<L0::DeviceImp *>(deviceL0);
    }

    UNRECOVERABLE_IF(device == nullptr);

    device->isSubdevice = isSubDevice;
    device->setDriverHandle(driverHandle);
    neoDevice->setSpecializedDevice(device);

    device->neoDevice = neoDevice;
    neoDevice->incRefInternal();

    auto &hwInfo = neoDevice->getHardwareInfo();
    auto &rootDeviceEnvironment = neoDevice->getRootDeviceEnvironment();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();

    device->allocationsForReuse = std::make_unique<NEO::AllocationsList>();
    bool platformImplicitScaling = gfxCoreHelper.platformSupportsImplicitScaling(rootDeviceEnvironment);
    device->implicitScalingCapable = NEO::ImplicitScalingHelper::isImplicitScalingEnabled(neoDevice->getDeviceBitfield(), platformImplicitScaling);
    device->metricContext = MetricDeviceContext::create(*device);
    device->builtins = BuiltinFunctionsLib::create(
        device, neoDevice->getBuiltIns());
    device->cacheReservation = CacheReservation::create(*device);
    device->maxNumHwThreads = NEO::GfxCoreHelper::getMaxThreadsForVfe(hwInfo);

    auto osInterface = rootDeviceEnvironment.osInterface.get();
    device->driverInfo.reset(NEO::DriverInfo::create(&hwInfo, osInterface));

    std::vector<char> stateSaveAreaHeader;

    if (neoDevice->getDebugger()) {
        if (neoDevice->getCompilerInterface()) {
            if (rootDeviceEnvironment.executionEnvironment.getDebuggingMode() == NEO::DebuggingMode::offline) {
                if (NEO::SipKernel::getSipKernel(*neoDevice, nullptr).getCtxOffset() == 0) {
                    NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                          "Invalid SIP binary.\n");
                }
            }
            auto &sipKernel = NEO::SipKernel::getSipKernel(*neoDevice, nullptr);
            stateSaveAreaHeader = sipKernel.getStateSaveAreaHeader();
            if (sipKernel.getStateSaveAreaSize(neoDevice) == 0) {
                *returnValue = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
            }
        } else {
            *returnValue = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }
    }

    NEO::GraphicsAllocation *debugSurface = device->getDebugSurface();
    if (debugSurface && stateSaveAreaHeader.size() > 0) {
        const auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();
        NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *debugSurface),
                                                              *neoDevice, debugSurface, 0, stateSaveAreaHeader.data(),
                                                              stateSaveAreaHeader.size());
        if (neoDevice->getBindlessHeapsHelper()) {
            auto &gfxCoreHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[debugSurface->getRootDeviceIndex()]->getHelper<NEO::GfxCoreHelper>();
            auto ssh = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getCpuBase();
            NEO::EncodeSurfaceStateArgs args;
            args.outMemory = ssh;
            args.graphicsAddress = device->getDebugSurface()->getGpuAddress();
            args.size = device->getDebugSurface()->getUnderlyingBufferSize();
            args.mocs = device->getMOCS(false, false);
            args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
            args.allocation = device->getDebugSurface();
            args.gmmHelper = neoDevice->getGmmHelper();
            args.areMultipleSubDevicesInContext = neoDevice->getNumGenericSubDevices() > 1;
            args.isDebuggerActive = true;
            gfxCoreHelper.encodeBufferSurfaceState(args);
        }
    }

    for (auto &neoSubDevice : neoDevice->getSubDevices()) {
        if (!neoSubDevice) {
            continue;
        }

        ze_device_handle_t subDevice = Device::create(driverHandle,
                                                      neoSubDevice,
                                                      true, returnValue, nullptr);
        if (subDevice == nullptr) {
            return nullptr;
        }
        static_cast<DeviceImp *>(subDevice)->setDebugSurface(debugSurface);
        static_cast<DeviceImp *>(subDevice)->rootDevice = device;
        device->subDevices.push_back(static_cast<Device *>(subDevice));
    }
    device->numSubDevices = static_cast<uint32_t>(device->subDevices.size());

    auto supportDualStorageSharedMemory = neoDevice->getMemoryManager()->isLocalMemorySupported(device->neoDevice->getRootDeviceIndex());
    if (NEO::debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get() != -1) {
        supportDualStorageSharedMemory = NEO::debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.get();
    }

    if (supportDualStorageSharedMemory) {
        ze_command_queue_desc_t cmdQueueDesc = {};
        cmdQueueDesc.ordinal = 0;
        cmdQueueDesc.index = 0;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        ze_result_t resultValue = ZE_RESULT_SUCCESS;

        Device *pageFaultDevice = device;
        if (device->implicitScalingCapable) {
            pageFaultDevice = device->subDevices[0];
        }

        device->pageFaultCommandList =
            CommandList::createImmediate(
                device->neoDevice->getHardwareInfo().platform.eProductFamily, pageFaultDevice, &cmdQueueDesc, true, NEO::EngineGroupType::copy, resultValue);
    }

    if (osInterface) {
        auto pciSpeedInfo = osInterface->getDriverModel()->getPciSpeedInfo();
        device->pciMaxSpeed.genVersion = pciSpeedInfo.genVersion;
        device->pciMaxSpeed.maxBandwidth = pciSpeedInfo.maxBandwidth;
        device->pciMaxSpeed.width = pciSpeedInfo.width;
    }

    bool isHwMode = device->getNEODevice()->getAllEngines().size() ? device->getNEODevice()->getAllEngines()[0].commandStreamReceiver->getType() == NEO::CommandStreamReceiverType::hardware : device->getNEODevice()->getRootDevice()->getAllEngines()[0].commandStreamReceiver->getType() == NEO::CommandStreamReceiverType::hardware;
    if (isHwMode) {
        device->createSysmanHandle(isSubDevice);
    }
    device->resourcesReleased = false;

    device->populateSubDeviceCopyEngineGroups();
    auto &productHelper = device->getProductHelper();
    device->calculationForDisablingEuFusionWithDpasNeeded = productHelper.isCalculationForDisablingEuFusionWithDpasNeeded(hwInfo);

    auto numPriorities = static_cast<int32_t>(device->getNEODevice()->getGfxCoreHelper().getQueuePriorityLevels());

    device->queuePriorityHigh = -(numPriorities + 1) / 2 + 1;
    device->queuePriorityLow = (numPriorities) / 2;

    return device;
}

void DeviceImp::releaseResources() {
    if (resourcesReleased) {
        return;
    }

    UNRECOVERABLE_IF(neoDevice == nullptr);

    neoDevice->stopDirectSubmissionAndWaitForCompletion();

    if (this->globalTimestampAllocation) {
        driverHandle->getSvmAllocsManager()->freeSVMAlloc(this->globalTimestampAllocation);
    }
    if (this->globalTimestampCommandList) {
        L0::CommandList::fromHandle(this->globalTimestampCommandList)->destroy();
    }

    getNEODevice()->getMemoryManager()->freeGraphicsMemory(syncDispatchTokenAllocation);

    getNEODevice()->cleanupUsmAllocationPool();

    this->bcsSplit->releaseResources();

    if (neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.get()) {
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.reset(nullptr);
    }
    if (neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->assertHandler.get()) {
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->assertHandler.reset(nullptr);
    }

    // close connection and async threads in debug session before releasing device resources
    if (debugSession.get()) {
        debugSession->closeConnection();
    }

    if (this->pageFaultCommandList) {
        this->pageFaultCommandList->destroy();
        this->pageFaultCommandList = nullptr;
    }

    for (uint32_t i = 0; i < this->numSubDevices; i++) {
        delete this->subDevices[i];
    }
    this->subDevices.clear();
    this->numSubDevices = 0;

    metricContext.reset();
    builtins.reset();
    cacheReservation.reset();

    peerCounterAllocations.freeAllocations(*neoDevice->getMemoryManager());

    deviceInOrderCounterAllocator.reset();
    hostInOrderCounterAllocator.reset();
    inOrderTimestampAllocator.reset();
    fillPatternAllocator.reset();

    if (allocationsForReuse.get()) {
        allocationsForReuse->freeAllGraphicsAllocations(neoDevice);
        allocationsForReuse.reset();
    }
    neoDevice->pollForCompletion();

    neoDevice->decRefInternal();
    neoDevice = nullptr;

    resourcesReleased = true;
}

DeviceImp::~DeviceImp() {
    releaseResources();

    if (!isSubdevice) {
        if (pSysmanDevice != nullptr) {
            delete pSysmanDevice;
            pSysmanDevice = nullptr;
        }
    } else {
        debugSession.release();
    }
}

NEO::PreemptionMode DeviceImp::getDevicePreemptionMode() const {
    return neoDevice->getPreemptionMode();
}

const NEO::DeviceInfo &DeviceImp::getDeviceInfo() const {
    return neoDevice->getDeviceInfo();
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
        for (auto &allocData : allocDataArray) {
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
        {getRootDeviceIndex(), false, size, NEO::AllocationType::bufferHostMemory, false, neoDevice->getDeviceBitfield()},
        buffer);

    if (allocation == nullptr) {
        return allocation;
    }

    NEO::SvmAllocationData allocData(getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(allocation);
    allocData.cpuAllocation = nullptr;
    allocData.size = size;
    allocData.memoryType = InternalMemoryType::notSpecified;
    allocData.device = nullptr;
    driverHandle->getSvmAllocsManager()->insertSVMAlloc(allocData);

    return allocation;
}

NEO::GraphicsAllocation *DeviceImp::allocateMemoryFromHostPtr(const void *buffer, size_t size, bool hostCopyAllowed) {
    NEO::AllocationProperties properties = {getRootDeviceIndex(), false, size,
                                            NEO::AllocationType::externalHostPtr,
                                            false, neoDevice->getDeviceBitfield()};
    // L3 must be flushed only if host memory is a transfer destination.
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = !hostCopyAllowed;
    auto allocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties,
                                                                                          buffer);
    if (allocation == nullptr && hostCopyAllowed) {
        allocation = neoDevice->getMemoryManager()->allocateInternalGraphicsMemoryWithHostCopy(neoDevice->getRootDeviceIndex(),
                                                                                               neoDevice->getDeviceBitfield(),
                                                                                               buffer,
                                                                                               size);
    }

    return allocation;
}

NEO::GraphicsAllocation *DeviceImp::obtainReusableAllocation(size_t requiredSize, NEO::AllocationType type) {
    auto alloc = allocationsForReuse->detachAllocation(requiredSize, nullptr, nullptr, type);
    if (alloc == nullptr)
        return nullptr;
    else
        return alloc.release();
}

void DeviceImp::storeReusableAllocation(NEO::GraphicsAllocation &alloc) {
    allocationsForReuse->pushFrontOne(alloc);
}

bool DeviceImp::isQueueGroupOrdinalValid(uint32_t ordinal) {
    auto &engineGroups = getActiveDevice()->getRegularEngineGroups();
    uint32_t numEngineGroups = static_cast<uint32_t>(engineGroups.size());
    uint32_t numSubDeviceCopyEngineGroups = static_cast<uint32_t>(this->subDeviceCopyEngineGroups.size());

    uint32_t totalEngineGroups = numEngineGroups + numSubDeviceCopyEngineGroups;
    if (ordinal >= totalEngineGroups) {
        return false;
    }

    return true;
}

ze_result_t DeviceImp::getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index, ze_command_queue_priority_t priority, std::optional<int> priorityLevel, bool allocateInterrupt) {
    auto &engineGroups = getActiveDevice()->getRegularEngineGroups();
    uint32_t numEngineGroups = static_cast<uint32_t>(engineGroups.size());

    if (!this->isQueueGroupOrdinalValid(ordinal)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if ((NEO::debugManager.flags.ForceBcsEngineIndex.get() != -1) && NEO::EngineHelper::isCopyOnlyEngineType(getEngineGroupTypeForOrdinal(ordinal))) {
        index = static_cast<uint32_t>(NEO::debugManager.flags.ForceBcsEngineIndex.get());

        constexpr uint32_t invalidOrdinal = std::numeric_limits<uint32_t>::max();

        auto findOrdinal = [&](NEO::EngineGroupType type) -> uint32_t {
            bool subDeviceCopyEngines = (ordinal >= numEngineGroups);
            auto &lookupGroup = subDeviceCopyEngines ? this->subDeviceCopyEngineGroups : engineGroups;

            uint32_t ordinal = invalidOrdinal;

            for (uint32_t i = 0; i < lookupGroup.size(); i++) {
                if (lookupGroup[i].engineGroupType == type) {
                    ordinal = (i + (subDeviceCopyEngines ? numEngineGroups : 0));
                    break;
                }
            }

            return ordinal;
        };

        if (index == 0 && getEngineGroupTypeForOrdinal(ordinal) != NEO::EngineGroupType::copy) {
            ordinal = findOrdinal(NEO::EngineGroupType::copy);
        } else if (index > 0) {
            if (getEngineGroupTypeForOrdinal(ordinal) != NEO::EngineGroupType::linkedCopy) {
                ordinal = findOrdinal(NEO::EngineGroupType::linkedCopy);
            }
            index--;
        }

        if (ordinal == invalidOrdinal) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    const NEO::GfxCoreHelper &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    bool secondaryContextsEnabled = gfxCoreHelper.areSecondaryContextsSupported();

    auto contextPriority = NEO::EngineUsage::regular;
    auto engineGroupType = getEngineGroupTypeForOrdinal(ordinal);
    bool copyOnly = NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType);

    if (priorityLevel.has_value()) {
        if (priorityLevel.value() < 0) {
            priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
        } else if (priorityLevel.value() == this->queuePriorityLow) {
            DEBUG_BREAK_IF(this->queuePriorityLow == 0);
            priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
        } else if (priorityLevel.value() > 0) {
            priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        }
    }

    if (priority == ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH) {
        contextPriority = NEO::EngineUsage::highPriority;
    } else if (isSuitableForLowPriority(priority, copyOnly)) {
        contextPriority = NEO::EngineUsage::lowPriority;
    }

    auto selectedDevice = this;
    if (ordinal < numEngineGroups) {

        if (contextPriority == NEO::EngineUsage::lowPriority) {
            auto result = getCsrForLowPriority(csr, copyOnly);
            DEBUG_BREAK_IF(result != ZE_RESULT_SUCCESS);
            return result;
        }

        auto &engines = engineGroups[ordinal].engines;
        if (index >= engines.size()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *csr = engines[index].commandStreamReceiver;

    } else {
        auto subDeviceOrdinal = ordinal - numEngineGroups;
        if (index >= this->subDeviceCopyEngineGroups[subDeviceOrdinal].engines.size()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *csr = this->subDeviceCopyEngineGroups[subDeviceOrdinal].engines[index].commandStreamReceiver;
        selectedDevice = static_cast<DeviceImp *>(this->subDevices[subDeviceOrdinal]);
    }

    if (copyOnly) {

        if (contextPriority == NEO::EngineUsage::highPriority) {
            selectedDevice->getCsrForHighPriority(csr, copyOnly);
        } else if (contextPriority == NEO::EngineUsage::lowPriority) {
            if (selectedDevice->getCsrForLowPriority(csr, copyOnly) == ZE_RESULT_SUCCESS) {
                return ZE_RESULT_SUCCESS;
            }
        }
    }

    auto &osContext = (*csr)->getOsContext();

    if (secondaryContextsEnabled) {
        selectedDevice->tryAssignSecondaryContext(osContext.getEngineType(), contextPriority, priorityLevel, csr, allocateInterrupt);
    }

    return ZE_RESULT_SUCCESS;
}

bool DeviceImp::tryAssignSecondaryContext(aub_stream::EngineType engineType, NEO::EngineUsage engineUsage, std::optional<int> priorityLevel, NEO::CommandStreamReceiver **csr, bool allocateInterrupt) {
    if (neoDevice->isSecondaryContextEngineType(engineType)) {
        NEO::EngineTypeUsage engineTypeUsage;
        engineTypeUsage.first = engineType;
        engineTypeUsage.second = engineUsage;
        auto engine = neoDevice->getSecondaryEngineCsr(engineTypeUsage, priorityLevel, allocateInterrupt);
        if (engine) {
            *csr = engine->commandStreamReceiver;
            return true;
        }
    }

    return false;
}

ze_result_t DeviceImp::getCsrForLowPriority(NEO::CommandStreamReceiver **csr, bool copyOnly) {
    for (auto &it : getActiveDevice()->getAllEngines()) {
        bool engineTypeMatch = NEO::EngineHelpers::isBcs(it.osContext->getEngineType()) == copyOnly;
        if (it.osContext->isLowPriority() && engineTypeMatch) {
            *csr = it.commandStreamReceiver;
            return ZE_RESULT_SUCCESS;
        }
    }

    // if the code falls through, we have no low priority context created by neoDevice.
    return ZE_RESULT_ERROR_UNKNOWN;
}
ze_result_t DeviceImp::getCsrForHighPriority(NEO::CommandStreamReceiver **csr, bool copyOnly) {

    if (copyOnly) {
        auto engine = getActiveDevice()->getHpCopyEngine();
        if (engine) {
            *csr = engine->commandStreamReceiver;
            return ZE_RESULT_SUCCESS;
        }
    }

    // if the code falls through, we have no high priority context created by neoDevice.
    return ZE_RESULT_ERROR_UNKNOWN;
}

bool DeviceImp::isSuitableForLowPriority(ze_command_queue_priority_t priority, bool copyOnly) {
    bool engineSuitable = copyOnly ? getGfxCoreHelper().areSecondaryContextsSupported() : !this->implicitScalingCapable;

    return (priority == ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW && engineSuitable);
}

DebugSession *DeviceImp::getDebugSession(const zet_debug_config_t &config) {
    return debugSession.get();
}

DebugSession *DeviceImp::createDebugSession(const zet_debug_config_t &config, ze_result_t &result, bool isRootAttach) {
    if (!this->isSubdevice) {
        if (debugSession.get() == nullptr) {
            auto session = DebugSession::create(config, this, result, isRootAttach);
            debugSession.reset(session);
        } else {
            result = ZE_RESULT_SUCCESS;
        }
    } else if (NEO::debugManager.flags.ExperimentalEnableTileAttach.get()) {
        result = ZE_RESULT_SUCCESS;
        auto rootL0Device = getNEODevice()->getRootDevice()->getSpecializedDevice<DeviceImp>();
        auto session = rootL0Device->getDebugSession(config);
        if (!session) {
            session = rootL0Device->createDebugSession(config, result, isRootAttach);
        }

        if (result == ZE_RESULT_SUCCESS) {
            debugSession.reset(session->attachTileDebugSession(this));
            result = debugSession ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
    } else {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    return debugSession.get();
}

void DeviceImp::removeDebugSession() {
    debugSession.release();
}
void DeviceImp::setDebugSession(DebugSession *session) {
    debugSession.reset(session);
}
bool DeviceImp::toPhysicalSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t &deviceIndex) {
    auto hwInfo = neoDevice->getRootDeviceEnvironment().getHardwareInfo();
    uint32_t subDeviceCount = NEO::GfxCoreHelper::getSubDevicesCount(hwInfo);
    auto deviceBitfield = neoDevice->getDeviceBitfield();

    if (topologyMap.size() == subDeviceCount && !isSubdevice) {
        uint32_t sliceId = slice;
        for (uint32_t i = 0; i < topologyMap.size(); i++) {
            if (deviceBitfield.test(i)) {
                if (sliceId < topologyMap.at(i).sliceIndices.size()) {
                    slice = topologyMap.at(i).sliceIndices[sliceId];

                    if (topologyMap.at(i).sliceIndices.size() == 1) {
                        uint32_t subsliceId = subslice;
                        subslice = topologyMap.at(i).subsliceIndices[subsliceId];
                    }
                    deviceIndex = i;
                    return true;
                }
                sliceId = sliceId - static_cast<uint32_t>(topologyMap.at(i).sliceIndices.size());
            }
        }
    } else if (isSubdevice) {
        UNRECOVERABLE_IF(!deviceBitfield.any());
        uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));

        if (topologyMap.find(subDeviceIndex) != topologyMap.end()) {
            if (slice < topologyMap.at(subDeviceIndex).sliceIndices.size()) {
                deviceIndex = subDeviceIndex;
                slice = topologyMap.at(subDeviceIndex).sliceIndices[slice];

                if (topologyMap.at(subDeviceIndex).sliceIndices.size() == 1) {
                    uint32_t subsliceId = subslice;
                    subslice = topologyMap.at(subDeviceIndex).subsliceIndices[subsliceId];
                }
                return true;
            }
        }
    }

    return false;
}

bool DeviceImp::toApiSliceId(const NEO::TopologyMap &topologyMap, uint32_t &slice, uint32_t &subslice, uint32_t deviceIndex) {
    auto deviceBitfield = neoDevice->getDeviceBitfield();

    if (isSubdevice) {
        UNRECOVERABLE_IF(!deviceBitfield.any());
        deviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    }

    if (topologyMap.find(deviceIndex) != topologyMap.end()) {
        uint32_t apiSliceId = 0;
        if (!isSubdevice) {
            for (uint32_t devId = 0; devId < deviceIndex; devId++) {
                if (deviceBitfield.test(devId)) {
                    apiSliceId += static_cast<uint32_t>(topologyMap.at(devId).sliceIndices.size());
                }
            }
        }

        for (uint32_t i = 0; i < topologyMap.at(deviceIndex).sliceIndices.size(); i++) {
            if (static_cast<uint32_t>(topologyMap.at(deviceIndex).sliceIndices[i]) == slice) {
                apiSliceId += i;
                slice = apiSliceId;
                if (topologyMap.at(deviceIndex).sliceIndices.size() == 1) {
                    for (uint32_t subsliceApiId = 0; subsliceApiId < topologyMap.at(deviceIndex).subsliceIndices.size(); subsliceApiId++) {
                        if (static_cast<uint32_t>(topologyMap.at(deviceIndex).subsliceIndices[subsliceApiId]) == subslice) {
                            subslice = subsliceApiId;
                        }
                    }
                }
                return true;
            }
        }
    }

    return false;
}

NEO::Device *DeviceImp::getActiveDevice() const {
    if (neoDevice->getNumGenericSubDevices() > 0u) {
        if (isImplicitScalingCapable()) {
            return this->neoDevice;
        }
        for (auto subDevice : this->neoDevice->getSubDevices()) {
            if (subDevice) {
                return subDevice;
            }
        }
    }
    return this->neoDevice;
}

uint32_t DeviceImp::getPhysicalSubDeviceId() {
    if (!neoDevice->isSubDevice()) {
        uint32_t deviceBitField = static_cast<uint32_t>(neoDevice->getDeviceBitfield().to_ulong());
        if (neoDevice->getDeviceBitfield().count() > 1) {
            // Clear all set bits other than the right most bit
            deviceBitField &= ~deviceBitField + 1;
        }
        return Math::log2(deviceBitField);
    }
    return static_cast<NEO::SubDevice *>(neoDevice)->getSubDeviceIndex();
}

NEO::EngineGroupType DeviceImp::getEngineGroupTypeForOrdinal(uint32_t ordinal) const {
    auto &engineGroups = getActiveDevice()->getRegularEngineGroups();
    uint32_t numEngineGroups = static_cast<uint32_t>(engineGroups.size());

    NEO::EngineGroupType engineGroupType{};
    if (ordinal < numEngineGroups) {
        engineGroupType = engineGroups[ordinal].engineGroupType;
    } else {
        engineGroupType = this->subDeviceCopyEngineGroups[ordinal - numEngineGroups].engineGroupType;
    }
    return engineGroupType;
}

ze_result_t DeviceImp::getFabricVertex(ze_fabric_vertex_handle_t *phVertex) {
    auto driverHandle = this->getDriverHandle();
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(driverHandle);
    if (driverHandleImp->fabricVertices.empty()) {
        driverHandleImp->initializeVertexes();
    }

    if (fabricVertex == nullptr) {
        return ZE_RESULT_EXP_ERROR_DEVICE_IS_NOT_VERTEX;
    }

    *phVertex = fabricVertex->toHandle();
    return ZE_RESULT_SUCCESS;
}

uint32_t DeviceImp::getEventMaxPacketCount() const {
    auto &l0GfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t basePackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(this->neoDevice->getRootDeviceEnvironment());
    if (this->isImplicitScalingCapable()) {
        basePackets *= static_cast<uint32_t>(neoDevice->getDeviceBitfield().count());
    }
    return basePackets;
}

uint32_t DeviceImp::getEventMaxKernelCount() const {
    const auto &hardwareInfo = this->getHwInfo();
    auto &l0GfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    return l0GfxCoreHelper.getEventMaxKernelCount(hardwareInfo);
}

ze_result_t DeviceImp::synchronize() {

    auto waitForCsr = [](NEO::CommandStreamReceiver *csr) -> ze_result_t {
        if (csr->isInitialized()) {
            auto lock = csr->obtainUniqueOwnership();
            auto taskCountToWait = csr->peekTaskCount();
            auto flushStampToWait = csr->obtainCurrentFlushStamp();
            lock.unlock();
            auto waitStatus = csr->waitForTaskCountWithKmdNotifyFallback(
                taskCountToWait,
                flushStampToWait,
                false,
                NEO::QueueThrottle::MEDIUM);
            if (waitStatus == NEO::WaitStatus::gpuHang) {
                return ZE_RESULT_ERROR_DEVICE_LOST;
            }
        }
        return ZE_RESULT_SUCCESS;
    };

    for (auto &engine : neoDevice->getAllEngines()) {
        auto ret = waitForCsr(engine.commandStreamReceiver);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
    }
    for (auto &secondaryCsr : neoDevice->getSecondaryCsrs()) {
        auto ret = waitForCsr(secondaryCsr.get());
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
    }

    return ZE_RESULT_SUCCESS;
}

DeviceImp::CmdListCreateFunPtrT DeviceImp::getCmdListCreateFunc(const ze_base_desc_t *desc) {
    if (desc) {
        if (desc->stype == ZE_STRUCTURE_TYPE_MUTABLE_COMMAND_LIST_EXP_DESC) {
            return &MCL::MutableCommandList::create;
        }
    }

    return nullptr;
}

} // namespace L0
