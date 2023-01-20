/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp.h"

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
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/source/utilities/debug_settings_reader_creator.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cache/cache_reservation.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_build_log.h"
#include "level_zero/core/source/printf_handler/printf_handler.h"
#include "level_zero/core/source/sampler/sampler.h"
#include "level_zero/tools/source/debug/debug_session.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/sysman/sysman.h"

namespace NEO {
bool releaseFP64Override();
} // namespace NEO

namespace L0 {

DeviceImp::DeviceImp() : bcsSplit(*this){};

DriverHandle *DeviceImp::getDriverHandle() {
    return this->driverHandle;
}

void DeviceImp::setDriverHandle(DriverHandle *driverHandle) {
    this->driverHandle = driverHandle;
}

ze_result_t DeviceImp::submitCopyForP2P(ze_device_handle_t hPeerDevice, ze_bool_t *value) {
    DeviceImp *pPeerDevice = static_cast<DeviceImp *>(Device::fromHandle(hPeerDevice));
    uint32_t peerRootDeviceIndex = pPeerDevice->getNEODevice()->getRootDeviceIndex();

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

    this->createCommandList(&listDescriptor, &commandList);
    this->createCommandQueue(&queueDescriptor, &commandQueue);

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
    contextImp->allocDeviceMem(hPeerDevice, &peerDeviceDesc, 8, 1, &peerMemory);

    auto ret = L0::CommandList::fromHandle(commandList)->appendMemoryCopy(peerMemory, memory, 8, nullptr, 0, nullptr);
    L0::CommandList::fromHandle(commandList)->close();

    if (ret == ZE_RESULT_SUCCESS) {
        ret = L0::CommandQueue::fromHandle(commandQueue)->executeCommandLists(1, &commandList, nullptr, true);
        if (ret == ZE_RESULT_SUCCESS) {
            this->crossAccessEnabledDevices[peerRootDeviceIndex] = true;
            pPeerDevice->crossAccessEnabledDevices[this->getNEODevice()->getRootDeviceIndex()] = true;

            ret = L0::CommandQueue::fromHandle(commandQueue)->synchronize(std::numeric_limits<uint64_t>::max());
            if (ret == ZE_RESULT_SUCCESS) {
                *value = true;
            }
        }
    }

    contextImp->freeMem(peerMemory);
    contextImp->freeMem(memory);

    L0::Context::fromHandle(context)->destroy();
    L0::CommandQueue::fromHandle(commandQueue)->destroy();
    L0::CommandList::fromHandle(commandList)->destroy();

    if (ret == ZE_RESULT_ERROR_DEVICE_LOST) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::canAccessPeer(ze_device_handle_t hPeerDevice, ze_bool_t *value) {
    *value = false;

    DeviceImp *pPeerDevice = static_cast<DeviceImp *>(Device::fromHandle(hPeerDevice));
    uint32_t peerRootDeviceIndex = pPeerDevice->getNEODevice()->getRootDeviceIndex();

    if (NEO::DebugManager.flags.ForceZeDeviceCanAccessPerReturnValue.get() != -1) {
        *value = !!NEO::DebugManager.flags.ForceZeDeviceCanAccessPerReturnValue.get();
        return ZE_RESULT_SUCCESS;
    }

    if (this->crossAccessEnabledDevices.find(peerRootDeviceIndex) != this->crossAccessEnabledDevices.end()) {
        *value = this->crossAccessEnabledDevices[peerRootDeviceIndex];
        return ZE_RESULT_SUCCESS;
    }

    if (this->getNEODevice()->getRootDeviceIndex() == peerRootDeviceIndex) {
        *value = true;
        return ZE_RESULT_SUCCESS;
    }

    uint32_t latency = std::numeric_limits<uint32_t>::max();
    uint32_t bandwidth = 0;
    ze_result_t res = queryFabricStats(pPeerDevice, latency, bandwidth);
    if (res == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE || bandwidth == 0) {
        return submitCopyForP2P(hPeerDevice, value);
    }

    *value = true;
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::createCommandList(const ze_command_list_desc_t *desc,
                                         ze_command_list_handle_t *commandList) {
    if (!this->isQueueGroupOrdinalValid(desc->commandQueueGroupOrdinal)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    uint32_t index = 0;
    uint32_t commandQueueGroupOrdinal = desc->commandQueueGroupOrdinal;
    adjustCommandQueueDesc(commandQueueGroupOrdinal, index);

    NEO::EngineGroupType engineGroupType = getEngineGroupTypeForOrdinal(commandQueueGroupOrdinal);

    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto createCommandList = getCmdListCreateFunc(desc);
    *commandList = createCommandList(productFamily, this, engineGroupType, desc->flags, returnValue);

    return returnValue;
}

ze_result_t DeviceImp::createCommandListImmediate(const ze_command_queue_desc_t *desc,
                                                  ze_command_list_handle_t *phCommandList) {
    if (!this->isQueueGroupOrdinalValid(desc->ordinal)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_command_queue_desc_t commandQueueDesc = *desc;
    adjustCommandQueueDesc(commandQueueDesc.ordinal, commandQueueDesc.index);

    NEO::EngineGroupType engineGroupType = getEngineGroupTypeForOrdinal(commandQueueDesc.ordinal);

    auto productFamily = neoDevice->getHardwareInfo().platform.eProductFamily;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    *phCommandList = CommandList::createImmediate(productFamily, this, &commandQueueDesc, false, engineGroupType, returnValue);

    return returnValue;
}

void DeviceImp::adjustCommandQueueDesc(uint32_t &ordinal, uint32_t &index) {
    auto nodeOrdinal = NEO::DebugManager.flags.NodeOrdinal.get();
    if (nodeOrdinal != -1) {
        const NEO::HardwareInfo &hwInfo = neoDevice->getHardwareInfo();
        const NEO::GfxCoreHelper &gfxCoreHelper = neoDevice->getGfxCoreHelper();
        auto &engineGroups = getActiveDevice()->getRegularEngineGroups();

        auto engineGroupType = gfxCoreHelper.getEngineGroupType(static_cast<aub_stream::EngineType>(nodeOrdinal), NEO::EngineUsage::Regular, hwInfo);
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

    bool isCopyOnly = NEO::EngineHelper::isCopyOnlyEngineType(
        getEngineGroupTypeForOrdinal(commandQueueDesc.ordinal));

    if (commandQueueDesc.priority == ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW && !isCopyOnly) {
        getCsrForLowPriority(&csr);
    } else {
        auto ret = getCsrForOrdinalAndIndex(&csr, commandQueueDesc.ordinal, commandQueueDesc.index);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
    }

    UNRECOVERABLE_IF(csr == nullptr);

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    *commandQueue = CommandQueue::create(platform.eProductFamily, this, csr, &commandQueueDesc, isCopyOnly, false, returnValue);

    return returnValue;
}

void DeviceImp::populateSubDeviceCopyEngineGroups() {
    NEO::Device *activeDevice = this->getActiveDevice();
    if (this->isImplicitScalingCapable() == false || activeDevice->getNumSubDevices() == 0) {
        return;
    }

    NEO::Device *activeSubDevice = activeDevice->getSubDevice(0u);
    auto &subDeviceEngineGroups = activeSubDevice->getRegularEngineGroups();
    uint32_t numSubDeviceEngineGroups = static_cast<uint32_t>(subDeviceEngineGroups.size());

    for (uint32_t subDeviceQueueGroupsIter = 0; subDeviceQueueGroupsIter < numSubDeviceEngineGroups; subDeviceQueueGroupsIter++) {
        if (NEO::EngineHelper::isCopyOnlyEngineType(subDeviceEngineGroups[subDeviceQueueGroupsIter].engineGroupType)) {
            subDeviceCopyEngineGroups.push_back(subDeviceEngineGroups[subDeviceQueueGroupsIter]);
        }
    }
}

uint32_t DeviceImp::getCopyQueueGroupsFromSubDevice(uint32_t numberOfSubDeviceCopyEngineGroupsRequested,
                                                    ze_command_queue_group_properties_t *pCommandQueueGroupProperties) {
    uint32_t numSubDeviceCopyEngineGroups = static_cast<uint32_t>(this->subDeviceCopyEngineGroups.size());

    if (pCommandQueueGroupProperties == nullptr) {
        return numSubDeviceCopyEngineGroups;
    }

    auto &gfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto &l0GfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t subDeviceQueueGroupsIter = 0;
    for (; subDeviceQueueGroupsIter < std::min(numSubDeviceCopyEngineGroups, numberOfSubDeviceCopyEngineGroupsRequested); subDeviceQueueGroupsIter++) {
        pCommandQueueGroupProperties[subDeviceQueueGroupsIter].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
        pCommandQueueGroupProperties[subDeviceQueueGroupsIter].maxMemoryFillPatternSize = gfxCoreHelper.getMaxFillPaternSizeForCopyEngine();

        l0GfxCoreHelper.setAdditionalGroupProperty(pCommandQueueGroupProperties[subDeviceQueueGroupsIter], this->subDeviceCopyEngineGroups[subDeviceQueueGroupsIter]);
        pCommandQueueGroupProperties[subDeviceQueueGroupsIter].numQueues = static_cast<uint32_t>(this->subDeviceCopyEngineGroups[subDeviceQueueGroupsIter].engines.size());
    }

    return subDeviceQueueGroupsIter;
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

    auto &gfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
    auto &l0GfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    *pCount = std::min(totalEngineGroups, *pCount);
    for (uint32_t i = 0; i < std::min(numEngineGroups, *pCount); i++) {
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::RenderCompute) {
            pCommandQueueGroupProperties[i].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_METRICS |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS;
            pCommandQueueGroupProperties[i].maxMemoryFillPatternSize = std::numeric_limits<size_t>::max();
        }
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Compute) {
            pCommandQueueGroupProperties[i].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY |
                                                    ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COOPERATIVE_KERNELS;
            pCommandQueueGroupProperties[i].maxMemoryFillPatternSize = std::numeric_limits<size_t>::max();
        }
        if (engineGroups[i].engineGroupType == NEO::EngineGroupType::Copy) {
            pCommandQueueGroupProperties[i].flags = ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY;
            pCommandQueueGroupProperties[i].maxMemoryFillPatternSize = gfxCoreHelper.getMaxFillPaternSizeForCopyEngine();
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

                if (strcmp(edgeProperties.model, "XeLink") == 0) {
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
    if (this->getNEODevice()->getHardwareInfo().capabilityTable.p2pAccessSupported &&
        peerDevice->getNEODevice()->getHardwareInfo().capabilityTable.p2pAccessSupported) {
        pP2PProperties->flags = ZE_DEVICE_P2P_PROPERTY_FLAG_ACCESS;
        if (this->getNEODevice()->getHardwareInfo().capabilityTable.p2pAtomicAccessSupported &&
            peerDevice->getNEODevice()->getHardwareInfo().capabilityTable.p2pAtomicAccessSupported) {
            pP2PProperties->flags |= ZE_DEVICE_P2P_PROPERTY_FLAG_ATOMICS;
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
    strcpy_s(pMemProperties->name, ZE_MAX_DEVICE_NAME, productHelper.getDeviceMemoryName().c_str());
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
            const std::array<ze_device_memory_ext_type_t, 5> sysInfoMemType = {
                ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR4,
                ZE_DEVICE_MEMORY_EXT_TYPE_LPDDR5,
                ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
                ZE_DEVICE_MEMORY_EXT_TYPE_HBM2,
                ZE_DEVICE_MEMORY_EXT_TYPE_GDDR6,
            };

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

    pMemAccessProperties->sharedSingleDeviceAllocCapabilities =
        static_cast<ze_memory_access_cap_flags_t>(productHelper.getSingleDeviceSharedMemCapabilities());

    pMemAccessProperties->sharedCrossDeviceAllocCapabilities = {};
    if (this->getNEODevice()->getHardwareInfo().capabilityTable.p2pAccessSupported) {
        pMemAccessProperties->sharedCrossDeviceAllocCapabilities = ZE_MEMORY_ACCESS_CAP_FLAG_RW;

        auto memoryManager = this->getDriverHandle()->getMemoryManager();
        if (memoryManager->isKmdMigrationAvailable(this->getRootDeviceIndex()) &&
            NEO::DebugManager.flags.EnableRecoverablePageFaults.get() == 1) {
            pMemAccessProperties->sharedCrossDeviceAllocCapabilities |= ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT;
            if (this->getNEODevice()->getHardwareInfo().capabilityTable.p2pAtomicAccessSupported) {
                pMemAccessProperties->sharedCrossDeviceAllocCapabilities |= ZE_MEMORY_ACCESS_CAP_FLAG_ATOMIC | ZE_MEMORY_ACCESS_CAP_FLAG_CONCURRENT_ATOMIC;
            }
        }
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
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();
    auto &gfxCoreHelper = this->neoDevice->getGfxCoreHelper();

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
    processAdditionalKernelProperties(gfxCoreHelper, pKernelProperties);
    pKernelProperties->maxArgumentsSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().maxParameterSize);
    pKernelProperties->printfBufferSize = static_cast<uint32_t>(this->neoDevice->getDeviceInfo().printfBufferSize);

    auto &productHelper = this->getProductHelper();

    void *pNext = pKernelProperties->pNext;
    while (pNext) {
        ze_base_desc_t *extendedProperties = reinterpret_cast<ze_base_desc_t *>(pNext);
        if (extendedProperties->stype == ZE_STRUCTURE_TYPE_FLOAT_ATOMIC_EXT_PROPERTIES) {
            ze_float_atomic_ext_properties_t *floatProperties =
                reinterpret_cast<ze_float_atomic_ext_properties_t *>(extendedProperties);
            productHelper.getKernelExtendedProperties(&floatProperties->fp16Flags,
                                                      &floatProperties->fp32Flags,
                                                      &floatProperties->fp64Flags);
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
            auto &l0GfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

            if (l0GfxCoreHelper.platformSupportsRayTracing()) {
                rtProperties->flags = ZE_DEVICE_RAYTRACING_EXT_FLAG_RAYQUERY;
                rtProperties->maxBVHLevels = NEO::RayTracingHelper::maxBvhLevels;
            } else {
                rtProperties->flags = 0;
                rtProperties->maxBVHLevels = 0;
            }
        }
        pNext = const_cast<void *>(extendedProperties->pNext);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getProperties(ze_device_properties_t *pDeviceProperties) {
    const auto &deviceInfo = this->neoDevice->getDeviceInfo();
    const auto &hardwareInfo = this->neoDevice->getHardwareInfo();
    auto &gfxCoreHelper = this->neoDevice->getGfxCoreHelper();

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

    if (NEO::DebugManager.flags.DebugApiUsed.get() == 1) {
        pDeviceProperties->numSubslicesPerSlice = hardwareInfo.gtSystemInfo.MaxSubSlicesSupported / hardwareInfo.gtSystemInfo.MaxSlicesSupported;
    } else {
        pDeviceProperties->numSubslicesPerSlice = hardwareInfo.gtSystemInfo.SubSliceCount / hardwareInfo.gtSystemInfo.SliceCount;
    }

    pDeviceProperties->numSlices = hardwareInfo.gtSystemInfo.SliceCount;

    if (isImplicitScalingCapable()) {
        pDeviceProperties->numSlices *= neoDevice->getNumGenericSubDevices();
    }

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
        const auto &isReturnSubDevicesAsApiDevices = NEO::DebugManager.flags.ReturnSubDevicesAsApiDevices.get();
        if (isReturnSubDevicesAsApiDevices != 1) {
            pDeviceProperties->flags |= ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE;
        }
    }

    if (this->neoDevice->getDeviceInfo().errorCorrectionSupport) {
        pDeviceProperties->flags |= ZE_DEVICE_PROPERTY_FLAG_ECC;
    }

    if (hardwareInfo.capabilityTable.supportsOnDemandPageFaults) {
        pDeviceProperties->flags |= ZE_DEVICE_PROPERTY_FLAG_ONDEMANDPAGING;
    }

    memset(pDeviceProperties->name, 0, ZE_MAX_DEVICE_NAME);

    std::string name = getNEODevice()->getDeviceInfo().name;
    if (NEO::DebugManager.flags.OverrideDeviceName.get() != "unk") {
        name.assign(NEO::DebugManager.flags.OverrideDeviceName.get().c_str());
    } else if (driverInfo) {
        name.assign(driverInfo->getDeviceName(name).c_str());
    }
    memcpy_s(pDeviceProperties->name, ZE_MAX_DEVICE_NAME, name.c_str(), name.length() + 1);
    if (NEO::DebugManager.flags.EnableL0ReadLUIDExtension.get()) {
        if (pDeviceProperties->pNext) {
            ze_base_properties_t *extendedProperties = reinterpret_cast<ze_base_properties_t *>(pDeviceProperties->pNext);
            if (extendedProperties->stype == ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES) {
                ze_device_luid_ext_properties_t *deviceLuidProperties =
                    reinterpret_cast<ze_device_luid_ext_properties_t *>(extendedProperties);
                deviceLuidProperties->nodeMask = queryDeviceNodeMask();
                ze_result_t result = queryDeviceLuid(deviceLuidProperties);
                if (result != ZE_RESULT_SUCCESS) {
                    return result;
                }
            }
        }
    }

    if (NEO::DebugManager.flags.EnableL0EuCount.get()) {
        if (pDeviceProperties->pNext) {
            ze_base_desc_t *extendedDesc = reinterpret_cast<ze_base_desc_t *>(pDeviceProperties->pNext);
            if (extendedDesc->stype == ZE_STRUCTURE_TYPE_EU_COUNT_EXT) {
                ze_eu_count_ext_t *zeEuCountDesc = reinterpret_cast<ze_eu_count_ext_t *>(extendedDesc);
                uint32_t numTotalEUs = hardwareInfo.gtSystemInfo.MaxEuPerSubSlice * hardwareInfo.gtSystemInfo.SubSliceCount * hardwareInfo.gtSystemInfo.SliceCount;

                if (isImplicitScalingCapable()) {
                    numTotalEUs *= neoDevice->getNumGenericSubDevices();
                }
                zeEuCountDesc->numTotalEUs = numTotalEUs;
            }
        }
    }

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
    auto &stateSaveAreaHeader = NEO::SipKernel::getBindlessDebugSipKernel(*this->getNEODevice()).getStateSaveAreaHeader();

    if (stateSaveAreaHeader.size() == 0) {
        PRINT_DEBUGGER_INFO_LOG("Context state save area header missing", "");
        isDebugAttachAvailable = false;
    }

    auto &hwInfo = neoDevice->getHardwareInfo();
    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        isDebugAttachAvailable = false;
    }

    bool tileAttach = NEO::DebugManager.flags.ExperimentalEnableTileAttach.get();
    pDebugProperties->flags = 0;
    if (isDebugAttachAvailable) {
        if ((isSubdevice && tileAttach) || !isSubdevice) {
            pDebugProperties->flags = zet_device_debug_property_flag_t::ZET_DEVICE_DEBUG_PROPERTY_FLAG_ATTACH;
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::systemBarrier() { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }

ze_result_t DeviceImp::activateMetricGroupsDeferred(uint32_t count,
                                                    zet_metric_group_handle_t *phMetricGroups) {
    if (!this->isSubdevice && this->isImplicitScalingCapable()) {
        for (auto subDevice : this->subDevices) {
            subDevice->getMetricDeviceContext().activateMetricGroupsDeferred(count, phMetricGroups);
        }
    } else {
        metricContext->activateMetricGroupsDeferred(count, phMetricGroups);
    }
    return ZE_RESULT_SUCCESS;
}

void *DeviceImp::getExecEnvironment() { return execEnvironment; }

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

NEO::OSInterface &DeviceImp::getOsInterface() { return *neoDevice->getRootDeviceEnvironment().osInterface; }

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
    auto &gfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();

    device->execEnvironment = (void *)neoDevice->getExecutionEnvironment();
    device->allocationsForReuse = std::make_unique<NEO::AllocationsList>();
    bool platformImplicitScaling = gfxCoreHelper.platformSupportsImplicitScaling(hwInfo);
    device->implicitScalingCapable = NEO::ImplicitScalingHelper::isImplicitScalingEnabled(neoDevice->getDeviceBitfield(), platformImplicitScaling);
    device->metricContext = MetricDeviceContext::create(*device);
    device->builtins = BuiltinFunctionsLib::create(
        device, neoDevice->getBuiltIns());
    device->cacheReservation = CacheReservation::create(*device);
    device->maxNumHwThreads = NEO::GfxCoreHelper::getMaxThreadsForVfe(hwInfo);

    auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();
    device->driverInfo.reset(NEO::DriverInfo::create(&hwInfo, osInterface));

    auto debugSurfaceSize = gfxCoreHelper.getSipKernelMaxDbgSurfaceSize(hwInfo);
    std::vector<char> stateSaveAreaHeader;

    if (neoDevice->getDebugger() || neoDevice->getPreemptionMode() == NEO::PreemptionMode::MidThread) {
        if (neoDevice->getCompilerInterface()) {
            bool ret = NEO::SipKernel::initSipKernel(NEO::SipKernel::getSipKernelType(*neoDevice), *neoDevice);
            UNRECOVERABLE_IF(!ret);

            stateSaveAreaHeader = NEO::SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();
            debugSurfaceSize = NEO::SipKernel::getSipKernel(*neoDevice).getStateSaveAreaSize(neoDevice);
        } else {
            *returnValue = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }
    }

    const bool allocateDebugSurface = (device->getL0Debugger() || neoDevice->getDeviceInfo().debuggerActive) && !isSubDevice;
    NEO::GraphicsAllocation *debugSurface = nullptr;
    if (allocateDebugSurface) {
        debugSurface = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(
            {device->getRootDeviceIndex(), true,
             debugSurfaceSize,
             NEO::AllocationType::DEBUG_CONTEXT_SAVE_AREA,
             false,
             false,
             device->getNEODevice()->getDeviceBitfield()});
        device->setDebugSurface(debugSurface);
    }

    if (debugSurface && stateSaveAreaHeader.size() > 0) {
        const auto &productHelper = neoDevice->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(hwInfo, *debugSurface),
                                                              *neoDevice, debugSurface, 0, stateSaveAreaHeader.data(),
                                                              stateSaveAreaHeader.size());
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
        device->subDevices.push_back(static_cast<Device *>(subDevice));
    }
    device->numSubDevices = static_cast<uint32_t>(device->subDevices.size());

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

        Device *pageFaultDevice = device;
        if (device->implicitScalingCapable) {
            pageFaultDevice = device->subDevices[0];
        }

        device->pageFaultCommandList =
            CommandList::createImmediate(
                device->neoDevice->getHardwareInfo().platform.eProductFamily, pageFaultDevice, &cmdQueueDesc, true, NEO::EngineGroupType::Copy, resultValue);
    }

    if (osInterface) {
        auto pciSpeedInfo = osInterface->getDriverModel()->getPciSpeedInfo();
        device->pciMaxSpeed.genVersion = pciSpeedInfo.genVersion;
        device->pciMaxSpeed.maxBandwidth = pciSpeedInfo.maxBandwidth;
        device->pciMaxSpeed.width = pciSpeedInfo.width;
    }

    if (device->getSourceLevelDebugger()) {
        auto osInterface = neoDevice->getRootDeviceEnvironment().osInterface.get();
        device->getSourceLevelDebugger()
            ->notifyNewDevice(osInterface ? osInterface->getDriverModel()->getDeviceHandle() : 0);
    }
    if (device->getNEODevice()->getAllEngines()[0].commandStreamReceiver->getType() == NEO::CommandStreamReceiverType::CSR_HW) {
        device->createSysmanHandle(isSubDevice);
    }
    device->resourcesReleased = false;

    device->populateSubDeviceCopyEngineGroups();

    return device;
}

void DeviceImp::releaseResources() {
    if (resourcesReleased) {
        return;
    }

    UNRECOVERABLE_IF(neoDevice == nullptr);

    this->bcsSplit.releaseResources();

    if (neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.get() &&
        !neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger->isLegacy()) {
        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->debugger.reset(nullptr);
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

    if (allocationsForReuse.get()) {
        allocationsForReuse->freeAllGraphicsAllocations(neoDevice);
        allocationsForReuse.reset();
    }

    if (getSourceLevelDebugger()) {
        getSourceLevelDebugger()->notifyDeviceDestruction();
    }

    if (!isSubdevice) {
        if (this->debugSurface) {
            this->neoDevice->getMemoryManager()->freeGraphicsMemory(this->debugSurface);
            this->debugSurface = nullptr;
        }
    }

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
        {getRootDeviceIndex(), false, size, NEO::AllocationType::BUFFER_HOST_MEMORY, false, neoDevice->getDeviceBitfield()},
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

NEO::GraphicsAllocation *DeviceImp::allocateMemoryFromHostPtr(const void *buffer, size_t size, bool hostCopyAllowed) {
    NEO::AllocationProperties properties = {getRootDeviceIndex(), false, size,
                                            NEO::AllocationType::EXTERNAL_HOST_PTR,
                                            false, neoDevice->getDeviceBitfield()};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = true;
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

ze_result_t DeviceImp::getCsrForOrdinalAndIndex(NEO::CommandStreamReceiver **csr, uint32_t ordinal, uint32_t index) {
    auto &engineGroups = getActiveDevice()->getRegularEngineGroups();
    uint32_t numEngineGroups = static_cast<uint32_t>(engineGroups.size());

    if (!this->isQueueGroupOrdinalValid(ordinal)) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (ordinal < numEngineGroups) {
        auto &engines = engineGroups[ordinal].engines;
        if (index >= engines.size()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *csr = engines[index].commandStreamReceiver;

        auto &osContext = (*csr)->getOsContext();

        if (neoDevice->getNumberOfRegularContextsPerEngine() > 1 && !osContext.isRootDevice() && NEO::EngineHelpers::isCcs(osContext.getEngineType())) {
            *csr = neoDevice->getNextEngineForMultiRegularContextMode().commandStreamReceiver;
        }
    } else {
        auto subDeviceOrdinal = ordinal - numEngineGroups;
        if (index >= this->subDeviceCopyEngineGroups[subDeviceOrdinal].engines.size()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        *csr = this->subDeviceCopyEngineGroups[subDeviceOrdinal].engines[index].commandStreamReceiver;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t DeviceImp::getCsrForLowPriority(NEO::CommandStreamReceiver **csr) {
    NEO::Device *activeDevice = getActiveDevice();
    if (this->implicitScalingCapable) {
        *csr = activeDevice->getDefaultEngine().commandStreamReceiver;
        return ZE_RESULT_SUCCESS;
    } else {
        for (auto &it : activeDevice->getAllEngines()) {
            if (it.osContext->isLowPriority()) {
                *csr = it.commandStreamReceiver;
                return ZE_RESULT_SUCCESS;
            }
        }
        // if the code falls through, we have no low priority context created by neoDevice.
    }
    UNRECOVERABLE_IF(true);
    return ZE_RESULT_ERROR_UNKNOWN;
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
    } else if (NEO::DebugManager.flags.ExperimentalEnableTileAttach.get()) {
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
    if (neoDevice->getNumGenericSubDevices() > 1u) {
        if (isImplicitScalingCapable()) {
            return this->neoDevice;
        }
        return this->neoDevice->getSubDevice(0);
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
    const auto &hardwareInfo = this->getHwInfo();
    auto &l0GfxCoreHelper = this->neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();

    uint32_t basePackets = l0GfxCoreHelper.getEventBaseMaxPacketCount(hardwareInfo);
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

} // namespace L0
