/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric.inl"
#include "level_zero/tools/source/metrics/metric_multidevice_programmable.h"
#include "level_zero/tools/source/metrics/metric_multidevice_programmable.inl"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_programmable_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"
#include "level_zero/zet_intel_gpu_metric.h"

namespace L0 {

std::unique_ptr<OaMetricSourceImp> OaMetricSourceImp::create(const MetricDeviceContext &metricDeviceContext) {
    return std::unique_ptr<OaMetricSourceImp>(new (std::nothrow) OaMetricSourceImp(metricDeviceContext));
}

OaMetricSourceImp::OaMetricSourceImp(const MetricDeviceContext &metricDeviceContext) : metricDeviceContext(metricDeviceContext),
                                                                                       metricEnumeration(std::unique_ptr<MetricEnumeration>(new(std::nothrow) MetricEnumeration(*this))),
                                                                                       metricsLibrary(std::unique_ptr<MetricsLibrary>(new(std::nothrow) MetricsLibrary(*this))) {
    activationTracker = std::make_unique<MultiDomainDeferredActivationTracker>(metricDeviceContext.getSubDeviceIndex());
    type = MetricSource::metricSourceTypeOa;
}

OaMetricSourceImp::~OaMetricSourceImp() = default;

void OaMetricSourceImp::enable() {
    loadDependencies();
}

ze_result_t OaMetricSourceImp::getTimerResolution(uint64_t &resolution) {
    if (!metricEnumeration->readGlobalSymbol(globalSymbolOaGpuTimestampFrequency.data(), resolution)) {
        resolution = 0;
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

void OaMetricSourceImp::getTimestampValidBits(uint64_t &validBits) {
    DeviceImp *deviceImp = static_cast<DeviceImp *>(&getDevice());
    auto &l0GfxCoreHelper = deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    validBits = l0GfxCoreHelper.getOaTimestampValidBits();
}

bool OaMetricSourceImp::isAvailable() {
    return isInitialized();
}

ze_result_t OaMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    DeviceImp *pDeviceImp = static_cast<DeviceImp *>(commandList.getDevice());

    if (pDeviceImp->metricContext->isImplicitScalingCapable()) {
        // Use one of the sub-device contexts to append to command list.
        pDeviceImp = static_cast<DeviceImp *>(pDeviceImp->subDevices[0]);
    }

    auto &metricContext = pDeviceImp->getMetricDeviceContext();
    auto &metricsLibrary = metricContext.getMetricSource<OaMetricSourceImp>().getMetricsLibrary();

    // Obtain gpu commands.
    CommandBufferData_1_0 commandBuffer = {};
    commandBuffer.CommandsType = MetricsLibraryApi::ObjectType::OverrideFlushCaches;
    commandBuffer.Override.Enable = true;
    commandBuffer.Type = metricContext.getMetricSource<OaMetricSourceImp>().isComputeUsed()
                             ? MetricsLibraryApi::GpuCommandBufferType::Compute
                             : MetricsLibraryApi::GpuCommandBufferType::Render;

    return metricsLibrary.getGpuCommands(commandList, commandBuffer) ? ZE_RESULT_SUCCESS
                                                                     : ZE_RESULT_ERROR_UNKNOWN;
}

bool OaMetricSourceImp::loadDependencies() {
    bool result = true;
    if (metricEnumeration->loadMetricsDiscovery() != ZE_RESULT_SUCCESS) {
        result = false;
        DEBUG_BREAK_IF(!result);
    }
    if (result && !metricsLibrary->load()) {
        result = false;
        DEBUG_BREAK_IF(!result);
    }

    // Set metric context initialization state.
    setInitializationState(result
                               ? ZE_RESULT_SUCCESS
                               : ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);

    return result;
}
bool OaMetricSourceImp::isInitialized() {
    return initializationState == ZE_RESULT_SUCCESS;
}

void OaMetricSourceImp::setInitializationState(const ze_result_t state) {
    initializationState = state;
}

Device &OaMetricSourceImp::getDevice() {
    return metricDeviceContext.getDevice();
}

bool OaMetricSourceImp::canDisable() {
    return !activationTracker->isAnyMetricGroupActivated();
}

void OaMetricSourceImp::initMetricScopes(MetricDeviceContext &metricDeviceContext) {
    if (!metricDeviceContext.isComputeMetricScopesInitialized()) {
        initComputeMetricScopes(metricDeviceContext);
    }
}

MetricsLibrary &OaMetricSourceImp::getMetricsLibrary() {
    return *metricsLibrary;
}
MetricEnumeration &OaMetricSourceImp::getMetricEnumeration() {
    return *metricEnumeration;
}
MetricStreamer *OaMetricSourceImp::getMetricStreamer() {
    return pMetricStreamer;
}

void OaMetricSourceImp::setMetricStreamer(MetricStreamer *pMetricStreamer) {
    this->pMetricStreamer = pMetricStreamer;
}

void OaMetricSourceImp::setUseCompute(const bool useCompute) {
    this->useCompute = useCompute;
}

bool OaMetricSourceImp::isComputeUsed() const {
    return useCompute;
}

ze_result_t OaMetricSourceImp::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    return getMetricEnumeration().metricGroupGet(*pCount, phMetricGroups);
}

uint32_t OaMetricSourceImp::getSubDeviceIndex() {
    return metricDeviceContext.getSubDeviceIndex();
}

bool OaMetricSourceImp::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
    return activationTracker->isMetricGroupActivated(hMetricGroup);
}

