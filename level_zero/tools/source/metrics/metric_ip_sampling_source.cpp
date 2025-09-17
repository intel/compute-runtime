/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric.inl"
#include "level_zero/tools/source/metrics/metric_ip_sampling_streamer.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/zet_intel_gpu_metric.h"
#include "level_zero/zet_intel_gpu_metric_export.h"
#include <level_zero/zet_api.h>

#include <cstring>
#include <unordered_set>

namespace L0 {
constexpr uint32_t ipSamplinDomainId = 100u;

std::unique_ptr<IpSamplingMetricSourceImp> IpSamplingMetricSourceImp::create(const MetricDeviceContext &metricDeviceContext) {
    return std::unique_ptr<IpSamplingMetricSourceImp>(new (std::nothrow) IpSamplingMetricSourceImp(metricDeviceContext));
}

IpSamplingMetricSourceImp::IpSamplingMetricSourceImp(const MetricDeviceContext &metricDeviceContext) : metricDeviceContext(metricDeviceContext) {
    metricIPSamplingpOsInterface = MetricIpSamplingOsInterface::create(metricDeviceContext.getDevice());
    activationTracker = std::make_unique<MultiDomainDeferredActivationTracker>(metricDeviceContext.getSubDeviceIndex());
    type = MetricSource::metricSourceTypeIpSampling;

    const auto deviceImp = static_cast<DeviceImp *>(&metricDeviceContext.getDevice());
    L0::L0GfxCoreHelper &l0GfxCoreHelper = deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    ipSamplingCalculation = std::make_unique<IpSamplingCalculation>(l0GfxCoreHelper, *this);
}

ze_result_t IpSamplingMetricSourceImp::getTimerResolution(uint64_t &resolution) {
    resolution = metricDeviceContext.getDevice().getNEODevice()->getDeviceInfo().outProfilingTimerClock;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::getTimestampValidBits(uint64_t &validBits) {
    validBits = metricDeviceContext.getDevice().getNEODevice()->getHardwareInfo().capabilityTable.timestampValidBits;
    return ZE_RESULT_SUCCESS;
}

void IpSamplingMetricSourceImp::enable() {
    isEnabled = metricIPSamplingpOsInterface->isDependencyAvailable();
}

bool IpSamplingMetricSourceImp::isAvailable() {
    return isEnabled;
}

ze_result_t IpSamplingMetricSourceImp::cacheMetricGroup() {

    const auto deviceImp = static_cast<DeviceImp *>(&metricDeviceContext.getDevice());
    if (metricDeviceContext.isImplicitScalingCapable()) {
        std::vector<IpSamplingMetricGroupImp *> subDeviceMetricGroup = {};
        subDeviceMetricGroup.reserve(deviceImp->subDevices.size());

        // Prepare cached metric group for sub-devices
        for (auto &subDevice : deviceImp->subDevices) {
            IpSamplingMetricSourceImp &source = subDevice->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
            // 1 metric group available for IP Sampling
            uint32_t count = 1;
            zet_metric_group_handle_t hMetricGroup = {};
            const auto result = source.metricGroupGet(&count, &hMetricGroup);
            if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
                return result;
            }
            // Getting MetricGroup from sub-device cannot fail, since RootDevice is successful
            UNRECOVERABLE_IF(result != ZE_RESULT_SUCCESS);
            subDeviceMetricGroup.push_back(static_cast<IpSamplingMetricGroupImp *>(MetricGroup::fromHandle(hMetricGroup)));
        }

        IpSamplingMetricSourceImp &source = deviceImp->getMetricDeviceContext().getMetricSource<IpSamplingMetricSourceImp>();
        cachedMetricGroup = MultiDeviceIpSamplingMetricGroupImp::create(source, subDeviceMetricGroup);
        return ZE_RESULT_SUCCESS;
    }

    // Confirm whether sample collection is possible
    uint32_t referenceValues = 100;
    const ze_result_t sampleCheckResult = getMetricOsInterface()->startMeasurement(referenceValues, referenceValues);
    if (sampleCheckResult != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else {
        getMetricOsInterface()->stopMeasurement();
    }

    std::vector<IpSamplingMetricImp> metrics = {};
    auto &l0GfxCoreHelper = deviceImp->getNEODevice()->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    metrics.reserve(l0GfxCoreHelper.getIpSamplingMetricCount());
    metricSourceCount = l0GfxCoreHelper.getIpSamplingMetricCount();

    zet_metric_properties_t metricProperties = {};

    metricProperties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
    metricProperties.pNext = nullptr;
    strcpy_s(metricProperties.component, ZET_MAX_METRIC_COMPONENT, "XVE");
    metricProperties.tierNumber = 4;
    metricProperties.resultType = ZET_VALUE_TYPE_UINT64;

    // Preparing properties for IP separately because of unique values
    strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, "IP");
    strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, "IP address");
    metricProperties.metricType = ZET_METRIC_TYPE_IP;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Address");
    metrics.push_back(IpSamplingMetricImp(*this, metricProperties));

