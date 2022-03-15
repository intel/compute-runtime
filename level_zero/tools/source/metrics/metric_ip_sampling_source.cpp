/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_metric_ip_sampling.h"
#include <level_zero/zet_api.h>

#include <cstring>

namespace L0 {
constexpr uint32_t ipSamplinMetricCount = 10u;
constexpr uint32_t ipSamplinDomainId = 100u;

std::unique_ptr<IpSamplingMetricSourceImp> IpSamplingMetricSourceImp::create(const MetricDeviceContext &metricDeviceContext) {
    return std::unique_ptr<IpSamplingMetricSourceImp>(new (std::nothrow) IpSamplingMetricSourceImp(metricDeviceContext));
}

IpSamplingMetricSourceImp::IpSamplingMetricSourceImp(const MetricDeviceContext &metricDeviceContext) : metricDeviceContext(metricDeviceContext) {
    metricOsInterface = MetricIpSamplingOsInterface::create(metricDeviceContext.getDevice());
}

void IpSamplingMetricSourceImp::enable() {
    isEnabled = metricOsInterface->isDependencyAvailable();
}

bool IpSamplingMetricSourceImp::isAvailable() {
    return isEnabled;
}

void IpSamplingMetricSourceImp::cacheMetricGroup() {

    std::vector<IpSamplingMetricImp> metrics = {};
    metrics.reserve(ipSamplinMetricCount);

    zet_metric_properties_t metricProperties = {};

    metricProperties.stype = ZET_STRUCTURE_TYPE_METRIC_PROPERTIES;
    metricProperties.pNext = nullptr;
    strcpy_s(metricProperties.component, ZET_MAX_METRIC_COMPONENT, "XVE");
    metricProperties.tierNumber = 4;
    metricProperties.resultType = ZET_VALUE_TYPE_UINT64;

    // Preparing properties for IP seperately because of unique values
    strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, "IP");
    strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, "IP address");
    metricProperties.metricType = ZET_METRIC_TYPE_IP_EXP;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Address");
    metrics.push_back(IpSamplingMetricImp(metricProperties));

    std::vector<std::pair<const char *, const char *>> metricPropertiesList = {
        {"Active", "Active cycles"},
        {"ControlStall", "Stall on control"},
        {"PipeStall", "Stall on pipe"},
        {"SendStall", "Stall on send"},
        {"DistStall", "Stall on distance"},
        {"SbidStall", "Stall on scoreboard"},
        {"SyncStall", "Stall on sync"},
        {"InstrFetchStall", "Stall on instruction fetch"},
        {"OtherStall", "Stall on other condition"},
    };

    // Preparing properties for others because of common values
    metricProperties.metricType = ZET_METRIC_TYPE_EVENT;
    strcpy_s(metricProperties.resultUnits, ZET_MAX_METRIC_RESULT_UNITS, "Events");

    for (auto &property : metricPropertiesList) {
        strcpy_s(metricProperties.name, ZET_MAX_METRIC_NAME, property.first);
        strcpy_s(metricProperties.description, ZET_MAX_METRIC_DESCRIPTION, property.second);
        metrics.push_back(IpSamplingMetricImp(metricProperties));
    }

    cachedMetricGroup = IpSamplingMetricGroupImp::create(metrics);
    DEBUG_BREAK_IF(cachedMetricGroup == nullptr);
}

