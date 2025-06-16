/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

using namespace MetricsLibraryApi;

namespace L0 {

MetricsLibrary::MetricsLibrary(OaMetricSourceImp &metricSourceInput)
    : metricSource(metricSourceInput) {}

MetricsLibrary::~MetricsLibrary() {
    release();
}

ze_result_t MetricsLibrary::getInitializationState() {
    return initializationState;
}

bool MetricsLibrary::isInitialized() {
    // Try to initialize metrics library only once.
    if (initializationState == ZE_RESULT_ERROR_UNINITIALIZED) {
        initialize();
    }

    return initializationState == ZE_RESULT_SUCCESS;
}

uint32_t MetricsLibrary::getQueryReportGpuSize() {

    TypedValue_1_0 gpuReportSize = {};

    // Obtain gpu report size.
    if (!isInitialized() ||
        api.GetParameter(ParameterType::QueryHwCountersReportGpuSize, &gpuReportSize.Type, &gpuReportSize) != StatusCode::Success) {

        DEBUG_BREAK_IF(true);
        return 0;
    }

    // Validate gpu report size.
    if (!gpuReportSize.ValueUInt32) {
        DEBUG_BREAK_IF(true);
        return 0;
    }

    return gpuReportSize.ValueUInt32;
}

bool MetricsLibrary::createMetricQuery(const uint32_t slotsCount, QueryHandle_1_0 &query,
                                       NEO::GraphicsAllocation *&pAllocation) {

    std::lock_guard<std::mutex> lock(mutex);

    // Validate metrics library state.
    if (!isInitialized()) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    QueryCreateData_1_0 queryData = {};
    queryData.HandleContext = context;
    queryData.Type = ObjectType::QueryHwCounters;
    queryData.Slots = slotsCount;

    // Create query pool within metrics library.
    if (api.QueryCreate(&queryData, &query) != StatusCode::Success) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    // Register created query.
    queries.push_back(query);

    return true;
}

uint32_t MetricsLibrary::getMetricQueryCount() {
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<uint32_t>(queries.size());
}

bool MetricsLibrary::destroyMetricQuery(QueryHandle_1_0 &query) {
    std::lock_guard<std::mutex> lock(mutex);
    DEBUG_BREAK_IF(!query.IsValid());

    const bool result = isInitialized() && (api.QueryDelete(query) == StatusCode::Success);
    auto iter = std::find_if(queries.begin(), queries.end(), [&](const QueryHandle_1_0 &element) { return element.data == query.data; });

    // Unregister query.
    if (iter != queries.end()) {
        queries.erase(iter);
    }

    return result;
}

bool MetricsLibrary::getMetricQueryReportSize(size_t &rawDataSize) {
    ValueType valueType = ValueType::Last;
    TypedValue_1_0 value = {};

    const bool result = isInitialized() && (api.GetParameter(ParameterType::QueryHwCountersReportApiSize, &valueType, &value) == StatusCode::Success);
    rawDataSize = static_cast<size_t>(value.ValueUInt32);
    DEBUG_BREAK_IF(!result);
    return result;
}

bool MetricsLibrary::getMetricQueryReport(QueryHandle_1_0 &query, const uint32_t slot,
                                          const size_t rawDataSize, uint8_t *pData) {

    GetReportData_1_0 report = {};
    report.Type = ObjectType::QueryHwCounters;
    report.Query.Handle = query;
    report.Query.Slot = slot;
    report.Query.SlotsCount = 1;
    report.Query.Data = pData;
    report.Query.DataSize = static_cast<uint32_t>(rawDataSize);

    const bool result = isInitialized() && (api.GetData(&report) == StatusCode::Success);
    DEBUG_BREAK_IF(!result);
    return result;
}

void MetricsLibrary::initialize() {
    auto &metricsEnumeration = metricSource.getMetricEnumeration();

    // Function should be called only once.
    DEBUG_BREAK_IF(initializationState != ZE_RESULT_ERROR_UNINITIALIZED);

    // Metrics Enumeration needs to be initialized before Metrics Library
    const bool validMetricsEnumeration = metricsEnumeration.isInitialized();
    const bool validMetricsLibrary = validMetricsEnumeration && handle && createContext();

    // Load metrics library and exported functions.
    initializationState = validMetricsLibrary ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
    DEBUG_BREAK_IF(initializationState != ZE_RESULT_SUCCESS);
}

void MetricsLibrary::release() {

    // Delete metric group configurations.
    deleteAllConfigurations();

    // Destroy context.
    if (context.IsValid() && contextDeleteFunction) {
        contextDeleteFunction(context);
    }

    // Reset metric query state to not initialized.
    api = {};
    callbacks = {};
    context = {};
    initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
}

bool MetricsLibrary::load() {
    // Load library.
    handle.reset(NEO::OsLibrary::loadFunc({getFilename()}));

    // Load exported functions.
    if (handle) {
        contextCreateFunction = reinterpret_cast<ContextCreateFunction_1_0>(
            handle->getProcAddress(METRICS_LIBRARY_CONTEXT_CREATE_1_0));
        contextDeleteFunction = reinterpret_cast<ContextDeleteFunction_1_0>(
            handle->getProcAddress(METRICS_LIBRARY_CONTEXT_DELETE_1_0));
    }

    if (contextCreateFunction == nullptr || contextDeleteFunction == nullptr) {
        METRICS_LOG_ERR("cannot load %s exported functions", MetricsLibrary::getFilename());
        return false;
    }

    // Return success if exported functions have been loaded.
    return true;
}

void MetricsLibrary::enableWorkloadPartition() {
    isWorkloadPartitionEnabled = true;
}

void MetricsLibrary::getSubDeviceClientOptions(
    ClientOptionsData_1_0 &subDevice,
    ClientOptionsData_1_0 &subDeviceIndex,
    ClientOptionsData_1_0 &subDeviceCount,
    ClientOptionsData_1_0 &workloadPartition) {

    auto &deviceImp = *static_cast<DeviceImp *>(&metricSource.getDevice());

    std::tuple<uint32_t, uint32_t, uint32_t> subDeviceMap;
    uint32_t hwSubDeviceIndex = 0u;
    uint32_t hwSubDevicesCount = 0u;
    bool requiresSubDeviceHierarchy = false;
    bool isSubDevice = deviceImp.isSubdevice;
    if (deviceImp.getNEODevice()->getExecutionEnvironment()->getSubDeviceHierarchy(deviceImp.getNEODevice()->getRootDeviceIndex(), &subDeviceMap)) {
        hwSubDeviceIndex = std::get<1>(subDeviceMap);
        hwSubDevicesCount = std::get<2>(subDeviceMap);
        requiresSubDeviceHierarchy = true;
        isSubDevice = true;
    }

    if (!isSubDevice) {

        // Root device.
        subDevice.Type = ClientOptionsType::SubDevice;
        subDevice.SubDevice.Enabled = false;

        subDeviceIndex.Type = ClientOptionsType::SubDeviceIndex;
        subDeviceIndex.SubDeviceIndex.Index = static_cast<uint8_t>(deviceImp.getPhysicalSubDeviceId());

        subDeviceCount.Type = ClientOptionsType::SubDeviceCount;
        subDeviceCount.SubDeviceCount.Count = std::max(deviceImp.getNEODevice()->getRootDevice()->getNumSubDevices(), 1u);

        workloadPartition.Type = ClientOptionsType::WorkloadPartition;
        workloadPartition.WorkloadPartition.Enabled = false;

    } else {

        // Sub device.
        subDevice.Type = ClientOptionsType::SubDevice;
        subDevice.SubDevice.Enabled = true;

        subDeviceIndex.Type = ClientOptionsType::SubDeviceIndex;
        if (requiresSubDeviceHierarchy) {
            subDeviceIndex.SubDeviceIndex.Index = hwSubDeviceIndex;
        } else {
            subDeviceIndex.SubDeviceIndex.Index = static_cast<uint8_t>(deviceImp.getPhysicalSubDeviceId());
        }

        subDeviceCount.Type = ClientOptionsType::SubDeviceCount;
        if (requiresSubDeviceHierarchy) {
            subDeviceCount.SubDeviceCount.Count = hwSubDevicesCount;
        } else {
            subDeviceCount.SubDeviceCount.Count = std::max(deviceImp.getNEODevice()->getRootDevice()->getNumSubDevices(), 1u);
        }

        workloadPartition.Type = ClientOptionsType::WorkloadPartition;
        workloadPartition.WorkloadPartition.Enabled = isWorkloadPartitionEnabled;
    }
}

bool MetricsLibrary::createContext() {
    auto &device = metricSource.getDevice();
    const auto &gfxCoreHelper = device.getGfxCoreHelper();
    const auto &asyncComputeEngines = gfxCoreHelper.getGpgpuEngineInstances(device.getNEODevice()->getRootDeviceEnvironment());
    ContextCreateData_1_0 createData = {};
    ClientOptionsData_1_0 clientOptions[6] = {};
    ClientData_1_0 clientData = {};
    ClientType_1_0 clientType = {};
    ClientDataLinuxAdapter_1_0 adapter = {};

    // Check if compute command streamer is used.
    auto asyncComputeEngine = std::find_if(asyncComputeEngines.begin(), asyncComputeEngines.end(), [&](const auto &engine) {
        return engine.first == aub_stream::ENGINE_CCS;
    });

    const auto &deviceImp = *static_cast<DeviceImp *>(&device);
    const auto &commandStreamReceiver = *deviceImp.getNEODevice()->getDefaultEngine().commandStreamReceiver;
    const auto engineType = commandStreamReceiver.getOsContext().getEngineType();
    const bool isComputeUsed = NEO::EngineHelpers::isCcs(engineType);

    metricSource.setUseCompute(isComputeUsed);

    // Create metrics library context.
    DEBUG_BREAK_IF(!contextCreateFunction);
    clientType.Api = ClientApi::OneApi;
    clientType.Gen = getGenType(device.getGfxCoreHelper());

    clientOptions[0].Type = ClientOptionsType::Compute;
    clientOptions[0].Compute.Asynchronous = asyncComputeEngine != asyncComputeEngines.end();

    clientOptions[1].Type = ClientOptionsType::Tbs;
    clientOptions[1].Tbs.Enabled = metricSource.getMetricStreamer() != nullptr;

    // Sub device client options #2
    getSubDeviceClientOptions(clientOptions[2], clientOptions[3], clientOptions[4], clientOptions[5]);

    clientData.Linux.Adapter = &adapter;
    clientData.ClientOptions = clientOptions;
    clientData.ClientOptionsCount = sizeof(clientOptions) / sizeof(ClientOptionsData_1_0);

    createData.Api = &api;
    createData.ClientCallbacks = &callbacks;
    createData.ClientData = &clientData;

    const bool result =
        getContextData(device, createData) &&
        contextCreateFunction(clientType, &createData, &context) == StatusCode::Success;

    DEBUG_BREAK_IF(!result);
    return result;
}

ClientGen MetricsLibrary::getGenType(const NEO::GfxCoreHelper &gfxCoreHelper) const {
    return static_cast<MetricsLibraryApi::ClientGen>(gfxCoreHelper.getMetricsLibraryGenId());
}

uint32_t MetricsLibrary::getGpuCommandsSize(CommandBufferData_1_0 &commandBuffer) {
    CommandBufferSize_1_0 commandBufferSize = {};

    bool result = isInitialized();

    // Validate metrics library initialization state.
    if (result) {
        commandBuffer.HandleContext = context;
        result = api.CommandBufferGetSize(&commandBuffer, &commandBufferSize) == StatusCode::Success;
    }

    DEBUG_BREAK_IF(!result);
    return result ? commandBufferSize.GpuMemorySize : 0;
}

bool MetricsLibrary::getGpuCommands(CommandBufferData_1_0 &commandBuffer) {

    // Obtain gpu commands from metrics library.
    const bool result =
        isInitialized() && (api.CommandBufferGet(&commandBuffer) == StatusCode::Success);
    DEBUG_BREAK_IF(!result);
    return result;
}

bool MetricsLibrary::getGpuCommands(CommandList &commandList,
                                    CommandBufferData_1_0 &commandBuffer) {

    // Obtain required command buffer size.
    commandBuffer.Size = getGpuCommandsSize(commandBuffer);

    // Validate gpu commands size.
    if (!commandBuffer.Size) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    // Allocate command buffer.
    auto stream = commandList.getCmdContainer().getCommandStream();
    auto buffer = stream->getSpace(commandBuffer.Size);

    // Fill attached command buffer with gpu commands.
    commandBuffer.Data = buffer;

    // Obtain gpu commands from metrics library.
    const bool result =
        isInitialized() && (api.CommandBufferGet(&commandBuffer) == StatusCode::Success);
    DEBUG_BREAK_IF(!result);
    return result;
}

ConfigurationHandle_1_0
MetricsLibrary::createConfiguration(const zet_metric_group_handle_t metricGroupHandle,
                                    const zet_metric_group_properties_t &properties) {
    // Metric group internal data.
    auto metricGroup = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(metricGroupHandle));
    auto metricGroupDummy = ConfigurationHandle_1_0{};
    DEBUG_BREAK_IF(!metricGroup);