    std::vector<std::pair<const char *, const char *>> stallSamplingReportList = l0GfxCoreHelper.getStallSamplingReportMetrics();

    // Preparing properties for others because of common values
    metricProperties.metricType = ZET_METRIC_TYPE_EVENT;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Events");

    for (auto &property : stallSamplingReportList) {
        strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, property.first);
        strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, property.second);
        metrics.push_back(IpSamplingMetricImp(*this, metricProperties));
    }

    cachedMetricGroup = IpSamplingMetricGroupImp::create(*this, metrics);
    DEBUG_BREAK_IF(cachedMetricGroup == nullptr);

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {

    if (!isEnabled) {
        *pCount = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (cachedMetricGroup == nullptr) {
        auto status = cacheMetricGroup();

        if (status != ZE_RESULT_SUCCESS || cachedMetricGroup == nullptr) {
            *pCount = 0;
            isEnabled = false;
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }

    DEBUG_BREAK_IF(phMetricGroups == nullptr);
    phMetricGroups[0] = cachedMetricGroup->toHandle();
    *pCount = 1;

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    METRICS_LOG_ERR("%s", "Memory barrier not supported for IP Sampling");
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t IpSamplingMetricSourceImp::activateMetricGroupsPreferDeferred(uint32_t count,
                                                                          zet_metric_group_handle_t *phMetricGroups) {

    DeviceImp &deviceImp = static_cast<DeviceImp &>(metricDeviceContext.getDevice());
    if (metricDeviceContext.isImplicitScalingCapable()) {
        return MetricSource::activatePreferDeferredHierarchical<IpSamplingMetricSourceImp>(&deviceImp, count, phMetricGroups);
    }

    auto status = activationTracker->activateMetricGroupsDeferred(count, phMetricGroups);
    if (!status) {
        METRICS_LOG_ERR("%s", "Metric group activation failed");
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::activateMetricGroupsAlreadyDeferred() {
    return activationTracker->activateMetricGroupsAlreadyDeferred();
}

bool IpSamplingMetricSourceImp::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
    return activationTracker->isMetricGroupActivated(hMetricGroup);
}

void IpSamplingMetricSourceImp::setMetricOsInterface(std::unique_ptr<MetricIpSamplingOsInterface> &metricIPSamplingpOsInterface) {
    this->metricIPSamplingpOsInterface = std::move(metricIPSamplingpOsInterface);
}

ze_result_t IpSamplingMetricGroupBase::getExportData(const uint8_t *pRawData, size_t rawDataSize, size_t *pExportDataSize,
                                                     uint8_t *pExportData) {
    const auto expectedExportDataSize = sizeof(zet_intel_metric_df_gpu_export_data_format_t) + rawDataSize;

    if (*pExportDataSize == 0u) {
        *pExportDataSize = expectedExportDataSize;
        return ZE_RESULT_SUCCESS;
    }

    if (*pExportDataSize < expectedExportDataSize) {
        METRICS_LOG_ERR("Incorrect Size Passed. Returning 0x%x", ZE_RESULT_ERROR_INVALID_SIZE);
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    zet_intel_metric_df_gpu_export_data_format_t *exportData = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(pExportData);
    exportData->header.type = ZET_INTEL_METRIC_DF_SOURCE_TYPE_IPSAMPLING;
    exportData->header.version.major = ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MAJOR;
    exportData->header.version.minor = ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MINOR;
    exportData->header.rawDataOffset = sizeof(zet_intel_metric_df_gpu_export_data_format_t);
    exportData->header.rawDataSize = rawDataSize;

    // Append the rawData
    memcpy_s(reinterpret_cast<void *>(pExportData + exportData->header.rawDataOffset), rawDataSize, pRawData, rawDataSize);

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
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

ze_result_t IpSamplingMetricSourceImp::handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup,
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

            getTimerResolution(metricsTimestampProperties->timerResolution);
            getTimestampValidBits(metricsTimestampProperties->timestampValidBits);
            retVal = ZE_RESULT_SUCCESS;
        } else if (extendedProperties->stype == ZET_STRUCTURE_TYPE_METRIC_GROUP_TYPE_EXP) {
            zet_metric_group_type_exp_t *groupType = reinterpret_cast<zet_metric_group_type_exp_t *>(extendedProperties);
            groupType->type = ZET_METRIC_GROUP_TYPE_EXP_FLAG_OTHER;
            retVal = ZE_RESULT_SUCCESS;
        } else if (static_cast<uint32_t>(extendedProperties->stype) == ZET_INTEL_STRUCTURE_TYPE_METRIC_GROUP_CALCULATION_EXP_PROPERTIES) {
            auto calcProperties = reinterpret_cast<zet_intel_metric_group_calculation_properties_exp_t *>(extendedProperties);
            calcProperties->isTimeFilterSupported = false;
            retVal = ZE_RESULT_SUCCESS;
        }

        pNext = extendedProperties->pNext;
    }

    return retVal;
}

ze_result_t IpSamplingMetricSourceImp::calcOperationCreate(MetricDeviceContext &metricDeviceContext,
                                                           zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
                                                           const std::vector<MetricScopeImp *> &metricScopes,
                                                           zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation) {
    ze_result_t status = ZE_RESULT_ERROR_UNKNOWN;

    bool isMultiDevice = (metricDeviceContext.isImplicitScalingCapable()) ? true : false;
    status = IpSamplingMetricCalcOpImp::create(isMultiDevice, metricScopes, *this, pCalculationDesc, phCalculationOperation);
    return status;
}

bool IpSamplingMetricSourceImp::canDisable() {
    return !activationTracker->isAnyMetricGroupActivated();
}

void IpSamplingMetricSourceImp::initMetricScopes(MetricDeviceContext &metricDeviceContext) {
    if (!metricDeviceContext.isComputeMetricScopesInitialized()) {
        initComputeMetricScopes(metricDeviceContext);
    }
}

IpSamplingMetricGroupImp::IpSamplingMetricGroupImp(IpSamplingMetricSourceImp &metricSource,
                                                   std::vector<IpSamplingMetricImp> &metrics) : IpSamplingMetricGroupBase(metricSource) {
    this->metrics.reserve(metrics.size());
    for (const auto &metric : metrics) {
        this->metrics.push_back(std::make_unique<IpSamplingMetricImp>(metric));
    }

    properties.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    properties.pNext = nullptr;
    strcpy_s(properties.name, ZET_MAX_METRIC_GROUP_NAME, "EuStallSampling");
    strcpy_s(properties.description, ZET_MAX_METRIC_GROUP_DESCRIPTION, "EU stall sampling");
    properties.samplingType = ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;
    properties.domain = ipSamplinDomainId;
    properties.metricCount = this->getMetricSource().metricSourceCount;
}

ze_result_t IpSamplingMetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {
    void *pNext = pProperties->pNext;
    *pProperties = properties;
    pProperties->pNext = pNext;

    if (pNext) {
        return metricSource.handleMetricGroupExtendedProperties(toHandle(), pProperties, pNext);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {

    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(metrics.size());
        return ZE_RESULT_SUCCESS;
    }
    // User is expected to allocate space.
    DEBUG_BREAK_IF(phMetrics == nullptr);

    *pCount = std::min(*pCount, static_cast<uint32_t>(metrics.size()));

    for (uint32_t i = 0; i < *pCount; i++) {
        phMetrics[i] = metrics[i]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

bool IpSamplingMetricGroupBase::isMultiDeviceCaptureData(const size_t rawDataSize, const uint8_t *pRawData) {
    if (rawDataSize >= sizeof(IpSamplingMetricDataHeader)) {
        const auto header = reinterpret_cast<const IpSamplingMetricDataHeader *>(pRawData);
        return header->magic == IpSamplingMetricDataHeader::magicValue;
    }
    return false;
}

ze_result_t IpSamplingMetricGroupImp::calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                            const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                            zet_typed_value_t *pMetricValues) {
    return getMetricSource().ipSamplingCalculation->calculateMetricForSubdevice(type, rawDataSize, pRawData, pMetricValueCount, pMetricValues);
}

ze_result_t IpSamplingMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                               const uint8_t *pRawData, uint32_t *pSetCount,
                                                               uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                               zet_typed_value_t *pMetricValues) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    const bool calculateCountOnly = (*pTotalMetricValueCount == 0) || (*pSetCount == 0);
    if (calculateCountOnly) {
        *pTotalMetricValueCount = 0;
        *pSetCount = 0;
    }

    if (!isMultiDeviceCaptureData(rawDataSize, pRawData)) {
        result = this->calculateMetricValues(type, rawDataSize, pRawData, pTotalMetricValueCount, pMetricValues);
    } else {
        METRICS_LOG_ERR("%s", "Calculating sub-device results using root device captured data is not supported");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if ((result == ZE_RESULT_SUCCESS) || (result == ZE_RESULT_WARNING_DROPPED_DATA)) {
        *pSetCount = 1;
        if (!calculateCountOnly) {
            pMetricCounts[0] = *pTotalMetricValueCount;
        }
    } else if (!calculateCountOnly) {
        pMetricCounts[0] = 0;
    }

    return result;
}

ze_result_t getDeviceTimestamps(DeviceImp *deviceImp, const ze_bool_t synchronizedWithHost,
                                uint64_t *globalTimestamp, uint64_t *metricTimestamp) {

    ze_result_t result;
    uint64_t hostTimestamp;
    uint64_t deviceTimestamp;

    result = deviceImp->getGlobalTimestamps(&hostTimestamp, &deviceTimestamp);
    if (result != ZE_RESULT_SUCCESS) {
        *globalTimestamp = 0;
        *metricTimestamp = 0;
    } else {
        if (synchronizedWithHost) {
            *globalTimestamp = hostTimestamp;
        } else {
            *globalTimestamp = deviceTimestamp;
        }
        *metricTimestamp = deviceTimestamp;
        result = ZE_RESULT_SUCCESS;
    }

    return result;
}

ze_result_t IpSamplingMetricGroupImp::getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                                             uint64_t *globalTimestamp,
                                                             uint64_t *metricTimestamp) {
    DeviceImp *deviceImp = static_cast<DeviceImp *>(&getMetricSource().getMetricDeviceContext().getDevice());
    return getDeviceTimestamps(deviceImp, synchronizedWithHost, globalTimestamp, metricTimestamp);
}

zet_metric_group_handle_t IpSamplingMetricGroupImp::getMetricGroupForSubDevice(const uint32_t subDeviceIndex) {
    return toHandle();
}

std::unique_ptr<IpSamplingMetricGroupImp> IpSamplingMetricGroupImp::create(IpSamplingMetricSourceImp &metricSource,
                                                                           std::vector<IpSamplingMetricImp> &ipSamplingMetrics) {
    return std::unique_ptr<IpSamplingMetricGroupImp>(new (std::nothrow) IpSamplingMetricGroupImp(metricSource, ipSamplingMetrics));
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {
    return subDeviceMetricGroup[0]->getProperties(pProperties);
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {
    return subDeviceMetricGroup[0]->metricGet(pCount, phMetrics);
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                                       const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                                       zet_typed_value_t *pMetricValues) {

    if (IpSamplingCalculation::isMultiDeviceCaptureData(rawDataSize, pRawData)) {
        METRICS_LOG_ERR("%s", "The API is not supported for root device captured data");
        METRICS_LOG_ERR("%s", "Please use zetMetricGroupCalculateMultipleMetricValuesExp instead");
    } else {
        METRICS_LOG_ERR("%s", "Cannot validate root device captured data. Input size or captured data are invalid");
    }

    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                                          const uint8_t *pRawData, uint32_t *pSetCount,
                                                                          uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                                          zet_typed_value_t *pMetricValues) {

    const bool calculateCountOnly = *pSetCount == 0 || *pTotalMetricValueCount == 0;
    bool isDroppedData = false;
    ze_result_t result = ZE_RESULT_SUCCESS;

    auto calcUtils = getMetricSource().ipSamplingCalculation.get();
    if (calculateCountOnly) {
        *pSetCount = 0;
        *pTotalMetricValueCount = 0;
        for (uint32_t setIndex = 0; setIndex < subDeviceMetricGroup.size(); setIndex++) {
            uint32_t currTotalMetricValueCount = 0;
            result = calcUtils->getMetricCountSubDevIndex(pRawData, rawDataSize, currTotalMetricValueCount, setIndex);
            if (result != ZE_RESULT_SUCCESS) {
                return result;
            }
            *pTotalMetricValueCount += currTotalMetricValueCount;
        }
        *pSetCount = static_cast<uint32_t>(subDeviceMetricGroup.size());
    } else {
        memset(pMetricCounts, 0, *pSetCount);
        const auto maxSets = std::min<uint32_t>(static_cast<uint32_t>(subDeviceMetricGroup.size()), *pSetCount);

        auto tempTotalMetricValueCount = *pTotalMetricValueCount;
        for (uint32_t setIndex = 0; setIndex < maxSets; setIndex++) {
            uint32_t currTotalMetricValueCount = tempTotalMetricValueCount;
            result = calcUtils->calculateMetricValuesSubDevIndex(type, rawDataSize, pRawData, currTotalMetricValueCount, pMetricValues, setIndex);
            if (result != ZE_RESULT_SUCCESS) {
                if (result == ZE_RESULT_WARNING_DROPPED_DATA) {
                    isDroppedData = true;
                } else {
                    memset(pMetricCounts, 0, *pSetCount);
                    return result;
                }
            }

            pMetricCounts[setIndex] = currTotalMetricValueCount;
            pMetricValues += currTotalMetricValueCount;
            tempTotalMetricValueCount -= currTotalMetricValueCount;
        }
        *pTotalMetricValueCount -= tempTotalMetricValueCount;
    }

    return isDroppedData ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

zet_metric_group_handle_t MultiDeviceIpSamplingMetricGroupImp::getMetricGroupForSubDevice(const uint32_t subDeviceIndex) {
    return subDeviceMetricGroup[subDeviceIndex]->toHandle();
}

void MultiDeviceIpSamplingMetricGroupImp::closeSubDeviceStreamers(std::vector<IpSamplingMetricStreamerImp *> &subDeviceStreamers) {
    for (auto streamer : subDeviceStreamers) {
        streamer->close();
    }
}

ze_result_t MultiDeviceIpSamplingMetricGroupImp::getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                                                        uint64_t *globalTimestamp,
                                                                        uint64_t *metricTimestamp) {
    DeviceImp *deviceImp = static_cast<DeviceImp *>(&subDeviceMetricGroup[0]->getMetricSource().getMetricDeviceContext().getDevice());
    return getDeviceTimestamps(deviceImp, synchronizedWithHost, globalTimestamp, metricTimestamp);
}

std::unique_ptr<MultiDeviceIpSamplingMetricGroupImp> MultiDeviceIpSamplingMetricGroupImp::create(
    MetricSource &metricSource,
    std::vector<IpSamplingMetricGroupImp *> &subDeviceMetricGroup) {
    UNRECOVERABLE_IF(subDeviceMetricGroup.size() == 0);
    return std::unique_ptr<MultiDeviceIpSamplingMetricGroupImp>(new (std::nothrow) MultiDeviceIpSamplingMetricGroupImp(metricSource, subDeviceMetricGroup));
}

IpSamplingMetricImp::IpSamplingMetricImp(MetricSource &metricSource, zet_metric_properties_t &properties) : MetricImp(metricSource), properties(properties) {
}

ze_result_t IpSamplingMetricImp::getProperties(zet_metric_properties_t *pProperties) {
    auto pNext = pProperties->pNext;
    *pProperties = properties;

    while (pNext != nullptr) {
        auto extendedProperties = reinterpret_cast<zet_base_properties_t *>(pNext);
        if (static_cast<uint32_t>(extendedProperties->stype) == ZET_INTEL_STRUCTURE_TYPE_METRIC_CALCULABLE_PROPERTIES_EXP) {
            auto calculableProperties = reinterpret_cast<zet_intel_metric_calculable_properties_exp_t *>(extendedProperties);
            // All metrics are calculable in IP Sampling
            calculableProperties->isCalculable = true;
        }
        pNext = extendedProperties->pNext;
    }

    return ZE_RESULT_SUCCESS;
}

bool IpSamplingCalculation::isMultiDeviceCaptureData(const size_t rawDataSize, const uint8_t *pRawData) {
    if (rawDataSize >= sizeof(IpSamplingMetricDataHeader)) {
        const auto header = reinterpret_cast<const IpSamplingMetricDataHeader *>(pRawData);
        return header->magic == IpSamplingMetricDataHeader::magicValue;
    }
    return false;
}

ze_result_t IpSamplingCalculation::getMetricCount(const uint8_t *pRawData, const size_t rawDataSize,
                                                  uint32_t &metricValueCount) {

    std::unordered_set<uint64_t> stallReportIpCount{};
    constexpr uint32_t rawReportSize = IpSamplingMetricGroupBase::rawReportSize;

    if ((rawDataSize % rawReportSize) != 0) {
        METRICS_LOG_ERR("%s", "Invalid input raw data size");
        metricValueCount = 0;
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / rawReportSize;

    for (const uint8_t *pRawIpData = pRawData; pRawIpData < pRawData + (rawReportCount * rawReportSize); pRawIpData += rawReportSize) {
        uint64_t ip = 0ULL;
        memcpy_s(reinterpret_cast<uint8_t *>(&ip), sizeof(ip), pRawIpData, sizeof(ip));
        ip &= gfxCoreHelper.getIpSamplingIpMask();
        stallReportIpCount.insert(ip);
    }

    metricValueCount = static_cast<uint32_t>(stallReportIpCount.size()) * metricSource.metricSourceCount;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingCalculation::getMetricCountSubDevIndex(const uint8_t *pMultiMetricData, const size_t rawDataSize, uint32_t &metricValueCount, const uint32_t setIndex) {

    // Iterate through headers and assign required sizes
    auto processedSize = 0u;
    while (processedSize < rawDataSize) {
        auto processMetricData = pMultiMetricData + processedSize;
        if (!isMultiDeviceCaptureData(rawDataSize - processedSize, processMetricData)) {
            METRICS_LOG_ERR("%s", "Cannot validate root device captured data. Input size or captured data are invalid");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        auto header = reinterpret_cast<const IpSamplingMetricDataHeader *>(processMetricData);
        processedSize += sizeof(IpSamplingMetricDataHeader) + header->rawDataSize;
        if (header->setIndex != setIndex) {
            continue;
        }

        auto currTotalMetricValueCount = 0u;
        auto result = getMetricCount((processMetricData + sizeof(IpSamplingMetricDataHeader)), header->rawDataSize, currTotalMetricValueCount);
        if (result != ZE_RESULT_SUCCESS) {
            metricValueCount = 0;
            return result;
        }
        metricValueCount += currTotalMetricValueCount;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingCalculation::calculateMetricValues(const zet_metric_group_calculation_type_t type, const size_t rawDataSize,
                                                         const uint8_t *pRawData, uint32_t &metricValueCount,
                                                         zet_typed_value_t *pCalculatedData) {
    bool dataOverflow = false;
    std::map<uint64_t, void *> stallReportDataMap;

    // MAX_METRIC_VALUES is not supported yet.
    if (type != ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES) {
        METRICS_LOG_ERR("%s", "IP sampling only supports ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES");
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    DEBUG_BREAK_IF(pCalculatedData == nullptr);

    const uint32_t rawReportSize = IpSamplingMetricGroupBase::rawReportSize;

    if ((rawDataSize % rawReportSize) != 0) {
        METRICS_LOG_ERR("%s", "Invalid input raw data size");
        metricValueCount = 0;
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / rawReportSize;

    for (const uint8_t *pRawIpData = pRawData; pRawIpData < pRawData + (rawReportCount * rawReportSize); pRawIpData += rawReportSize) {
        dataOverflow |= gfxCoreHelper.stallIpDataMapUpdate(stallReportDataMap, pRawIpData);
    }

    metricValueCount = std::min<uint32_t>(metricValueCount, static_cast<uint32_t>(stallReportDataMap.size()) * metricSource.metricSourceCount);
    std::vector<zet_typed_value_t> ipDataValues;
    uint32_t i = 0;
    for (auto it = stallReportDataMap.begin(); it != stallReportDataMap.end(); ++it) {
        gfxCoreHelper.stallSumIpDataToTypedValues(it->first, it->second, ipDataValues);
        for (auto jt = ipDataValues.begin(); (jt != ipDataValues.end()) && (i < metricValueCount); jt++, i++) {
            *(pCalculatedData + i) = *jt;
        }
        ipDataValues.clear();
    }
    gfxCoreHelper.stallIpDataMapDelete(stallReportDataMap);
    stallReportDataMap.clear();

    return dataOverflow ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingCalculation::calculateMetricValuesSubDevIndex(const zet_metric_group_calculation_type_t type, const size_t rawDataSize,
                                                                    const uint8_t *pMultiMetricData, uint32_t &metricValueCount,
                                                                    zet_typed_value_t *pCalculatedData, const uint32_t setIndex) {

    auto processedSize = 0u;
    auto isDataDropped = false;
    auto requestTotalMetricValueCount = metricValueCount;

    while (processedSize < rawDataSize && requestTotalMetricValueCount > 0) {
        auto processMetricData = pMultiMetricData + processedSize;
        if (!isMultiDeviceCaptureData(rawDataSize - processedSize, processMetricData)) {
            METRICS_LOG_ERR("%s", "Calculating root device results using sub-device captured data is not supported");
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }

        auto header = reinterpret_cast<const IpSamplingMetricDataHeader *>(processMetricData);
        processedSize += header->rawDataSize + sizeof(IpSamplingMetricDataHeader);
        if (header->setIndex != setIndex) {
            continue;
        }

        auto processMetricRawData = processMetricData + sizeof(IpSamplingMetricDataHeader);
        auto currTotalMetricValueCount = requestTotalMetricValueCount;
        auto result = calculateMetricValues(type, header->rawDataSize, processMetricRawData, currTotalMetricValueCount, pCalculatedData);
        if (result != ZE_RESULT_SUCCESS) {
            if (result == ZE_RESULT_WARNING_DROPPED_DATA) {
                isDataDropped = true;
            } else {
                metricValueCount = 0;
                return result;
            }
        }
        pCalculatedData += currTotalMetricValueCount;
        requestTotalMetricValueCount -= currTotalMetricValueCount;
    }

    metricValueCount -= requestTotalMetricValueCount;
    return isDataDropped ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingCalculation::calculateMetricForSubdevice(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                               const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                               zet_typed_value_t *pMetricValues) {
    const bool calculateCountOnly = *pMetricValueCount == 0;

    if (isMultiDeviceCaptureData(rawDataSize, pRawData)) {
        METRICS_LOG_ERR("%s", "Calculating sub-device results using root device captured data is not supported");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (calculateCountOnly) {
        return getMetricCount(pRawData, rawDataSize, *pMetricValueCount);
    } else {
        return calculateMetricValues(type, rawDataSize, pRawData, *pMetricValueCount, pMetricValues);
    }
}

template <>
IpSamplingMetricSourceImp &MetricDeviceContext::getMetricSource<IpSamplingMetricSourceImp>() const {
    return static_cast<IpSamplingMetricSourceImp &>(*metricSources.at(MetricSource::metricSourceTypeIpSampling));
}

} // namespace L0
