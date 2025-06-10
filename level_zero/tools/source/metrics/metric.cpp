/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include <map>
#include <utility>

namespace L0 {

void MetricSource::getMetricGroupSourceIdProperty(zet_base_properties_t *property) {

    zet_intel_metric_source_id_exp_t *groupProperty = reinterpret_cast<zet_intel_metric_source_id_exp_t *>(property);
    groupProperty->sourceId = type;
}

MetricDeviceContext::MetricDeviceContext(Device &inputDevice) : device(inputDevice) {
    auto deviceNeo = device.getNEODevice();
    std::tuple<uint32_t, uint32_t, uint32_t> subDeviceMap;
    uint32_t hwSubDeviceIndex = 0u;
    bool requiresSubDeviceHierarchy = false;
    if (deviceNeo->getExecutionEnvironment()->getSubDeviceHierarchy(deviceNeo->getRootDeviceIndex(), &subDeviceMap)) {
        hwSubDeviceIndex = std::get<1>(subDeviceMap);
        requiresSubDeviceHierarchy = true;
    }
    if (requiresSubDeviceHierarchy) {
        subDeviceIndex = hwSubDeviceIndex;
        multiDeviceCapable = false;
    } else {
        bool isSubDevice = deviceNeo->isSubDevice();
        subDeviceIndex = isSubDevice
                             ? static_cast<NEO::SubDevice *>(deviceNeo)->getSubDeviceIndex()
                             : 0;

        multiDeviceCapable = !isSubDevice && device.isImplicitScalingCapable();
    }
    metricSources[MetricSource::metricSourceTypeOa] = OaMetricSourceImp::create(*this);
    metricSources[MetricSource::metricSourceTypeIpSampling] = IpSamplingMetricSourceImp::create(*this);

    if (NEO::debugManager.flags.DisableProgrammableMetricsSupport.get()) {
        isProgrammableMetricsEnabled = false;
    }
}

bool MetricDeviceContext::enable() {
    bool status = false;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;

        // Enable only if not already enabled.
        if (!isEnableChecked) {
            metricSource->enable();
        }
        status |= metricSource->isAvailable();
    }
    setMetricsCollectionAllowed(status);
    isEnableChecked = true;
    return status;
}

bool MetricDeviceContext::canDisable() {
    if (isMetricsCollectionAllowed) {
        for (auto const &entry : metricSources) {
            auto const &metricSource = entry.second;
            if (!metricSource->canDisable()) {
                return false;
            }
        }
    }
    return true;
}

void MetricDeviceContext::disable() {
    setMetricsCollectionAllowed(false);
}

ze_result_t MetricDeviceContext::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t availableCount = 0;
    uint32_t requestCount = *pCount;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;

        if (!metricSource->isAvailable()) {
            continue;
        }

        result = metricSource->metricGroupGet(&requestCount, phMetricGroups);
        if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
            result = ZE_RESULT_SUCCESS;
            continue;
        }

        if (result != ZE_RESULT_SUCCESS) {
            break;
        }
        availableCount += requestCount;
        if (*pCount == 0) {
            requestCount = 0;
        } else {
            DEBUG_BREAK_IF(availableCount > *pCount);
            phMetricGroups += requestCount;
            requestCount = *pCount - availableCount;
            if (requestCount == 0) {
                break;
            }
        }
    }

    *pCount = availableCount;
    return result;
}