ze_result_t IpSamplingMetricSourceImp::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {

    if (!isEnabled) {
        *pCount = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (*pCount == 0) {
        *pCount = 1;
        return ZE_RESULT_SUCCESS;
    }

    if (cachedMetricGroup == nullptr) {
        cacheMetricGroup();
    }

    DEBUG_BREAK_IF(phMetricGroups == nullptr);
    phMetricGroups[0] = cachedMetricGroup->toHandle();
    *pCount = 1;

    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void IpSamplingMetricSourceImp::setMetricOsInterface(std::unique_ptr<MetricIpSamplingOsInterface> &metricOsInterface) {
    this->metricOsInterface = std::move(metricOsInterface);
}

IpSamplingMetricGroupImp::IpSamplingMetricGroupImp(std::vector<IpSamplingMetricImp> &metrics) {
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
    properties.metricCount = ipSamplinMetricCount;
}

ze_result_t IpSamplingMetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {
    *pProperties = properties;
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

ze_result_t IpSamplingMetricGroupImp::calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                            const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                            zet_typed_value_t *pMetricValues) {
    const bool calculateCountOnly = *pMetricValueCount == 0;
    if (calculateCountOnly) {
        return getCalculatedMetricCount(rawDataSize, *pMetricValueCount);
    } else {
        return getCalculatedMetricValues(type, rawDataSize, pRawData, *pMetricValueCount, pMetricValues);
    }
}

ze_result_t IpSamplingMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                               const uint8_t *pRawData, uint32_t *pSetCount,
                                                               uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                               zet_typed_value_t *pMetricValues) {
    const bool calculationCountOnly = *pTotalMetricValueCount == 0;
    ze_result_t result = this->calculateMetricValues(type, rawDataSize, pRawData, pTotalMetricValueCount, pMetricValues);

    if (result == ZE_RESULT_SUCCESS) {
        *pSetCount = 1;
        if (!calculationCountOnly) {
            pMetricCounts[0] = *pTotalMetricValueCount;
        }
    } else {
        if (calculationCountOnly) {
            *pSetCount = 0;
            *pTotalMetricValueCount = 0;
        } else {
            pMetricCounts[0] = 0;
        }
    }
    return result;
}

ze_result_t IpSamplingMetricGroupImp::getCalculatedMetricCount(const size_t rawDataSize,
                                                               uint32_t &metricValueCount) {

    uint32_t rawReportSize = 64;

    if ((rawDataSize % rawReportSize) != 0) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / rawReportSize;
    metricValueCount = rawReportCount * properties.metricCount;
    return ZE_RESULT_SUCCESS;
}

ze_result_t IpSamplingMetricGroupImp::getCalculatedMetricValues(const zet_metric_group_calculation_type_t type, const size_t rawDataSize, const uint8_t *pRawData,
                                                                uint32_t &metricValueCount,
                                                                zet_typed_value_t *pCalculatedData) {
    StallSumIpDataMap_t stallSumIpDataMap;

    // MAX_METRIC_VALUES is not supported yet.
    if (type != ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    DEBUG_BREAK_IF(pCalculatedData == nullptr);

    uint32_t rawReportSize = 64;

    if ((rawDataSize % rawReportSize) != 0) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / rawReportSize;

    for (const uint8_t *pRawIpData = pRawData; pRawIpData < pRawData + (rawReportCount * rawReportSize); pRawIpData += rawReportSize) {
        stallIpDataMapUpdate(stallSumIpDataMap, pRawIpData);
    }

    metricValueCount = std::min<uint32_t>(metricValueCount, static_cast<uint32_t>(stallSumIpDataMap.size()) * properties.metricCount);
    std::vector<zet_typed_value_t> ipDataValues;
    uint32_t i = 0;
    for (auto it = stallSumIpDataMap.begin(); it != stallSumIpDataMap.end(); ++it) {
        stallSumIpDataToTypedValues(it->first, it->second, ipDataValues);
        for (auto jt = ipDataValues.begin(); (jt != ipDataValues.end()) && (i < metricValueCount); jt++, i++) {
            *(pCalculatedData + i) = *jt;
        }
        ipDataValues.clear();
    }

    return ZE_RESULT_SUCCESS;
}

/*
 * stall sample data item format:
 *
 * Bits		Field
 * 0  to 28	IP (addr)
 * 29 to 36	active count
 * 37 to 44	other count
 * 45 to 52	control count
 * 53 to 60	pipestall count
 * 61 to 68	send count
 * 69 to 76	dist_acc count
 * 77 to 84	sbid count
 * 85 to 92	sync count
 * 93 to 100	inst_fetch count
 *
 * bytes 49 and 50, subSlice
 * bytes 51 and 52, flags
 *
 * total size 64 bytes
 */
void IpSamplingMetricGroupImp::stallIpDataMapUpdate(StallSumIpDataMap_t &stallSumIpDataMap, const uint8_t *pRawIpData) {

    const uint8_t *tempAddr = pRawIpData;
    uint64_t ip = 0ULL;
    memcpy_s(reinterpret_cast<uint8_t *>(&ip), sizeof(ip), tempAddr, sizeof(ip));
    ip &= 0x1fffffff;
    StallSumIpData_t &stallSumData = stallSumIpDataMap[ip];
    tempAddr += 3;

    auto getCount = [&tempAddr]() {
        uint16_t tempCount = 0;
        memcpy_s(reinterpret_cast<uint8_t *>(&tempCount), sizeof(tempCount), tempAddr, sizeof(tempCount));
        tempCount = (tempCount >> 5) & 0xff;
        tempAddr += 1;
        return static_cast<uint8_t>(tempCount);
    };

    stallSumData.activeCount += getCount();
    stallSumData.otherCount += getCount();
    stallSumData.controlCount += getCount();
    stallSumData.pipeStallCount += getCount();
    stallSumData.sendCount += getCount();
    stallSumData.distAccCount += getCount();
    stallSumData.sbidCount += getCount();
    stallSumData.syncCount += getCount();
    stallSumData.instFetchCount += getCount();

    struct stallCntrInfo {
        uint16_t subslice;
        uint16_t flags;
    } stallCntrInfo = {};

    tempAddr = pRawIpData + 48;
    memcpy_s(reinterpret_cast<uint8_t *>(&stallCntrInfo), sizeof(stallCntrInfo), tempAddr, sizeof(stallCntrInfo));

    constexpr int overflowDropFlag = (1 << 8);
    if (stallCntrInfo.flags & overflowDropFlag) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Stall Sampling Data Lost %s\n", " ");
    }
}

// The order of push_back calls must match the order of metricPropertiesList.
void IpSamplingMetricGroupImp::stallSumIpDataToTypedValues(uint64_t ip,
                                                           StallSumIpData_t &sumIpData,
                                                           std::vector<zet_typed_value_t> &ipDataValues) {
    zet_typed_value_t tmpValueData;
    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = ip;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.activeCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.controlCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.pipeStallCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.sendCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.distAccCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.sbidCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.syncCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.instFetchCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = sumIpData.otherCount;
    ipDataValues.push_back(tmpValueData);
}

bool IpSamplingMetricGroupImp::activate() {
    // There is no hardware specific activation, since metric collection starts in streamer open
    return true;
}

bool IpSamplingMetricGroupImp::deactivate() {
    return true;
}
zet_metric_group_handle_t IpSamplingMetricGroupImp::getMetricGroupForSubDevice(const uint32_t subDeviceIndex) {
    return toHandle();
}

ze_result_t IpSamplingMetricGroupImp::metricQueryPoolCreate(
    zet_context_handle_t hContext,
    zet_device_handle_t hDevice,
    const zet_metric_query_pool_desc_t *desc,
    zet_metric_query_pool_handle_t *phMetricQueryPool) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

std::unique_ptr<IpSamplingMetricGroupImp> IpSamplingMetricGroupImp::create(std::vector<IpSamplingMetricImp> &ipSamplingMetrics) {
    return std::unique_ptr<IpSamplingMetricGroupImp>(new (std::nothrow) IpSamplingMetricGroupImp(ipSamplingMetrics));
}

IpSamplingMetricImp::IpSamplingMetricImp(zet_metric_properties_t &properties) : properties(properties) {
}

ze_result_t IpSamplingMetricImp::getProperties(zet_metric_properties_t *pProperties) {
    *pProperties = properties;
    return ZE_RESULT_SUCCESS;
}

template <>
IpSamplingMetricSourceImp &MetricDeviceContext::getMetricSource<IpSamplingMetricSourceImp>() const {
    return static_cast<IpSamplingMetricSourceImp &>(*metricSources.at(MetricSource::SourceType::IpSampling));
}

} // namespace L0