    // Metrics library configuration creation data.
    ConfigurationHandle_1_0 handle = {};
    ConfigurationCreateData_1_0 handleData = {};

    // Check supported sampling types.
    const bool validSampling =
        properties.samplingType == ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED ||
        properties.samplingType == ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;

    // Activate metric group through metrics discovery to send metric group
    // configuration to kernel driver.
    const bool validActivate = isInitialized() && validSampling && metricGroup->activateMetricSet();

    if (validActivate) {
        handleData.HandleContext = context;
        handleData.Type = ObjectType::ConfigurationHwCountersOa;

        // Use metrics library to create configuration for the activated metric group.
        api.ConfigurationCreate(&handleData, &handle);

        // Use metrics discovery to deactivate metric group.
        metricGroup->deactivateMetricSet();
    }

    return validActivate ? handle : metricGroupDummy;
}

ConfigurationHandle_1_0 MetricsLibrary::getConfiguration(zet_metric_group_handle_t handle) {

    auto iter = configurations.find(handle);
    auto configuration = (iter != end(configurations)) ? iter->second : addConfiguration(handle);

    DEBUG_BREAK_IF(!configuration.IsValid());
    return configuration;
}

ConfigurationHandle_1_0 MetricsLibrary::addConfiguration(zet_metric_group_handle_t handle) {
    ConfigurationHandle_1_0 libraryHandle = {};
    DEBUG_BREAK_IF(!handle);

    // Create metrics library configuration.
    auto metricGroup = MetricGroup::fromHandle(handle);
    zet_metric_group_properties_t properties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    OaMetricGroupImp::getProperties(handle, &properties);
    auto configuration = createConfiguration(metricGroup, properties);

    // Cache configuration if valid.
    if (configuration.IsValid()) {
        libraryHandle = configuration;
        cacheConfiguration(handle, libraryHandle);
    }

    DEBUG_BREAK_IF(!libraryHandle.IsValid());
    return libraryHandle;
}