ze_result_t MetricDeviceContext::activateMetricGroupsPreferDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) {

    if (!isMetricsCollectionAllowed) {
        METRICS_LOG_ERR("%s", "Cannot activate when metrics is disabled");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    // Create a map of metric source types and Metric groups
    std::map<uint32_t, std::vector<zet_metric_group_handle_t>> metricGroupsPerMetricSourceMap{};
    for (auto index = 0u; index < count; index++) {
        auto &metricGroupSource =
            static_cast<MetricGroupImp *>(MetricGroup::fromHandle(phMetricGroups[index]))->getMetricSource();
        metricGroupsPerMetricSourceMap[metricGroupSource.getType()].push_back(phMetricGroups[index]);
    }

    for (auto const &entry : metricSources) {
        auto const &metricSourceEntry = entry.second;
        auto status = ZE_RESULT_SUCCESS;
        if (!metricSourceEntry->isAvailable()) {
            continue;
        }

        auto sourceType = metricSourceEntry->getType();

        if (metricGroupsPerMetricSourceMap.find(sourceType) == metricGroupsPerMetricSourceMap.end()) {
            status = metricSourceEntry->activateMetricGroupsPreferDeferred(0, nullptr);
        } else {
            auto &metricGroupVec = metricGroupsPerMetricSourceMap[sourceType];
            status = metricSourceEntry->activateMetricGroupsPreferDeferred(
                static_cast<uint32_t>(metricGroupVec.size()),
                metricGroupVec.data());
        }

        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricDeviceContext::appendMetricMemoryBarrier(CommandList &commandList) {

    bool isSuccess = false;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;
        if (!metricSource->isAvailable()) {
            continue;
        }

        ze_result_t result = metricSource->appendMetricMemoryBarrier(commandList);
        if (result == ZE_RESULT_SUCCESS) {
            isSuccess = true;
        } else if (result != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
            return result;
        }
    }
    return isSuccess == false ? ZE_RESULT_ERROR_UNSUPPORTED_FEATURE : ZE_RESULT_SUCCESS;
}

bool MetricDeviceContext::isImplicitScalingCapable() const {
    return multiDeviceCapable;
}

ze_result_t MetricDeviceContext::activateMetricGroups() {
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;
        metricSource->activateMetricGroupsAlreadyDeferred();
    }
    return ZE_RESULT_SUCCESS;
}

uint32_t MetricDeviceContext::getSubDeviceIndex() const {
    return subDeviceIndex;
}

Device &MetricDeviceContext::getDevice() const {
    return device;
}

void MetricDeviceContext::enableMetricApiForDevice(zet_device_handle_t hDevice, bool &isFailed) {

    auto deviceImp = static_cast<DeviceImp *>(L0::Device::fromHandle(hDevice));
    std::lock_guard<std::mutex> lock(deviceImp->getMetricDeviceContext().enableMetricsMutex);
    // Initialize device.
    isFailed |= !deviceImp->metricContext->enable();

    // Initialize sub devices if available.
    for (uint32_t i = 0; i < deviceImp->numSubDevices; ++i) {
        isFailed |= !deviceImp->subDevices[i]->getMetricDeviceContext().enable();
    }
}

ze_result_t MetricDeviceContext::disableMetricApiForDevice(zet_device_handle_t hDevice) {

    auto deviceImp = static_cast<DeviceImp *>(L0::Device::fromHandle(hDevice));
    std::lock_guard<std::mutex> lock(deviceImp->getMetricDeviceContext().enableMetricsMutex);

    for (uint32_t i = 0; i < deviceImp->numSubDevices; ++i) {
        if (!deviceImp->subDevices[i]->getMetricDeviceContext().canDisable()) {
            METRICS_LOG_ERR("%s", "Cannot disable sub device, since metrics resources are still in use.");
            return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
        }
    }

    if (!deviceImp->getMetricDeviceContext().canDisable()) {
        METRICS_LOG_ERR("%s", "Cannot disable root device, since metrics resources are still in use.");
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    for (uint32_t i = 0; i < deviceImp->numSubDevices; ++i) {
        deviceImp->subDevices[i]->getMetricDeviceContext().disable();
    }
    deviceImp->getMetricDeviceContext().disable();
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricDeviceContext::enableMetricApi() {

    bool failed = false;

    for (auto &globalDriverHandle : *globalDriverHandles) {
        auto driverHandle = L0::DriverHandle::fromHandle(globalDriverHandle);
        auto rootDevices = std::vector<ze_device_handle_t>();
        auto subDevices = std::vector<ze_device_handle_t>();

        // Obtain root devices.
        uint32_t rootDeviceCount = 0;
        driverHandle->getDevice(&rootDeviceCount, nullptr);
        rootDevices.resize(rootDeviceCount);
        driverHandle->getDevice(&rootDeviceCount, rootDevices.data());

        for (auto rootDeviceHandle : rootDevices) {
            enableMetricApiForDevice(rootDeviceHandle, failed);
        }
        if (failed) {
            break;
        }
    }

    return failed
               ? ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE
               : ZE_RESULT_SUCCESS;
}

ze_result_t MetricDeviceContext::getConcurrentMetricGroups(uint32_t metricGroupCount,
                                                           zet_metric_group_handle_t *phMetricGroups,
                                                           uint32_t *pConcurrentGroupCount, uint32_t *pCountPerConcurrentGroup) {

    if (!areMetricGroupsFromSameDeviceHierarchy(metricGroupCount, phMetricGroups)) {
        METRICS_LOG_ERR("%s", "Mix of root device and sub-device metric group handle is not allowed");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    std::map<MetricSource *, std::vector<zet_metric_group_handle_t>> metricGroupsPerMetricSourceMap{};
    for (auto index = 0u; index < metricGroupCount; index++) {
        auto &metricGroupSource =
            static_cast<MetricGroupImp *>(MetricGroup::fromHandle(phMetricGroups[index]))->getMetricSource();
        metricGroupsPerMetricSourceMap[&metricGroupSource].push_back(phMetricGroups[index]);
    }

    // Calculate the maximum concurrent group count
    uint32_t maxConcurrentGroupCount = 0;
    for (auto &[source, metricGroups] : metricGroupsPerMetricSourceMap) {
        uint32_t perSourceConcurrentCount = 0;
        auto status = source->getConcurrentMetricGroups(metricGroups, &perSourceConcurrentCount, nullptr);
        if (status != ZE_RESULT_SUCCESS) {
            METRICS_LOG_ERR("Per source concurrent metric group query returned error status %d", status);
            *pConcurrentGroupCount = 0;
            return status;
        }
        maxConcurrentGroupCount = std::max(maxConcurrentGroupCount, perSourceConcurrentCount);
    }

    if (*pConcurrentGroupCount == 0) {
        *pConcurrentGroupCount = maxConcurrentGroupCount;
        return ZE_RESULT_SUCCESS;
    }

    if (*pConcurrentGroupCount != maxConcurrentGroupCount) {
        METRICS_LOG_ERR("Input Concurrent Group Count %d is not same as expected %d", *pConcurrentGroupCount, maxConcurrentGroupCount);
        *pConcurrentGroupCount = 0;
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    std::vector<std::vector<zet_metric_group_handle_t>> concurrentGroups(maxConcurrentGroupCount);
    for (auto &entry : metricGroupsPerMetricSourceMap) {

        auto source = entry.first;
        auto &metricGroups = entry.second;

        // Using maximum possible concurrent group count
        uint32_t perSourceConcurrentCount = metricGroupCount;
        std::vector<uint32_t> countPerConcurrentGroup(perSourceConcurrentCount);
        auto status = source->getConcurrentMetricGroups(metricGroups, &perSourceConcurrentCount, countPerConcurrentGroup.data());
        if (status != ZE_RESULT_SUCCESS) {
            METRICS_LOG_ERR("getConcurrentMetricGroups returned error status %d", status);
            *pConcurrentGroupCount = 0;
            return status;
        }

        DEBUG_BREAK_IF(static_cast<uint32_t>(concurrentGroups.size() < perSourceConcurrentCount));

        auto metricGroupsStartOffset = metricGroups.begin();
        [[maybe_unused]] uint32_t totalMetricGroupCount = 0;
        // Copy the handles to appropriate groups
        for (uint32_t groupIndex = 0; groupIndex < perSourceConcurrentCount; groupIndex++) {
            totalMetricGroupCount += countPerConcurrentGroup[groupIndex];
            DEBUG_BREAK_IF(totalMetricGroupCount > static_cast<uint32_t>(metricGroups.size()));
            concurrentGroups[groupIndex].insert(concurrentGroups[groupIndex].end(),
                                                metricGroupsStartOffset,
                                                metricGroupsStartOffset + countPerConcurrentGroup[groupIndex]);
            metricGroupsStartOffset += countPerConcurrentGroup[groupIndex];
        }
    }

    // Update the concurrent Group count and count per concurrent grup
    *pConcurrentGroupCount = static_cast<uint32_t>(concurrentGroups.size());
    for (uint32_t index = 0u; index < *pConcurrentGroupCount; index++) {
        pCountPerConcurrentGroup[index] = static_cast<uint32_t>(concurrentGroups[index].size());
    }

    // Update the output metric groups
    size_t availableSize = metricGroupCount;
    for (auto &concurrentGroup : concurrentGroups) {
        memcpy_s(phMetricGroups, availableSize * sizeof(zet_metric_group_handle_t), concurrentGroup.data(), concurrentGroup.size() * sizeof(zet_metric_group_handle_t));

        availableSize -= concurrentGroup.size();
        phMetricGroups += concurrentGroup.size();
    }

    DEBUG_BREAK_IF(availableSize != 0);

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricDeviceContext::metricProgrammableGet(
    uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) {

    if (!isProgrammableMetricsEnabled) {
        *pCount = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t availableCount = 0;
    uint32_t requestCount = *pCount;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;

        if (!metricSource->isAvailable()) {
            continue;
        }

        result = metricSource->metricProgrammableGet(&requestCount, phMetricProgrammables);
        if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
            result = ZE_RESULT_SUCCESS;
            continue;
        }

        if (result != ZE_RESULT_SUCCESS) {
            // Currently there is no error possible other than unsupported feature
            DEBUG_BREAK_IF(true);
            break;
        }

        availableCount += requestCount;
        if (*pCount == 0) {
            requestCount = 0;
        } else {
            DEBUG_BREAK_IF(availableCount > *pCount);
            phMetricProgrammables += requestCount;
            requestCount = *pCount - availableCount;
            if (requestCount == 0) {
                break;
            }
        }
    }
    *pCount = availableCount;

    return result;
}

ze_result_t MetricDeviceContext::createMetricGroupsFromMetrics(uint32_t metricCount, zet_metric_handle_t *phMetrics,
                                                               const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                                               const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                               uint32_t *pMetricGroupCount,
                                                               zet_metric_group_handle_t *phMetricGroups) {
    if (!isProgrammableMetricsEnabled) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (metricCount == 0) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    zet_metric_group_handle_t *cleanupMetricGroups = phMetricGroups;

    // Create a map of metric source types and Metrics
    std::map<MetricSource *, std::vector<zet_metric_handle_t>> metricsPerMetricSourceMap{};
    for (auto index = 0u; index < metricCount; index++) {

        auto metricImp = static_cast<MetricImp *>(Metric::fromHandle(phMetrics[index]));
        if (metricImp->isImmutable()) {
            *pMetricGroupCount = 0;
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        auto &metricSource = metricImp->getMetricSource();
        metricsPerMetricSourceMap[&metricSource].push_back(phMetrics[index]);
    }

    auto isGetMetricGroupCountPath = *pMetricGroupCount == 0u;
    uint32_t remainingMetricGroupCount = *pMetricGroupCount;
    auto cleanupApi = [&]() {
        if (!isGetMetricGroupCountPath) {
            for (uint32_t index = 0; index < (*pMetricGroupCount - remainingMetricGroupCount); index++) {
                zetMetricGroupDestroyExp(cleanupMetricGroups[index]);
            }
        }
    };

    for (auto &metricSourceEntry : metricsPerMetricSourceMap) {

        MetricSource *metricSource = metricSourceEntry.first;
        std::vector<zet_metric_group_handle_t> metricGroupList{};
        uint32_t metricGroupCountPerSource = remainingMetricGroupCount;
        auto status = metricSource->createMetricGroupsFromMetrics(metricSourceEntry.second, metricGroupNamePrefix, description, &metricGroupCountPerSource, metricGroupList);
        if (status != ZE_RESULT_SUCCESS) {
            if (status == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
                continue;
            }
            cleanupApi();
            *pMetricGroupCount = 0;
            return status;
        }

        if (isGetMetricGroupCountPath) {
            *pMetricGroupCount += metricGroupCountPerSource;
        } else {
            memcpy_s(phMetricGroups, remainingMetricGroupCount * sizeof(zet_metric_group_handle_t), metricGroupList.data(), metricGroupCountPerSource * sizeof(metricGroupList[0]));
            DEBUG_BREAK_IF(metricGroupCountPerSource > remainingMetricGroupCount);
            phMetricGroups += metricGroupCountPerSource;
            remainingMetricGroupCount -= metricGroupCountPerSource;
            if (!remainingMetricGroupCount) {
                break;
            }
        }
    }

    *pMetricGroupCount = *pMetricGroupCount - remainingMetricGroupCount;
    return ZE_RESULT_SUCCESS;
}

bool MetricDeviceContext::areMetricGroupsFromSameDeviceHierarchy(uint32_t count, zet_metric_group_handle_t *phMetricGroups) {
    bool isRootDevice = isImplicitScalingCapable();

    // Verify whether metricGroups belong to the device heirarchy
    for (uint32_t index = 0; index < count; index++) {
        auto metricGroupImp = static_cast<MetricGroupImp *>(MetricGroup::fromHandle(phMetricGroups[index]));
        if (isRootDevice != metricGroupImp->isRootDevice()) {
            return false;
        }
    }
    return true;
}

bool MetricDeviceContext::areMetricsFromSameDeviceHierarchy(uint32_t count, zet_metric_handle_t *phMetrics) {
    bool isRootDevice = isImplicitScalingCapable();

    // Verify whether metricGroups belong to the device heirarchy
    for (uint32_t index = 0; index < count; index++) {
        auto metricImp = static_cast<MetricImp *>(Metric::fromHandle(phMetrics[index]));
        if (isRootDevice != metricImp->isRootDevice()) {
            return false;
        }
    }
    return true;
}

ze_result_t MetricDeviceContext::metricGroupCreate(const char name[ZET_MAX_METRIC_GROUP_NAME],
                                                   const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                   zet_metric_group_sampling_type_flag_t samplingType,
                                                   zet_metric_group_handle_t *pMetricGroupHandle) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool MetricDeviceContext::areMetricGroupsFromSameSource(uint32_t count, zet_metric_group_handle_t *phMetricGroups, uint32_t *sourceType) {
    DEBUG_BREAK_IF(count == 0);
    auto metricGroupImp = static_cast<MetricGroupImp *>(MetricGroup::fromHandle(phMetricGroups[0]));
    *sourceType = metricGroupImp->getMetricSource().getType();

    // Verify whether all metric groups have the same source type
    for (uint32_t index = 1; index < count; index++) {
        metricGroupImp = static_cast<MetricGroupImp *>(MetricGroup::fromHandle(phMetricGroups[index]));
        if (*sourceType != metricGroupImp->getMetricSource().getType()) {
            *sourceType = MetricSource::metricSourceTypeUndefined;
            return false;
        }
    }
    return true;
}

bool MetricDeviceContext::areMetricsFromSameSource(uint32_t count, zet_metric_handle_t *phMetrics, uint32_t *sourceType) {

    DEBUG_BREAK_IF(count == 0);
    auto metricImp = static_cast<MetricImp *>(Metric::fromHandle(phMetrics[0]));
    *sourceType = metricImp->getMetricSource().getType();

    // Verify whether all metrics have the same source type
    for (uint32_t index = 1; index < count; index++) {
        auto metricImp = static_cast<MetricImp *>(Metric::fromHandle(phMetrics[index]));
        if (*sourceType != metricImp->getMetricSource().getType()) {
            *sourceType = MetricSource::metricSourceTypeUndefined;
            return false;
        }
    }
    return true;
}

ze_result_t MetricDeviceContext::calcOperationCreate(zet_context_handle_t hContext,
                                                     zet_intel_metric_calculate_exp_desc_t *pCalculateDesc,
                                                     uint32_t *pExcludedMetricCount,
                                                     zet_metric_handle_t *phExcludedMetrics,
                                                     zet_intel_metric_calculate_operation_exp_handle_t *phCalculateOperation) {

    if (pCalculateDesc->timeAggregationWindow == 0) {
        METRICS_LOG_ERR("%s", "Must define an aggregation window");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    uint32_t metricGroupsSourceType = MetricSource::metricSourceTypeUndefined;
    MetricGroupImp *metricGroupImp = nullptr;
    if (pCalculateDesc->metricGroupCount > 0) {
        if (!areMetricGroupsFromSameSource(pCalculateDesc->metricGroupCount, pCalculateDesc->phMetricGroups, &metricGroupsSourceType)) {
            METRICS_LOG_ERR("%s", "Metric groups must be from the same domain");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        if (!areMetricGroupsFromSameDeviceHierarchy(pCalculateDesc->metricGroupCount, pCalculateDesc->phMetricGroups)) {
            METRICS_LOG_ERR("%s", "Mix of root device and sub-device metric group handle is not allowed");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        metricGroupImp = static_cast<MetricGroupImp *>(MetricGroup::fromHandle(pCalculateDesc->phMetricGroups[0]));
    }

    uint32_t metricsSourceType = MetricSource::metricSourceTypeUndefined;
    MetricImp *metricImp = nullptr;
    if (pCalculateDesc->metricCount > 0) {
        if (!areMetricsFromSameSource(pCalculateDesc->metricCount, pCalculateDesc->phMetrics, &metricsSourceType)) {
            METRICS_LOG_ERR("%s", "Metrics must be from the same domain");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        metricImp = static_cast<MetricImp *>(Metric::fromHandle(pCalculateDesc->phMetrics[0]));
        // IpSampling does not use multi-device metrics
        if ((metricImp->getMetricSource().getType() != MetricSource::metricSourceTypeIpSampling) &&
            (!areMetricsFromSameDeviceHierarchy(pCalculateDesc->metricCount, pCalculateDesc->phMetrics))) {
            METRICS_LOG_ERR("%s", "Mix of root device and sub-device metric handle is not allowed");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    if (pCalculateDesc->metricGroupCount > 0) {
        if ((pCalculateDesc->metricCount > 0) && (metricGroupsSourceType != metricsSourceType)) {
            METRICS_LOG_ERR("%s", "Metric groups and metrics must be from the same domain");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    } else if (pCalculateDesc->metricCount == 0) {
        METRICS_LOG_ERR("%s", "Must define at least one metric group or metric");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    MetricSource &metricSource = (metricGroupImp) ? metricGroupImp->getMetricSource() : metricImp->getMetricSource(); // NOLINT(clang-analyzer-core.CallAndMessage)
    return metricSource.calcOperationCreate(*this, pCalculateDesc, pExcludedMetricCount, phExcludedMetrics, phCalculateOperation);
}

ze_result_t MultiDeviceMetricImp::getProperties(zet_metric_properties_t *pProperties) {
    return subDeviceMetrics[0]->getProperties(pProperties);
}

MultiDeviceMetricImp *MultiDeviceMetricImp::create(MetricSource &metricSource, std::vector<MetricImp *> &subDeviceMetrics) {
    return new (std::nothrow) MultiDeviceMetricImp(metricSource, subDeviceMetrics);
}

MetricImp *MultiDeviceMetricImp::getMetricAtSubDeviceIndex(uint32_t index) {
    if (index < subDeviceMetrics.size()) {
        return subDeviceMetrics.at(index);
    }
    return nullptr;
}

ze_result_t metricGroupGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    auto device = Device::fromHandle(hDevice);
    return device->getMetricDeviceContext().metricGroupGet(pCount, phMetricGroups);
}

ze_result_t metricStreamerOpen(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                               zet_metric_streamer_desc_t *pDesc, ze_event_handle_t hNotificationEvent,
                               zet_metric_streamer_handle_t *phMetricStreamer) {
    return MetricGroup::fromHandle(hMetricGroup)->streamerOpen(hContext, hDevice, pDesc, hNotificationEvent, phMetricStreamer);
}

bool MultiDomainDeferredActivationTracker::activateMetricGroupsDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) {

    // Activation: postpone until zetMetricStreamerOpen or zeCommandQueueExecuteCommandLists
    // Deactivation: execute immediately.
    if (phMetricGroups == nullptr) {
        deActivateAllDomains();
        return true;
    }

    auto isMetricGroupProvided = [phMetricGroups, count](const zet_metric_group_handle_t hMetricGroup) {
        for (auto index = 0u; index < count; index++) {
            if (hMetricGroup == phMetricGroups[index]) {
                return true;
            }
        }
        return false;
    };

    // Deactive existing metric groups which are not provided in phMetricGroups
    std::vector<uint32_t> deactivateList = {};
    for (const auto &[domainId, metricGroupPair] : domains) {
        const auto &hMetricGroup = metricGroupPair.first;
        if (isMetricGroupProvided(hMetricGroup) == false) {
            deActivateDomain(domainId);
            deactivateList.push_back(domainId);
        }
    }

    // Remove deactivated ones from the map
    for (const auto &domainId : deactivateList) {
        domains.erase(domainId);
    }

    // Activate-deferred new metric groups if any
    for (auto index = 0u; index < count; index++) {

        zet_metric_group_handle_t hMetricGroup = MetricGroup::fromHandle(phMetricGroups[index])->getMetricGroupForSubDevice(subDeviceIndex);
        zet_metric_group_properties_t properties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        auto metricGroup = MetricGroup::fromHandle(hMetricGroup);
        metricGroup->getProperties(&properties);
        auto domain = properties.domain;

        // Domain already associated with the same handle.
        if (domains[domain].first == hMetricGroup) {
            continue;
        }

        domains[domain].first = hMetricGroup;
        domains[domain].second = false;
    }
    return true;
}

ze_result_t MultiDomainDeferredActivationTracker::activateMetricGroupsAlreadyDeferred() {
    for (auto &entry : domains) {
        auto &metricGroupEntry = entry.second;
        DEBUG_BREAK_IF(metricGroupEntry.first == nullptr);
        auto metricGroup = MetricGroup::fromHandle(metricGroupEntry.first);
        metricGroup->activate();
        metricGroupEntry.second = true;
    }
    return ZE_RESULT_SUCCESS;
}

void MultiDomainDeferredActivationTracker::deActivateDomain(uint32_t domain) {
    auto &metricGroupPair = domains[domain];
    if (metricGroupPair.second == true) {
        MetricGroup::fromHandle(metricGroupPair.first)->deactivate();
    }
}

void MultiDomainDeferredActivationTracker::deActivateAllDomains() {
    for (auto &entry : domains) {
        deActivateDomain(entry.first);
    }
    domains.clear();
}

bool MultiDomainDeferredActivationTracker::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
    for (auto const &entry : domains) {
        auto const &metricGroup = entry.second;
        if (metricGroup.first == hMetricGroup) {
            return true;
        }
    }
    return false;
}

bool MultiDomainDeferredActivationTracker::isMetricGroupActivatedInHw() const {
    for (auto const &entry : domains) {
        auto const &metricGroup = entry.second;
        if (metricGroup.second == true) {
            return true;
        }
    }
    return false;
}

void MetricCollectorEventNotify::attachEvent(ze_event_handle_t hEvent) {
    // Associate L0 notification event with metric notification.
    pNotificationEvent = Event::fromHandle(hEvent);
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->setMetricNotification(this);
    }
}

void MetricCollectorEventNotify::detachEvent() {
    // Remove association to L0 event.
    if (pNotificationEvent != nullptr) {
        pNotificationEvent->setMetricNotification(nullptr);
    }
}

void MetricGroupUserDefined::updateErrorString(std::string &errorString, size_t *errorStringSize, char *pErrorString) {
    if (*errorStringSize == 0) {
        *errorStringSize = errorString.length() + 1;
    } else {
        *errorStringSize = std::min(*errorStringSize, errorString.length() + 1);
        strncpy_s(pErrorString, *errorStringSize, errorString.data(), *errorStringSize);
    }
}

MetricProgrammable *HomogeneousMultiDeviceMetricProgrammable::create(MetricSource &metricSource,
                                                                     std::vector<MetricProgrammable *> &subDeviceProgrammables) {
    return static_cast<MetricProgrammable *>(new (std::nothrow) HomogeneousMultiDeviceMetricProgrammable(metricSource, subDeviceProgrammables));
}

ze_result_t HomogeneousMultiDeviceMetricProgrammable::getProperties(zet_metric_programmable_exp_properties_t *pProperties) {
    return subDeviceProgrammables[0]->getProperties(pProperties);
}

ze_result_t HomogeneousMultiDeviceMetricProgrammable::getParamInfo(uint32_t *pParameterCount, zet_metric_programmable_param_info_exp_t *pParameterInfo) {
    return subDeviceProgrammables[0]->getParamInfo(pParameterCount, pParameterInfo);
}

ze_result_t HomogeneousMultiDeviceMetricProgrammable::getParamValueInfo(uint32_t parameterOrdinal, uint32_t *pValueInfoCount,
                                                                        zet_metric_programmable_param_value_info_exp_t *pValueInfo) {
    return subDeviceProgrammables[0]->getParamValueInfo(parameterOrdinal, pValueInfoCount, pValueInfo);
}

ze_result_t HomogeneousMultiDeviceMetricProgrammable::createMetric(zet_metric_programmable_param_value_exp_t *pParameterValues,
                                                                   uint32_t parameterCount, const char name[ZET_MAX_METRIC_NAME],
                                                                   const char description[ZET_MAX_METRIC_DESCRIPTION],
                                                                   uint32_t *pMetricHandleCount, zet_metric_handle_t *phMetricHandles) {

    auto isCountEstimationPath = *pMetricHandleCount == 0;
    ze_result_t status = ZE_RESULT_SUCCESS;

    uint32_t expectedMetricHandleCount = 0;
    auto isExpectedHandleCount = [&](const uint32_t actualHandleCount) {
        if (expectedMetricHandleCount != 0 && expectedMetricHandleCount != actualHandleCount) {
            METRICS_LOG_ERR("Unexpected Metric Handle Count for subdevice expected:%d, actual:%d", expectedMetricHandleCount, actualHandleCount);
            return false;
        }
        expectedMetricHandleCount = actualHandleCount;
        return true;
    };

    if (isCountEstimationPath) {
        for (auto &subDeviceProgrammable : subDeviceProgrammables) {
            uint32_t metricHandleCount = 0;
            status = subDeviceProgrammable->createMetric(pParameterValues, parameterCount, name, description, &metricHandleCount, nullptr);
            if (status != ZE_RESULT_SUCCESS || metricHandleCount == 0u) {
                *pMetricHandleCount = 0;
                return status;
            }
            if (!isExpectedHandleCount(metricHandleCount)) {
                *pMetricHandleCount = 0;
                return ZE_RESULT_ERROR_UNKNOWN;
            }
        }
        *pMetricHandleCount = expectedMetricHandleCount;
        return status;
    }

    std::vector<std::vector<zet_metric_handle_t>> metricHandlesPerSubDeviceList{};
    auto cleanupApi = [&]() {
        for (auto &metricHandlesPerSubDevice : metricHandlesPerSubDeviceList) {
            for (auto &metricHandle : metricHandlesPerSubDevice) {
                if (metricHandle != nullptr) {
                    [[maybe_unused]] auto status = static_cast<MetricImp *>(metricHandle)->destroy();
                    DEBUG_BREAK_IF(status != ZE_RESULT_SUCCESS);
                }
            }
            metricHandlesPerSubDevice.clear();
        }
        metricHandlesPerSubDeviceList.clear();
    };

    metricHandlesPerSubDeviceList.resize(subDeviceProgrammables.size());
    for (uint32_t index = 0; index < static_cast<uint32_t>(subDeviceProgrammables.size()); index++) {
        auto &subDeviceProgrammable = subDeviceProgrammables[index];
        uint32_t metricHandleCount = *pMetricHandleCount;

        metricHandlesPerSubDeviceList[index].resize(metricHandleCount);
        status = subDeviceProgrammable->createMetric(pParameterValues, parameterCount, name, description, &metricHandleCount, metricHandlesPerSubDeviceList[index].data());
        if (status != ZE_RESULT_SUCCESS || metricHandleCount == 0u) {
            cleanupApi();
            *pMetricHandleCount = 0;
            return status;
        }
        if (!isExpectedHandleCount(metricHandleCount)) {
            cleanupApi();
            *pMetricHandleCount = 0;
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        metricHandlesPerSubDeviceList[index].resize(metricHandleCount);
    }

    DEBUG_BREAK_IF(metricHandlesPerSubDeviceList[0].size() > *pMetricHandleCount);

    // Create Root device Metric handles from sub-device handles
    for (uint32_t index = 0; index < static_cast<uint32_t>(metricHandlesPerSubDeviceList[0].size()); index++) {
        std::vector<MetricImp *> homogenousMetricList{};
        homogenousMetricList.reserve(subDeviceProgrammables.size());
        for (auto &metricHandlesPerSubdevice : metricHandlesPerSubDeviceList) {
            homogenousMetricList.push_back(static_cast<MetricImp *>(Metric::fromHandle(metricHandlesPerSubdevice[index])));
        }
        phMetricHandles[index] = HomogeneousMultiDeviceMetricCreated::create(metricSource, homogenousMetricList)->toHandle();
    }

    *pMetricHandleCount = static_cast<uint32_t>(metricHandlesPerSubDeviceList[0].size());
    return ZE_RESULT_SUCCESS;
}

ze_result_t HomogeneousMultiDeviceMetricCreated::destroy() {

    auto status = ZE_RESULT_SUCCESS;
    for (auto &subDeviceMetric : subDeviceMetrics) {
        ze_result_t subDeviceStatus = subDeviceMetric->destroy();
        // Hold the first error
        if (status == ZE_RESULT_SUCCESS) {
            status = subDeviceStatus;
        }
    }

    // release only if object is unused
    if (status != ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE) {
        subDeviceMetrics.clear();
        delete this;
    }
    return status;
}

MetricImp *HomogeneousMultiDeviceMetricCreated::create(MetricSource &metricSource, std::vector<MetricImp *> &subDeviceMetrics) {
    return new (std::nothrow) HomogeneousMultiDeviceMetricCreated(metricSource, subDeviceMetrics);
}

ze_result_t metricProgrammableGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) {
    auto device = Device::fromHandle(hDevice);
    return static_cast<MetricDeviceContext &>(device->getMetricDeviceContext()).metricProgrammableGet(pCount, phMetricProgrammables);
}

ze_result_t metricProgrammableGetProperties(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_exp_properties_t *pProperties) {
    return L0::MetricProgrammable::fromHandle(hMetricProgrammable)->getProperties(pProperties);
}

ze_result_t metricProgrammableGetParamInfo(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t *pParameterCount,
    zet_metric_programmable_param_info_exp_t *pParameterInfo) {
    return L0::MetricProgrammable::fromHandle(hMetricProgrammable)->getParamInfo(pParameterCount, pParameterInfo);
}

ze_result_t metricProgrammableGetParamValueInfo(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    uint32_t parameterOrdinal,
    uint32_t *pValueInfoCount,
    zet_metric_programmable_param_value_info_exp_t *pValueInfo) {
    return L0::MetricProgrammable::fromHandle(hMetricProgrammable)->getParamValueInfo(parameterOrdinal, pValueInfoCount, pValueInfo);
}

ze_result_t metricCreateFromProgrammable(
    zet_metric_programmable_exp_handle_t hMetricProgrammable,
    zet_metric_programmable_param_value_exp_t *pParameterValues,
    uint32_t parameterCount,
    const char name[ZET_MAX_METRIC_NAME],
    const char description[ZET_MAX_METRIC_DESCRIPTION],
    uint32_t *pMetricHandleCount,
    zet_metric_handle_t *phMetricHandles) {
    return L0::MetricProgrammable::fromHandle(hMetricProgrammable)->createMetric(pParameterValues, parameterCount, name, description, pMetricHandleCount, phMetricHandles);
}

ze_result_t metricCalculateOperationCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    zet_intel_metric_calculate_exp_desc_t *pCalculateDesc,
    uint32_t *pExcludedMetricCount,
    zet_metric_handle_t *phExcludedMetrics,
    zet_intel_metric_calculate_operation_exp_handle_t *phCalculateOperation) {

    DeviceImp *deviceImp = static_cast<DeviceImp *>(L0::Device::fromHandle(hDevice));
    return deviceImp->getMetricDeviceContext().calcOperationCreate(hContext, pCalculateDesc, pExcludedMetricCount, phExcludedMetrics, phCalculateOperation);
}

ze_result_t metricCalculateOperationDestroy(
    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation) {
    return MetricCalcOp::fromHandle(hCalculateOperation)->destroy();
}

ze_result_t metricCalculateGetReportFormat(
    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation,
    uint32_t *pCount,
    zet_metric_handle_t *phMetrics) {
    return MetricCalcOp::fromHandle(hCalculateOperation)->getReportFormat(pCount, phMetrics);
}

ze_result_t metricCalculateValues(
    const size_t rawDataSize,
    size_t *pOffset,
    const uint8_t *pRawData,
    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation,
    uint32_t *pTotalMetricReportsCount,
    zet_intel_metric_result_exp_t *pMetricResults) {
    return MetricCalcOp::fromHandle(hCalculateOperation)->metricCalculateValues(rawDataSize, pOffset, pRawData, pTotalMetricReportsCount, pMetricResults);
}

ze_result_t metricCalculateMultipleValues(
    const size_t rawDataSize,
    size_t *offset,
    const uint8_t *pRawData,
    zet_intel_metric_calculate_operation_exp_handle_t hCalculateOperation,
    uint32_t *pSetCount,
    uint32_t *pMetricsReportCountPerSet,
    uint32_t *pTotalMetricReportCount,
    zet_intel_metric_result_exp_t *pMetricResults) {
    return MetricCalcOp::fromHandle(hCalculateOperation)->metricCalculateMultipleValues(rawDataSize, offset, pRawData, pSetCount, pMetricsReportCountPerSet, pTotalMetricReportCount, pMetricResults);
}

ze_result_t metricsEnable(zet_device_handle_t hDevice) {
    auto isFailed = false;

    MetricDeviceContext::enableMetricApiForDevice(hDevice, isFailed);
    return isFailed ? ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE : ZE_RESULT_SUCCESS;
}

ze_result_t metricsDisable(zet_device_handle_t hDevice) {
    return MetricDeviceContext::disableMetricApiForDevice(hDevice);
}

} // namespace L0