bool OaMetricSourceImp::isMetricGroupActivatedInHw() const {
    return activationTracker->isMetricGroupActivatedInHw();
}

bool OaMetricSourceImp::isImplicitScalingCapable() const {
    return metricDeviceContext.isImplicitScalingCapable();
}

ze_result_t OaMetricSourceImp::activateMetricGroupsPreferDeferred(uint32_t count,
                                                                  zet_metric_group_handle_t *phMetricGroups) {
    DeviceImp &deviceImp = static_cast<DeviceImp &>(metricDeviceContext.getDevice());
    if (metricDeviceContext.isImplicitScalingCapable()) {
        return MetricSource::activatePreferDeferredHierarchical<OaMetricSourceImp>(&deviceImp, count, phMetricGroups);
    }

    activationTracker->activateMetricGroupsDeferred(count, phMetricGroups);
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricSourceImp::activateMetricGroupsAlreadyDeferred() {
    return activationTracker->activateMetricGroupsAlreadyDeferred();
}

ze_result_t OaMetricSourceImp::getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                                         uint32_t *pConcurrentGroupCount,
                                                         uint32_t *pCountPerConcurrentGroup) {

    if (*pConcurrentGroupCount == 0) {
        *pConcurrentGroupCount = static_cast<uint32_t>(hMetricGroups.size());
        return ZE_RESULT_SUCCESS;
    }

    *pConcurrentGroupCount = std::min(*pConcurrentGroupCount, static_cast<uint32_t>(hMetricGroups.size()));
    // Each metric group is in unique container
    for (uint32_t index = 0; index < *pConcurrentGroupCount; index++) {
        pCountPerConcurrentGroup[index] = 1;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricSourceImp::handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup,
                                                                   zet_metric_group_properties_t *pBaseProperties,
                                                                   void *pNext) {
    ze_result_t retVal = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    while (pNext) {
        auto extendedProperties = reinterpret_cast<zet_base_properties_t *>(pNext);

        if (static_cast<uint32_t>(extendedProperties->stype) == ZET_INTEL_STRUCTURE_TYPE_METRIC_SOURCE_ID_EXP) {

            getMetricGroupSourceIdProperty(extendedProperties);
            retVal = ZE_RESULT_SUCCESS;
        } else if (extendedProperties->stype == ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP) {

            zet_metric_global_timestamps_resolution_exp_t *metricsTimestampProperties =
                reinterpret_cast<zet_metric_global_timestamps_resolution_exp_t *>(extendedProperties);

            retVal = getTimerResolution(metricsTimestampProperties->timerResolution);
            if (retVal != ZE_RESULT_SUCCESS) {
                metricsTimestampProperties->timerResolution = 0;
                metricsTimestampProperties->timestampValidBits = 0;
                return retVal;
            }

            getTimestampValidBits(metricsTimestampProperties->timestampValidBits);

        } else if (extendedProperties->stype == ZET_STRUCTURE_TYPE_METRIC_GROUP_TYPE_EXP) {
            zet_metric_group_type_exp_t *groupType = reinterpret_cast<zet_metric_group_type_exp_t *>(extendedProperties);
            groupType->type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_OTHER;
            retVal = ZE_RESULT_SUCCESS;
        } else if (static_cast<uint32_t>(extendedProperties->stype) == ZET_INTEL_STRUCTURE_TYPE_METRIC_GROUP_CALCULATION_EXP_PROPERTIES) {
            auto calcProperties = reinterpret_cast<zet_intel_metric_group_calculation_properties_exp_t *>(extendedProperties);
            if (pBaseProperties->samplingType == ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {
                calcProperties->isTimeFilterSupported = true;
            } else {
                calcProperties->isTimeFilterSupported = false;
            }
            retVal = ZE_RESULT_SUCCESS;
        }
        pNext = extendedProperties->pNext;
    }

    return retVal;
}

void OaMetricSourceImp::metricGroupCreate(const char name[ZET_MAX_METRIC_GROUP_NAME],
                                          const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                          zet_metric_group_sampling_type_flag_t samplingType,
                                          zet_metric_group_handle_t *pMetricGroupHandle) {

    zet_metric_group_properties_t properties{};
    memcpy_s(properties.description, ZET_MAX_METRIC_GROUP_DESCRIPTION, description, ZET_MAX_METRIC_GROUP_DESCRIPTION);
    memcpy_s(properties.name, ZET_MAX_METRIC_GROUP_NAME, name, ZET_MAX_METRIC_GROUP_NAME);
    properties.samplingType = samplingType;
    properties.domain = UINT32_MAX;

    auto concurrentGrp = getMetricEnumeration().getConcurrentGroup();
    MetricsDiscovery::IMetricSet_1_13 *metricSet = concurrentGrp->AddMetricSet(name, description);
    auto metricGroup = OaMetricGroupUserDefined::create(properties, *metricSet, *concurrentGrp, *this);
    *pMetricGroupHandle = metricGroup->toHandle();
}

ze_result_t OaMetricSourceImp::metricGroupCreateFromMetric(const char *pName, const char *pDescription,
                                                           zet_metric_group_sampling_type_flags_t samplingType, zet_metric_handle_t hMetric,
                                                           zet_metric_group_handle_t *phMetricGroup) {

    zet_metric_group_handle_t hMetricGroup{};
    metricGroupCreate(pName, pDescription, static_cast<zet_metric_group_sampling_type_flag_t>(samplingType), &hMetricGroup);

    auto oaMetricGroupImp = static_cast<OaMetricGroupUserDefined *>(MetricGroup::fromHandle(hMetricGroup));
    size_t errorStringSize = 0;
    auto status = oaMetricGroupImp->addMetric(hMetric, &errorStringSize, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
        oaMetricGroupImp->destroy();
        return status;
    }

    *phMetricGroup = hMetricGroup;
    return status;
}

ze_result_t OaMetricSourceImp::createMetricGroupsFromMetrics(std::vector<zet_metric_handle_t> &metricList,
                                                             const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                                             const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                             uint32_t *maxMetricGroupCount,
                                                             std::vector<zet_metric_group_handle_t> &metricGroupList) {

    if (isImplicitScalingCapable()) {
        return MultiDeviceCreatedMetricGroupManager::createMultipleMetricGroupsFromMetrics<OaMultiDeviceMetricGroupUserDefined>(
            metricDeviceContext, *this, metricList,
            metricGroupNamePrefix, description,
            maxMetricGroupCount, metricGroupList);
    }

    const auto isCountCalculationPath = *maxMetricGroupCount == 0;

    auto cleanupCreatedGroups = [](std::vector<zet_metric_group_handle_t> &createdMetricGroupList) {
        for (auto &metricGroup : createdMetricGroupList) {
            zetMetricGroupDestroyExp(metricGroup);
        }
        createdMetricGroupList.clear();
    };

    if (isCountCalculationPath) {
        // Metric group can be for streamer and query from a single programmable
        // So multiplying by 2 to estimate the maximum metric group count
        *maxMetricGroupCount = static_cast<uint32_t>(metricList.size()) * 2u;
        return ZE_RESULT_SUCCESS;
    }

    // Arrange the metrics based on their sampling types
    std::map<zet_metric_group_sampling_type_flags_t, std::vector<zet_metric_handle_t>> samplingTypeToMeticMap{};
    for (auto &metric : metricList) {
        auto metricImp = static_cast<OaMetricImp *>(Metric::fromHandle(metric));
        auto metricFromProgrammable = static_cast<OaMetricFromProgrammable *>(metricImp);
        auto samplingType = metricFromProgrammable->getSupportedSamplingType();
        // Different metric groups based on sampling type
        if (samplingType == METRICS_SAMPLING_TYPE_TIME_EVENT_BASED) {
            samplingTypeToMeticMap[ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED].push_back(metric);
            samplingTypeToMeticMap[ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED].push_back(metric);
        } else {
            samplingTypeToMeticMap[samplingType].push_back(metric);
        }
    }

    metricGroupList.clear();
    uint32_t numMetricGroupsCreated = 0;

    auto createMetricGroupAndAddMetric = [&](zet_metric_handle_t metricHandle,
                                             zet_metric_group_sampling_type_flags_t samplingType,
                                             zet_metric_group_handle_t &metricGroup) {
        char metricGroupName[ZET_MAX_METRIC_GROUP_NAME] = {};
        snprintf(metricGroupName, ZET_MAX_METRIC_GROUP_NAME - 1, "%s%d", metricGroupNamePrefix, numMetricGroupsCreated);
        auto status = metricGroupCreateFromMetric(metricGroupName, description, samplingType, metricHandle, &metricGroup);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        numMetricGroupsCreated++;
        return ZE_RESULT_SUCCESS;
    };

    bool isMaxMetricGroupCountReached = numMetricGroupsCreated >= *maxMetricGroupCount;
    // Process the metrics in each sampling type separately
    for (auto &entry : samplingTypeToMeticMap) {
        if (isMaxMetricGroupCountReached) {
            break;
        }
        std::vector<zet_metric_group_handle_t> perSamplingTypeMetricGroupList{};
        zet_metric_group_handle_t currentMetricGroup{};
        auto samplingType = entry.first;

        // Create and add the metrics to group
        for (uint32_t index = 0; index < static_cast<uint32_t>(entry.second.size()); index++) {

            auto &metricToAdd = entry.second[index];
            bool isAddedToExistingMetricGroup = false;
            for (auto &perSamplingTypeMetricGroup : perSamplingTypeMetricGroupList) {
                auto oaMetricGroup = static_cast<OaMetricGroupUserDefined *>(MetricGroup::fromHandle(perSamplingTypeMetricGroup));
                size_t errorStringSize = 0;
                auto status = oaMetricGroup->addMetric(metricToAdd, &errorStringSize, nullptr);
                if (status == ZE_RESULT_SUCCESS) {
                    isAddedToExistingMetricGroup = true;
                    break;
                }
            }

            if (!isAddedToExistingMetricGroup) {
                if (isMaxMetricGroupCountReached) {
                    break;
                }
                currentMetricGroup = nullptr;
                auto status = createMetricGroupAndAddMetric(metricToAdd, samplingType, currentMetricGroup);
                if (status != ZE_RESULT_SUCCESS) {
                    cleanupCreatedGroups(metricGroupList);
                    cleanupCreatedGroups(perSamplingTypeMetricGroupList);
                    *maxMetricGroupCount = 0;
                    return status;
                }
                perSamplingTypeMetricGroupList.push_back(currentMetricGroup);
                isMaxMetricGroupCountReached = numMetricGroupsCreated >= *maxMetricGroupCount;
            }
        }
        metricGroupList.insert(metricGroupList.end(), perSamplingTypeMetricGroupList.begin(), perSamplingTypeMetricGroupList.end());
    }

    // close all the metric groups
    for (auto &metricGroup : metricGroupList) {
        auto oaMetricGroup = static_cast<OaMetricGroupUserDefined *>(MetricGroup::fromHandle(metricGroup));
        auto status = oaMetricGroup->close();
        if (status != ZE_RESULT_SUCCESS) {
            cleanupCreatedGroups(metricGroupList);
            *maxMetricGroupCount = 0;
            return status;
        }
    }

    *maxMetricGroupCount = static_cast<uint32_t>(metricGroupList.size());
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricSourceImp::metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) {
    return getMetricEnumeration().metricProgrammableGet(pCount, phMetricProgrammables);
}

ze_result_t OaMetricSourceImp::appendMarker(zet_command_list_handle_t hCommandList, zet_metric_group_handle_t hMetricGroup, uint32_t value) {

    auto commandListImp = static_cast<CommandListImp *>(CommandList::fromHandle(hCommandList));
    DeviceImp *pDeviceImp = static_cast<DeviceImp *>(commandListImp->getDevice());

    if (pDeviceImp->metricContext->isImplicitScalingCapable()) {
        // Use one of the sub-device contexts to append to command list.
        pDeviceImp = static_cast<DeviceImp *>(pDeviceImp->subDevices[0]);
    }

    OaMetricSourceImp &metricSource = pDeviceImp->metricContext->getMetricSource<OaMetricSourceImp>();
    auto &metricsLibrary = metricSource.getMetricsLibrary();

    // Obtain gpu commands.
    CommandBufferData_1_0 commandBuffer = {};
    commandBuffer.CommandsType = MetricsLibraryApi::ObjectType::MarkerStreamUser;
    commandBuffer.MarkerStreamUser.Value = value;
    commandBuffer.Type = metricSource.isComputeUsed()
                             ? MetricsLibraryApi::GpuCommandBufferType::Compute
                             : MetricsLibraryApi::GpuCommandBufferType::Render;

    return metricsLibrary.getGpuCommands(*commandListImp, commandBuffer) ? ZE_RESULT_SUCCESS
                                                                         : ZE_RESULT_ERROR_UNKNOWN;
}

template <>
OaMetricSourceImp &MetricDeviceContext::getMetricSource<OaMetricSourceImp>() const {
    return static_cast<OaMetricSourceImp &>(*metricSources.at(MetricSource::metricSourceTypeOa));
}

} // namespace L0