void MetricsLibrary::deleteAllConfigurations() {

    if (api.ConfigurationDelete) {
        for (auto &configuration : configurations) {
            if (configuration.second.IsValid()) {
                api.ConfigurationDelete(configuration.second);
            }
        }
    }

    configurations.clear();
}

ze_result_t OaMetricGroupImp::metricQueryPoolCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {

    return OaMetricQueryPoolImp::metricQueryPoolCreate(hContext, hDevice, toHandle(), desc, phMetricQueryPool);
}

ze_result_t OaMetricQueryPoolImp::metricQueryPoolCreate(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                                        const zet_metric_query_pool_desc_t *pDesc, zet_metric_query_pool_handle_t *phMetricQueryPool) {
    auto device = Device::fromHandle(hDevice);
    auto &metricSource = device->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();

    // Metric query cannot be used with streamer simultaneously
    // (due to oa buffer usage constraints).

    if (metricSource.getMetricStreamer() != nullptr) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    const auto &deviceImp = *static_cast<DeviceImp *>(device);
    auto metricPoolImp = new OaMetricQueryPoolImp(metricSource, hMetricGroup, *pDesc);

    if (metricSource.isImplicitScalingCapable()) {

        auto emptyMetricGroups = std::vector<MetricGroupImp *>();

        auto metricGroups = hMetricGroup
                                ? static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup))->getMetricGroups()
                                : emptyMetricGroups;

        const bool useMetricGroupSubDevice = metricGroups.size() > 0;

        auto &metricPools = metricPoolImp->getMetricQueryPools();

        for (size_t i = 0; i < deviceImp.numSubDevices; ++i) {

            auto &subDevice = deviceImp.subDevices[i];
            auto &subDeviceMetricSource = subDevice->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>();

            zet_metric_group_handle_t metricGroupHandle = useMetricGroupSubDevice
                                                              ? metricGroups[subDeviceMetricSource.getSubDeviceIndex()]
                                                              : hMetricGroup;

            auto metricPoolSubdeviceImp = new OaMetricQueryPoolImp(subDeviceMetricSource, metricGroupHandle, *pDesc);

            // Create metric query pool.
            if (!metricPoolSubdeviceImp->create()) {
                metricPoolSubdeviceImp->destroy();
                metricPoolImp->destroy();
                metricPoolSubdeviceImp = nullptr;
                metricPoolImp = nullptr;
                *phMetricQueryPool = nullptr;
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }

            metricPools.push_back(metricPoolSubdeviceImp);
        }

    } else {

        // Create metric query pool.
        if (!metricPoolImp->create()) {
            metricPoolImp->destroy();
            metricPoolImp = nullptr;
            *phMetricQueryPool = nullptr;
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    // Allocate gpu memory.
    if (!metricPoolImp->allocateGpuMemory()) {
        metricPoolImp->destroy();
        metricPoolImp = nullptr;
        *phMetricQueryPool = nullptr;
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *phMetricQueryPool = metricPoolImp;

    return ZE_RESULT_SUCCESS;
}

ze_result_t metricQueryPoolCreate(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                  const zet_metric_query_pool_desc_t *pDesc, zet_metric_query_pool_handle_t *phMetricQueryPool) {

    if (pDesc->type == ZET_METRIC_QUERY_POOL_TYPE_EXECUTION) {
        return OaMetricQueryPoolImp::metricQueryPoolCreate(hContext, hDevice, hMetricGroup, pDesc, phMetricQueryPool);
    } else {
        UNRECOVERABLE_IF(hMetricGroup == nullptr);
        return MetricGroup::fromHandle(hMetricGroup)->metricQueryPoolCreate(hContext, hDevice, pDesc, phMetricQueryPool);
    }
}

OaMetricQueryPoolImp::OaMetricQueryPoolImp(OaMetricSourceImp &metricSourceInput,
                                           zet_metric_group_handle_t hEventMetricGroupInput,
                                           const zet_metric_query_pool_desc_t &poolDescription)
    : metricSource(metricSourceInput), metricsLibrary(metricSource.getMetricsLibrary()),
      description(poolDescription),
      hMetricGroup(hEventMetricGroupInput) {}

bool OaMetricQueryPoolImp::create() {
    switch (description.type) {
    case ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE:
        return createMetricQueryPool();
    case ZET_METRIC_QUERY_POOL_TYPE_EXECUTION:
        return createSkipExecutionQueryPool();
    default:
        DEBUG_BREAK_IF(true);
        return false;
    }
}

ze_result_t OaMetricQueryPoolImp::destroy() {
    switch (description.type) {
    case ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE:
        if (metricQueryPools.size() > 0) {
            for (auto &metricQueryPool : metricQueryPools) {
                MetricQueryPool::fromHandle(metricQueryPool)->destroy();
            }
        }
        if (query.IsValid()) {
            metricsLibrary.destroyMetricQuery(query);
        }
        if (pAllocation) {
            metricSource.getDevice().getDriverHandle()->getMemoryManager()->freeGraphicsMemory(pAllocation);
        }
        break;
    case ZET_METRIC_QUERY_POOL_TYPE_EXECUTION:
        for (auto &metricQueryPool : metricQueryPools) {
            MetricQueryPool::fromHandle(metricQueryPool)->destroy();
        }
        break;
    default:
        DEBUG_BREAK_IF(true);
        break;
    }

    // Check open queries.
    if (metricSource.getMetricsLibrary().getMetricQueryCount() == 0) {
        if (!metricSource.isMetricGroupActivatedInHw()) {
            metricSource.getMetricsLibrary().release();
        }
    }

    delete this;

    return ZE_RESULT_SUCCESS;
}

bool OaMetricQueryPoolImp::allocateGpuMemory() {

    if (description.type == ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE) {
        // Get allocation size.
        const auto &deviceImp = *static_cast<DeviceImp *>(&metricSource.getDevice());

        allocationSize = (metricSource.isImplicitScalingCapable())
                             ? deviceImp.subDevices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricsLibrary().getQueryReportGpuSize() * description.count * deviceImp.numSubDevices
                             : metricsLibrary.getQueryReportGpuSize() * description.count;

        if (allocationSize == 0) {
            return false;
        }

        // Allocate gpu memory.
        NEO::AllocationProperties properties(
            metricSource.getDevice().getRootDeviceIndex(), allocationSize, NEO::AllocationType::bufferHostMemory, metricSource.getDevice().getNEODevice()->getDeviceBitfield());
        properties.alignment = 64u;
        pAllocation = metricSource.getDevice().getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

        UNRECOVERABLE_IF(pAllocation == nullptr);

        // Clear allocation.
        memset(pAllocation->getUnderlyingBuffer(), 0, allocationSize);
    }
    return true;
}

bool OaMetricQueryPoolImp::createMetricQueryPool() {
    // Validate metric group query - only event based is supported.
    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    OaMetricGroupImp::getProperties(hMetricGroup, &metricGroupProperties);
    const bool validMetricGroup = metricGroupProperties.samplingType == ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

    if (!validMetricGroup) {
        return false;
    }

    // Pool initialization.
    pool.reserve(description.count);
    for (uint32_t i = 0; i < description.count; ++i) {
        pool.push_back({metricSource, *this, i});
    }

    // Metrics library query object initialization.
    return metricsLibrary.createMetricQuery(description.count, query, pAllocation);
}

bool OaMetricQueryPoolImp::createSkipExecutionQueryPool() {

    pool.reserve(description.count);
    for (uint32_t i = 0; i < description.count; ++i) {
        pool.push_back({metricSource, *this, i});
    }

    return true;
}

MetricQueryPool *MetricQueryPool::fromHandle(zet_metric_query_pool_handle_t handle) {
    return static_cast<MetricQueryPool *>(handle);
}

zet_metric_query_pool_handle_t MetricQueryPool::toHandle() { return this; }

ze_result_t OaMetricQueryPoolImp::metricQueryCreate(uint32_t index,
                                                    zet_metric_query_handle_t *phMetricQuery) {

    if (index >= description.count) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (metricQueryPools.size() > 0) {

        auto pMetricQueryImp = new OaMetricQueryImp(metricSource, *this, index);

        for (auto metricQueryPoolHandle : metricQueryPools) {
            auto &metricQueries = pMetricQueryImp->getMetricQueries();
            auto metricQueryPoolImp = static_cast<OaMetricQueryPoolImp *>(MetricQueryPool::fromHandle(metricQueryPoolHandle));
            metricQueries.push_back(&metricQueryPoolImp->pool[index]);
        }

        *phMetricQuery = pMetricQueryImp;

        return ZE_RESULT_SUCCESS;

    } else {

        *phMetricQuery = &(pool[index]);

        return ZE_RESULT_SUCCESS;
    }
}

std::vector<zet_metric_query_pool_handle_t> &OaMetricQueryPoolImp::getMetricQueryPools() {
    return metricQueryPools;
}

OaMetricQueryImp::OaMetricQueryImp(OaMetricSourceImp &metricSourceInput, OaMetricQueryPoolImp &poolInput,
                                   const uint32_t slotInput)
    : metricSource(metricSourceInput), metricsLibrary(metricSource.getMetricsLibrary()),
      pool(poolInput), slot(slotInput) {}

ze_result_t OaMetricQueryImp::appendBegin(CommandList &commandList) {
    switch (pool.description.type) {
    case ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE:
        return writeMetricQuery(commandList, nullptr, 0, nullptr, true);
    case ZET_METRIC_QUERY_POOL_TYPE_EXECUTION:
        return writeSkipExecutionQuery(commandList, nullptr, 0, nullptr, true);
    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
}

ze_result_t OaMetricQueryImp::appendEnd(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                        uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    switch (pool.description.type) {
    case ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE:
        return writeMetricQuery(commandList, hSignalEvent, numWaitEvents, phWaitEvents, false);
    case ZET_METRIC_QUERY_POOL_TYPE_EXECUTION:
        return writeSkipExecutionQuery(commandList, hSignalEvent, numWaitEvents, phWaitEvents, false);
    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }
}

ze_result_t OaMetricQueryImp::getData(size_t *pRawDataSize, uint8_t *pRawData) {

    const bool calculateSizeOnly = *pRawDataSize == 0;
    const size_t metricQueriesSize = metricQueries.size();
    bool result = true;

    if (metricQueriesSize > 0) {

        if (calculateSizeOnly) {

            const size_t headerSize = sizeof(MetricGroupCalculateHeader);
            const size_t rawDataOffsetsRequiredSize = sizeof(uint32_t) * metricQueriesSize;
            const size_t rawDataSizesRequiredSize = sizeof(uint32_t) * metricQueriesSize;

            auto pMetricQueryImp = static_cast<OaMetricQueryImp *>(MetricQuery::fromHandle(metricQueries[0]));
            result = pMetricQueryImp->metricsLibrary.getMetricQueryReportSize(*pRawDataSize);

            const size_t rawDataRequiredSize = *pRawDataSize * metricQueriesSize;

            *pRawDataSize = headerSize + rawDataOffsetsRequiredSize + rawDataSizesRequiredSize + rawDataRequiredSize;

        } else {

            MetricGroupCalculateHeader *pRawDataHeader = reinterpret_cast<MetricGroupCalculateHeader *>(pRawData);
            pRawDataHeader->magic = MetricGroupCalculateHeader::magicValue;
            pRawDataHeader->dataCount = static_cast<uint32_t>(metricQueriesSize);

            // Relative offsets in the header allow to move/copy the buffer.
            pRawDataHeader->rawDataOffsets = sizeof(MetricGroupCalculateHeader);
            pRawDataHeader->rawDataSizes = static_cast<uint32_t>(pRawDataHeader->rawDataOffsets + (sizeof(uint32_t) * metricQueriesSize));
            pRawDataHeader->rawDataOffset = static_cast<uint32_t>(pRawDataHeader->rawDataSizes + (sizeof(uint32_t) * metricQueriesSize));

            const size_t sizePerSubDevice = (*pRawDataSize - pRawDataHeader->rawDataOffset) / metricQueriesSize;
            DEBUG_BREAK_IF(sizePerSubDevice == 0);
            *pRawDataSize = pRawDataHeader->rawDataOffset;

            uint32_t *pRawDataOffsetsUnpacked = reinterpret_cast<uint32_t *>(pRawData + pRawDataHeader->rawDataOffsets);
            uint32_t *pRawDataSizesUnpacked = reinterpret_cast<uint32_t *>(pRawData + pRawDataHeader->rawDataSizes);
            uint8_t *pRawDataUnpacked = reinterpret_cast<uint8_t *>(pRawData + pRawDataHeader->rawDataOffset);

            for (size_t i = 0; i < metricQueriesSize; ++i) {

                size_t getDataSize = sizePerSubDevice;
                const uint32_t rawDataOffset = (i != 0) ? (pRawDataSizesUnpacked[i - 1] + pRawDataOffsetsUnpacked[i - 1]) : 0;
                auto pMetricQuery = MetricQuery::fromHandle(metricQueries[i]);
                ze_result_t tmpResult = pMetricQuery->getData(&getDataSize, pRawDataUnpacked + rawDataOffset);
                // Return at first error.
                if (tmpResult != ZE_RESULT_SUCCESS) {
                    return tmpResult;
                }
                pRawDataSizesUnpacked[i] = static_cast<uint32_t>(getDataSize);
                pRawDataOffsetsUnpacked[i] = (i != 0) ? pRawDataOffsetsUnpacked[i - 1] + pRawDataSizesUnpacked[i - 1] : 0;
                *pRawDataSize += getDataSize;
            }
        }

    } else {
        result = calculateSizeOnly
                     ? metricsLibrary.getMetricQueryReportSize(*pRawDataSize)
                     : metricsLibrary.getMetricQueryReport(pool.query, slot, *pRawDataSize, pRawData);
    }

    return result
               ? ZE_RESULT_SUCCESS
               : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t OaMetricQueryImp::reset() {
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricQueryImp::destroy() {

    if (metricQueries.size() > 0) {
        delete this;
    }

    return ZE_RESULT_SUCCESS;
}

std::vector<zet_metric_query_handle_t> &OaMetricQueryImp::getMetricQueries() {
    return metricQueries;
}

ze_result_t OaMetricQueryImp::writeMetricQuery(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                               uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                               const bool begin) {

    bool result = true;
    const bool writeCompletionEvent = hSignalEvent && !begin;
    const size_t metricQueriesSize = metricQueries.size();

    // Make gpu allocation visible.
    commandList.getCmdContainer().addToResidencyContainer(pool.pAllocation);

    // Wait for events before executing query.
    commandList.appendWaitOnEvents(numWaitEvents, phWaitEvents, nullptr, false, true, false, false, false, false);

    if (metricQueriesSize) {

        const size_t allocationSizeForSubDevice = pool.allocationSize / metricQueriesSize;
        static_cast<CommandListImp &>(commandList).appendMultiPartitionPrologue(static_cast<uint32_t>(allocationSizeForSubDevice));
        void *buffer = nullptr;
        bool gpuCommandStatus = true;

        // Revert iteration to be ensured that the last set of gpu commands overwrite the previous written sets of gpu commands,
        // so only one of the sub-device contexts will be used to append to command list.
        for (int32_t i = static_cast<int32_t>(metricQueriesSize - 1); i >= 0; --i) {

            // Adjust cpu and gpu addresses for each sub-device's query object.
            uint64_t gpuAddress = pool.pAllocation->getGpuAddress() + (i * allocationSizeForSubDevice);
            uint8_t *cpuAddress = static_cast<uint8_t *>(pool.pAllocation->getUnderlyingBuffer()) + (i * allocationSizeForSubDevice);

            auto &metricQueryImp = *static_cast<OaMetricQueryImp *>(MetricQuery::fromHandle(metricQueries[i]));
            auto &metricLibrarySubDevice = metricQueryImp.metricsLibrary;
            auto &metricSourceSubDevice = metricQueryImp.metricSource;

            // Obtain gpu commands.
            CommandBufferData_1_0 commandBuffer = {};
            commandBuffer.CommandsType = ObjectType::QueryHwCounters;
            commandBuffer.QueryHwCounters.Handle = metricQueryImp.pool.query;
            commandBuffer.QueryHwCounters.Begin = begin;
            commandBuffer.QueryHwCounters.Slot = slot;
            commandBuffer.Allocation.GpuAddress = gpuAddress;
            commandBuffer.Allocation.CpuAddress = cpuAddress;
            commandBuffer.Type = metricSourceSubDevice.isComputeUsed()
                                     ? GpuCommandBufferType::Compute
                                     : GpuCommandBufferType::Render;

            // Obtain required command buffer size.
            commandBuffer.Size = metricLibrarySubDevice.getGpuCommandsSize(commandBuffer);

            // Validate gpu commands size.
            if (!commandBuffer.Size) {
                return ZE_RESULT_ERROR_UNKNOWN;
            }

            // Allocate command buffer only once.
            if (buffer == nullptr) {
                auto stream = commandList.getCmdContainer().getCommandStream();
                buffer = stream->getSpace(commandBuffer.Size);
            }

            // Fill attached command buffer with gpu commands.
            commandBuffer.Data = buffer;

            // Obtain gpu commands from metrics library for each sub-device to update cpu and gpu addresses for
            // each query object in metrics library, so that get data works properly.
            gpuCommandStatus = metricLibrarySubDevice.getGpuCommands(commandBuffer);
            if (!gpuCommandStatus) {
                break;
            }
        }
        static_cast<CommandListImp &>(commandList).appendMultiPartitionEpilogue();
        if (!gpuCommandStatus) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        // Write gpu commands for sub device index 0.
    } else {
        // Obtain gpu commands.
        CommandBufferData_1_0 commandBuffer = {};
        commandBuffer.CommandsType = ObjectType::QueryHwCounters;
        commandBuffer.QueryHwCounters.Handle = pool.query;
        commandBuffer.QueryHwCounters.Begin = begin;
        commandBuffer.QueryHwCounters.Slot = slot;
        commandBuffer.Allocation.GpuAddress = pool.pAllocation->getGpuAddress();
        commandBuffer.Allocation.CpuAddress = pool.pAllocation->getUnderlyingBuffer();
        commandBuffer.Type = metricSource.isComputeUsed()
                                 ? GpuCommandBufferType::Compute
                                 : GpuCommandBufferType::Render;

        // Get query commands.
        result = metricsLibrary.getGpuCommands(commandList, commandBuffer);
    }

    // Write completion event.
    if (result && writeCompletionEvent) {
        result = commandList.appendSignalEvent(hSignalEvent, false) == ZE_RESULT_SUCCESS;
    }

    return result ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t OaMetricQueryImp::writeSkipExecutionQuery(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                                      uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                                      const bool begin) {

    bool writeCompletionEvent = hSignalEvent && !begin;
    bool result = false;

    // Obtain gpu commands.
    CommandBufferData_1_0 commandBuffer = {};
    commandBuffer.CommandsType = ObjectType::OverrideNullHardware;
    commandBuffer.Override.Enable = begin;
    commandBuffer.Type = metricSource.isComputeUsed()
                             ? GpuCommandBufferType::Compute
                             : GpuCommandBufferType::Render;

    // Wait for events before executing query.
    zeCommandListAppendWaitOnEvents(commandList.toHandle(), numWaitEvents, phWaitEvents);

    // Get query commands.
    result = metricsLibrary.getGpuCommands(commandList, commandBuffer);

    // Write completion event.
    if (result && writeCompletionEvent) {
        result = zeCommandListAppendSignalEvent(commandList.toHandle(), hSignalEvent) ==
                 ZE_RESULT_SUCCESS;
    }

    return result ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

MetricQuery *MetricQuery::fromHandle(zet_metric_query_handle_t handle) {
    return static_cast<MetricQuery *>(handle);
}

zet_metric_query_handle_t MetricQuery::toHandle() { return this; }

StatusCode ML_STDCALL MetricsLibrary::flushCommandBufferCallback(ClientHandle_1_0 handle) {
    Device *device = static_cast<Device *>(handle.data);
    if (device) {
        device->getNEODevice()->stopDirectSubmissionAndWaitForCompletion();
        return StatusCode::Success;
    }
    return StatusCode::Failed;
}

} // namespace L0
