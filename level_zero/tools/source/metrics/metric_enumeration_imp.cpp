/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/os_library.h"

#include <algorithm>

namespace L0 {

const char *MetricEnumeration::oaConcurrentGroupName = "OA";

MetricEnumeration::MetricEnumeration(MetricContext &metricContextInput)
    : metricContext(metricContextInput) {}

MetricEnumeration::~MetricEnumeration() {
    cleanupMetricsDiscovery();
    initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
}

ze_result_t MetricEnumeration::metricGroupGet(uint32_t &count,
                                              zet_metric_group_handle_t *phMetricGroups) {
    if (initialize() != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (count == 0) {
        count = static_cast<uint32_t>(metricGroups.size());
        return ZE_RESULT_SUCCESS;
    } else if (count > metricGroups.size()) {
        count = static_cast<uint32_t>(metricGroups.size());
    }

    // User is expected to allocate space.
    if (phMetricGroups == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < count; i++) {
        phMetricGroups[i] = metricGroups[i]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

bool MetricEnumeration::isInitialized() { return initializationState == ZE_RESULT_SUCCESS; }

ze_result_t MetricEnumeration::initialize() {
    if (initializationState == ZE_RESULT_ERROR_UNINITIALIZED) {
        if (metricContext.isInitialized() &&
            openMetricsDiscovery() == ZE_RESULT_SUCCESS &&
            cacheMetricInformation() == ZE_RESULT_SUCCESS) {
            initializationState = ZE_RESULT_SUCCESS;
        } else {
            initializationState = ZE_RESULT_ERROR_UNKNOWN;
            cleanupMetricsDiscovery();
        }
    }

    return initializationState;
}

ze_result_t MetricEnumeration::loadMetricsDiscovery() {
    // Load library.
    hMetricsDiscovery.reset(NEO::OsLibrary::load(getMetricsDiscoveryFilename()));

    // Load exported functions.
    if (hMetricsDiscovery) {
        openMetricsDevice = reinterpret_cast<MetricsDiscovery::OpenMetricsDevice_fn>(
            hMetricsDiscovery->getProcAddress("OpenMetricsDevice"));
        closeMetricsDevice = reinterpret_cast<MetricsDiscovery::CloseMetricsDevice_fn>(
            hMetricsDiscovery->getProcAddress("CloseMetricsDevice"));
        openMetricsDeviceFromFile =
            reinterpret_cast<MetricsDiscovery::OpenMetricsDeviceFromFile_fn>(
                hMetricsDiscovery->getProcAddress("OpenMetricsDeviceFromFile"));
    }

    if (openMetricsDevice == nullptr || closeMetricsDevice == nullptr ||
        openMetricsDeviceFromFile == nullptr) {
        cleanupMetricsDiscovery();
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Return success if exported functions have been loaded.
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricEnumeration::openMetricsDiscovery() {
    UNRECOVERABLE_IF(openMetricsDevice == nullptr);
    UNRECOVERABLE_IF(closeMetricsDevice == nullptr);

    auto openResult = openMetricsDevice(&pMetricsDevice);
    if (openResult != MetricsDiscovery::CC_OK) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricEnumeration::cleanupMetricsDiscovery() {
    for (uint32_t i = 0; i < metricGroups.size(); ++i) {
        delete metricGroups[i];
    }

    metricGroups.clear();

    if (pMetricsDevice) {
        closeMetricsDevice(pMetricsDevice);
        pMetricsDevice = nullptr;
    }

    if (hMetricsDiscovery != nullptr) {
        openMetricsDevice = nullptr;
        closeMetricsDevice = nullptr;
        openMetricsDeviceFromFile = nullptr;

        hMetricsDiscovery.reset();
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricEnumeration::cacheMetricInformation() {
    UNRECOVERABLE_IF(pMetricsDevice == nullptr);

    MetricsDiscovery::TMetricsDeviceParams_1_2 *pMetricsDeviceParams = pMetricsDevice->GetParams();
    UNRECOVERABLE_IF(pMetricsDeviceParams == nullptr);

    // Check required Metrics Discovery API version - should be at least 1.5.
    const bool unsupportedMajorVersion =
        pMetricsDeviceParams->Version.MajorNumber < requiredMetricsDiscoveryMajorVersion;
    const bool unsupportedMinorVersion =
        (pMetricsDeviceParams->Version.MajorNumber == requiredMetricsDiscoveryMajorVersion) &&
        (pMetricsDeviceParams->Version.MinorNumber < requiredMetricsDiscoveryMinorVersion);

    if (unsupportedMajorVersion || unsupportedMinorVersion) {
        // Metrics Discovery API version too low
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // 1. Iterate over concurrent groups.
    MetricsDiscovery::IConcurrentGroup_1_5 *pConcurrentGroup = nullptr;
    for (uint32_t i = 0; i < pMetricsDeviceParams->ConcurrentGroupsCount; ++i) {
        pConcurrentGroup = pMetricsDevice->GetConcurrentGroup(i);
        UNRECOVERABLE_IF(pConcurrentGroup == nullptr);

        MetricsDiscovery::TConcurrentGroupParams_1_0 *pConcurrentGroupParams =
            pConcurrentGroup->GetParams();
        UNRECOVERABLE_IF(pConcurrentGroupParams == nullptr);

        // 2. Find "OA" concurrent group.
        if (strcmp(pConcurrentGroupParams->SymbolName, oaConcurrentGroupName) == 0) {
            // Reserve memory for metric groups
            metricGroups.reserve(pConcurrentGroupParams->MetricSetsCount);

            // 3. Iterate over metric sets.
            for (uint32_t j = 0; j < pConcurrentGroupParams->MetricSetsCount; ++j) {
                MetricsDiscovery::IMetricSet_1_5 *pMetricSet = pConcurrentGroup->GetMetricSet(j);
                UNRECOVERABLE_IF(pMetricSet == nullptr);

                cacheMetricGroup(*pMetricSet, *pConcurrentGroup, i,
                                 ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED);
                cacheMetricGroup(*pMetricSet, *pConcurrentGroup, i,
                                 ZET_METRIC_GROUP_SAMPLING_TYPE_EVENT_BASED);
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t
MetricEnumeration::cacheMetricGroup(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                    MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                                    const uint32_t domain,
                                    const zet_metric_group_sampling_type_t samplingType) {
    MetricsDiscovery::TMetricSetParams_1_4 *pMetricSetParams = metricSet.GetParams();
    UNRECOVERABLE_IF(pMetricSetParams == nullptr);

    const uint32_t sourceApiMask = MetricGroupImp::getApiMask(samplingType);

    // Map metric groups to level zero format and cache them.
    if (pMetricSetParams->ApiMask & sourceApiMask) {
        metricSet.SetApiFiltering(sourceApiMask);

        // Obtain params once again - updated after SetApiFiltering
        pMetricSetParams = metricSet.GetParams();

        zet_metric_group_properties_t properties = {};
        properties.version = ZET_METRIC_GROUP_PROPERTIES_VERSION_CURRENT;
        snprintf(properties.name, sizeof(properties.name), "%s",
                 pMetricSetParams->SymbolName); // To always have null-terminated string
        snprintf(properties.description, sizeof(properties.description), "%s",
                 pMetricSetParams->ShortName);
        properties.samplingType = samplingType;
        properties.domain = domain; // Concurrent group number
        properties.metricCount =
            pMetricSetParams->MetricsCount + pMetricSetParams->InformationCount;

        std::vector<Metric *> metrics;
        createMetrics(metricSet, metrics);

        auto pMetricGroup = MetricGroup::create(properties, metricSet, concurrentGroup, metrics);
        UNRECOVERABLE_IF(pMetricGroup == nullptr);

        metricGroups.push_back(pMetricGroup);

        // Disable api filtering
        metricSet.SetApiFiltering(MetricsDiscovery::API_TYPE_ALL);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricEnumeration::createMetrics(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                             std::vector<Metric *> &metrics) {
    MetricsDiscovery::TMetricSetParams_1_4 *pMetricSetParams = metricSet.GetParams();
    UNRECOVERABLE_IF(pMetricSetParams == nullptr);

    metrics.reserve(pMetricSetParams->MetricsCount + pMetricSetParams->InformationCount);

    // Map metrics to level zero format and add them to 'metrics' vector.
    for (uint32_t i = 0; i < pMetricSetParams->MetricsCount; ++i) {
        MetricsDiscovery::IMetric_1_0 *pSourceMetric = metricSet.GetMetric(i);
        UNRECOVERABLE_IF(pSourceMetric == nullptr);

        MetricsDiscovery::TMetricParams_1_0 *pSourceMetricParams = pSourceMetric->GetParams();
        UNRECOVERABLE_IF(pSourceMetricParams == nullptr);

        zet_metric_properties_t properties = {};
        properties.version = ZET_METRIC_PROPERTIES_VERSION_CURRENT;
        snprintf(properties.name, sizeof(properties.name), "%s",
                 pSourceMetricParams->SymbolName); // To always have a null-terminated string
        snprintf(properties.description, sizeof(properties.description), "%s",
                 pSourceMetricParams->LongName);
        snprintf(properties.component, sizeof(properties.component), "%s",
                 pSourceMetricParams->GroupName);
        snprintf(properties.resultUnits, sizeof(properties.resultUnits), "%s",
                 pSourceMetricParams->MetricResultUnits);
        properties.tierNumber = getMetricTierNumber(pSourceMetricParams->UsageFlagsMask);
        properties.metricType = getMetricType(pSourceMetricParams->MetricType);
        properties.resultType = getMetricResultType(pSourceMetricParams->ResultType);

        auto pMetric = Metric::create(properties);
        UNRECOVERABLE_IF(pMetric == nullptr);

        metrics.push_back(pMetric);
    }

    // Map information to level zero format and add them to 'metrics' vector (as metrics).
    for (uint32_t i = 0; i < pMetricSetParams->InformationCount; ++i) {
        MetricsDiscovery::IInformation_1_0 *pSourceInformation = metricSet.GetInformation(i);
        UNRECOVERABLE_IF(pSourceInformation == nullptr);

        MetricsDiscovery::TInformationParams_1_0 *pSourceInformationParams =
            pSourceInformation->GetParams();
        UNRECOVERABLE_IF(pSourceInformationParams == nullptr);

        zet_metric_properties_t properties;
        properties.version = ZET_METRIC_PROPERTIES_VERSION_CURRENT;
        snprintf(properties.name, sizeof(properties.name), "%s",
                 pSourceInformationParams->SymbolName); // To always have a null-terminated string
        snprintf(properties.description, sizeof(properties.description), "%s",
                 pSourceInformationParams->LongName);
        snprintf(properties.component, sizeof(properties.component), "%s",
                 pSourceInformationParams->GroupName);
        snprintf(properties.resultUnits, sizeof(properties.resultUnits), "%s",
                 pSourceInformationParams->InfoUnits);
        properties.tierNumber = 1;
        properties.metricType = getMetricType(pSourceInformationParams->InfoType);
        // MetricsDiscovery information are always UINT64
        properties.resultType = ZET_VALUE_TYPE_UINT64;

        auto pMetric = Metric::create(properties);
        UNRECOVERABLE_IF(pMetric == nullptr);

        metrics.push_back(pMetric);
    }
    return ZE_RESULT_SUCCESS;
}

uint32_t MetricEnumeration::getMetricTierNumber(const uint32_t sourceUsageFlagsMask) const {
    uint32_t tierNumber = 0;
    if (sourceUsageFlagsMask & MetricsDiscovery::USAGE_FLAG_TIER_1) {
        tierNumber = 1;
    } else if (sourceUsageFlagsMask & MetricsDiscovery::USAGE_FLAG_TIER_2) {
        tierNumber = 2;
    } else if (sourceUsageFlagsMask & MetricsDiscovery::USAGE_FLAG_TIER_3) {
        tierNumber = 3;
    } else if (sourceUsageFlagsMask & MetricsDiscovery::USAGE_FLAG_TIER_4) {
        tierNumber = 4;
    } else {
        // No tier - some metrics may have this undefined
        tierNumber = 0;
    }
    return tierNumber;
}

zet_metric_type_t
MetricEnumeration::getMetricType(const MetricsDiscovery::TMetricType sourceMetricType) const {
    switch (sourceMetricType) {
    case MetricsDiscovery::METRIC_TYPE_DURATION:
        return ZET_METRIC_TYPE_DURATION;
    case MetricsDiscovery::METRIC_TYPE_EVENT:
        return ZET_METRIC_TYPE_EVENT;
    case MetricsDiscovery::METRIC_TYPE_EVENT_WITH_RANGE:
        return ZET_METRIC_TYPE_EVENT_WITH_RANGE;
    case MetricsDiscovery::METRIC_TYPE_THROUGHPUT:
        return ZET_METRIC_TYPE_THROUGHPUT;
    case MetricsDiscovery::METRIC_TYPE_TIMESTAMP:
        return ZET_METRIC_TYPE_TIMESTAMP;
    case MetricsDiscovery::METRIC_TYPE_FLAG:
        return ZET_METRIC_TYPE_FLAG;
    case MetricsDiscovery::METRIC_TYPE_RATIO:
        return ZET_METRIC_TYPE_RATIO;
    case MetricsDiscovery::METRIC_TYPE_RAW:
        return ZET_METRIC_TYPE_RAW;
    default:
        UNRECOVERABLE_IF(!false);
        return ZET_METRIC_TYPE_RAW;
    }
}

zet_metric_type_t MetricEnumeration::getMetricType(
    const MetricsDiscovery::TInformationType sourceInformationType) const {

    switch (sourceInformationType) {
    case MetricsDiscovery::INFORMATION_TYPE_REPORT_REASON:
        return ZET_METRIC_TYPE_EVENT;
    case MetricsDiscovery::INFORMATION_TYPE_VALUE:
    case MetricsDiscovery::INFORMATION_TYPE_CONTEXT_ID_TAG:
    case MetricsDiscovery::INFORMATION_TYPE_SAMPLE_PHASE:
    case MetricsDiscovery::INFORMATION_TYPE_GPU_NODE:
        return ZET_METRIC_TYPE_RAW;
    case MetricsDiscovery::INFORMATION_TYPE_FLAG:
        return ZET_METRIC_TYPE_FLAG;
    case MetricsDiscovery::INFORMATION_TYPE_TIMESTAMP:
        return ZET_METRIC_TYPE_TIMESTAMP;
    default:
        UNRECOVERABLE_IF(!false);
        return ZET_METRIC_TYPE_RAW;
    }
}

zet_value_type_t MetricEnumeration::getMetricResultType(
    const MetricsDiscovery::TMetricResultType sourceMetricResultType) const {

    switch (sourceMetricResultType) {
    case MetricsDiscovery::RESULT_UINT32:
        return ZET_VALUE_TYPE_UINT32;
    case MetricsDiscovery::RESULT_UINT64:
        return ZET_VALUE_TYPE_UINT64;
    case MetricsDiscovery::RESULT_BOOL:
        return ZET_VALUE_TYPE_BOOL8;
    case MetricsDiscovery::RESULT_FLOAT:
        return ZET_VALUE_TYPE_FLOAT32;
    default:
        UNRECOVERABLE_IF(!false);
        return ZET_VALUE_TYPE_UINT64;
    }
}

MetricGroupImp ::~MetricGroupImp() {

    for (uint32_t i = 0; i < metrics.size(); ++i) {
        delete metrics[i];
    }

    metrics.clear();
};

ze_result_t MetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {
    UNRECOVERABLE_IF(pProperties->version > ZET_METRIC_GROUP_PROPERTIES_VERSION_CURRENT);
    copyProperties(properties, *pProperties);
    return ZE_RESULT_SUCCESS;
}

zet_metric_group_properties_t MetricGroup::getProperties(const zet_metric_group_handle_t handle) {
    auto metricGroup = MetricGroup::fromHandle(handle);
    UNRECOVERABLE_IF(!metricGroup);

    zet_metric_group_properties_t properties = {ZET_METRIC_GROUP_PROPERTIES_VERSION_CURRENT};
    metricGroup->getProperties(&properties);

    return properties;
}

ze_result_t MetricGroupImp::getMetric(uint32_t *pCount, zet_metric_handle_t *phMetrics) {
    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(metrics.size());
        return ZE_RESULT_SUCCESS;
    } else if (*pCount > metrics.size()) {
        *pCount = static_cast<uint32_t>(metrics.size());
    }

    // User is expected to allocate space.
    if (phMetrics == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < *pCount; i++) {
        phMetrics[i] = metrics[i]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

bool MetricGroupImp::activate() {
    UNRECOVERABLE_IF(pReferenceMetricSet == nullptr);
    const bool result = pReferenceMetricSet->Activate() == MetricsDiscovery::CC_OK;
    UNRECOVERABLE_IF(!result);
    return result;
}

bool MetricGroupImp::deactivate() {
    UNRECOVERABLE_IF(pReferenceMetricSet == nullptr);
    const bool result = pReferenceMetricSet->Deactivate() == MetricsDiscovery::CC_OK;
    return result;
}

uint32_t MetricGroupImp::getApiMask(const zet_metric_group_sampling_type_t samplingType) {

    switch (samplingType) {
    case ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED:
        return MetricsDiscovery::API_TYPE_IOSTREAM;
    case ZET_METRIC_GROUP_SAMPLING_TYPE_EVENT_BASED:
        return MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_OGL4_X;
    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

ze_result_t MetricGroupImp::openIoStream(uint32_t &timerPeriodNs, uint32_t &oaBufferSize) {
    const auto openResult = pReferenceConcurrentGroup->OpenIoStream(pReferenceMetricSet, 0,
                                                                    &timerPeriodNs, &oaBufferSize);
    return (openResult == MetricsDiscovery::CC_OK) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t MetricGroupImp::waitForReports(const uint32_t timeoutMs) {
    return (pReferenceConcurrentGroup->WaitForReports(timeoutMs) == MetricsDiscovery::TCompletionCode::CC_OK)
               ? ZE_RESULT_SUCCESS
               : ZE_RESULT_NOT_READY;
}

ze_result_t MetricGroupImp::readIoStream(uint32_t &reportCount, uint8_t &reportData) {
    char *castedReportData = reinterpret_cast<char *>(&reportData);

    const auto readResult =
        pReferenceConcurrentGroup->ReadIoStream(&reportCount, castedReportData, 0);

    switch (readResult) {
    case MetricsDiscovery::CC_OK:
    case MetricsDiscovery::CC_READ_PENDING:
        return ZE_RESULT_SUCCESS;

    default:
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t MetricGroupImp::closeIoStream() {
    const auto closeResult = pReferenceConcurrentGroup->CloseIoStream();
    return (closeResult == MetricsDiscovery::CC_OK) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t MetricGroupImp::calculateMetricValues(size_t rawDataSize,
                                                  const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                  zet_typed_value_t *pMetricValues) {
    const bool calculateCountOnly = *pMetricValueCount == 0;
    const bool result = calculateCountOnly
                            ? getCalculatedMetricCount(rawDataSize, *pMetricValueCount)
                            : getCalculatedMetricValues(rawDataSize, pRawData, *pMetricValueCount, pMetricValues);

    return result ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

bool MetricGroupImp::getCalculatedMetricCount(const size_t rawDataSize,
                                              uint32_t &metricValueCount) {
    uint32_t rawReportSize = getRawReportSize();

    if (rawReportSize == 0) {
        return false;
    }

    if ((rawDataSize % rawReportSize) != 0) {
        return false;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / rawReportSize;
    metricValueCount = rawReportCount * properties.metricCount;
    return true;
}

bool MetricGroupImp::getCalculatedMetricValues(const size_t rawDataSize, const uint8_t *pRawData,
                                               uint32_t &metricValueCount,
                                               zet_typed_value_t *pCalculatedData) {

    uint32_t calculatedReportCount = 0;
    uint32_t expectedMetricValueCount = 0;

    if (pCalculatedData == nullptr) {
        return false;
    }

    if (getCalculatedMetricCount(rawDataSize, expectedMetricValueCount) == false) {
        return false;
    }

    // Calculated metrics container.
    std::vector<MetricsDiscovery::TTypedValue_1_0> calculatedMetrics(expectedMetricValueCount);

    // Set filtering type.
    pReferenceMetricSet->SetApiFiltering(MetricGroupImp::getApiMask(properties.samplingType));

    // Calculate metrics.
    const bool result = pReferenceMetricSet->CalculateMetrics(
                            reinterpret_cast<unsigned char *>(const_cast<uint8_t *>(pRawData)), static_cast<uint32_t>(rawDataSize),
                            calculatedMetrics.data(),
                            static_cast<uint32_t>(calculatedMetrics.size()) * sizeof(MetricsDiscovery::TTypedValue_1_0),
                            &calculatedReportCount, nullptr, static_cast<uint32_t>(0)) == MetricsDiscovery::CC_OK;

    if (result) {

        // Adjust copied reports to buffer provided by the user.
        metricValueCount = std::min<uint32_t>(metricValueCount, calculatedReportCount * properties.metricCount);

        // Translate metrics from metrics discovery to oneAPI format.
        for (size_t i = 0; i < metricValueCount; ++i) {
            copyValue(calculatedMetrics[i], pCalculatedData[i]);
        }
    }

    return result;
}

ze_result_t MetricGroupImp::initialize(const zet_metric_group_properties_t &sourceProperties,
                                       MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                       MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                                       const std::vector<Metric *> &groupMetrics) {
    copyProperties(sourceProperties, properties);
    pReferenceMetricSet = &metricSet;
    pReferenceConcurrentGroup = &concurrentGroup;
    metrics = groupMetrics;
    return ZE_RESULT_SUCCESS;
}

uint32_t MetricGroupImp::getRawReportSize() {
    auto pMetricSetParams = pReferenceMetricSet->GetParams();

    return (properties.samplingType == ZET_METRIC_GROUP_SAMPLING_TYPE_TIME_BASED)
               ? pMetricSetParams->RawReportSize
               : pMetricSetParams->QueryReportSize;
}

void MetricGroupImp::copyProperties(const zet_metric_group_properties_t &source,
                                    zet_metric_group_properties_t &destination) {
    UNRECOVERABLE_IF(source.version < destination.version);
    destination = source;
    memcpy_s(destination.name, sizeof(destination.name),
             source.name, sizeof(destination.name));
    memcpy_s(destination.description, sizeof(destination.description),
             source.description, sizeof(destination.description));
}

void MetricGroupImp::copyValue(const MetricsDiscovery::TTypedValue_1_0 &source,
                               zet_typed_value_t &destination) const {

    destination = {};

    switch (source.ValueType) {
    case MetricsDiscovery::VALUE_TYPE_UINT32:
        destination.type = ZET_VALUE_TYPE_UINT32;
        destination.value.ui32 = source.ValueUInt32;
        break;

    case MetricsDiscovery::VALUE_TYPE_UINT64:
        destination.type = ZET_VALUE_TYPE_UINT64;
        destination.value.ui64 = source.ValueUInt64;
        break;

    case MetricsDiscovery::VALUE_TYPE_FLOAT:
        destination.type = ZET_VALUE_TYPE_FLOAT32;
        destination.value.fp32 = source.ValueFloat;
        break;

    case MetricsDiscovery::VALUE_TYPE_BOOL:
        destination.type = ZET_VALUE_TYPE_BOOL8;
        destination.value.b8 = source.ValueBool;
        break;

    default:
        destination.type = ZET_VALUE_TYPE_UINT64;
        destination.value.ui64 = 0;
        UNRECOVERABLE_IF(true);
        break;
    }
}

ze_result_t MetricImp::getProperties(zet_metric_properties_t *pProperties) {
    UNRECOVERABLE_IF(pProperties->version > ZET_METRIC_PROPERTIES_VERSION_CURRENT);
    copyProperties(properties, *pProperties);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricImp::initialize(const zet_metric_properties_t &sourceProperties) {
    copyProperties(sourceProperties, properties);
    return ZE_RESULT_SUCCESS;
}

void MetricImp::copyProperties(const zet_metric_properties_t &source,
                               zet_metric_properties_t &destination) {
    UNRECOVERABLE_IF(source.version < destination.version);
    destination = source;
    memcpy_s(destination.name, sizeof(destination.name),
             source.name, sizeof(destination.name));
    memcpy_s(destination.description, sizeof(destination.description),
             source.description, sizeof(destination.description));
    memcpy_s(destination.component, sizeof(destination.component),
             source.component, sizeof(destination.component));
    memcpy_s(destination.resultUnits, sizeof(destination.resultUnits),
             source.resultUnits, sizeof(destination.resultUnits));
}

MetricGroup *MetricGroup::create(zet_metric_group_properties_t &properties,
                                 MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                 MetricsDiscovery::IConcurrentGroup_1_5 &concurrentGroup,
                                 const std::vector<Metric *> &metrics) {
    auto pMetricGroup = new MetricGroupImp();
    UNRECOVERABLE_IF(pMetricGroup == nullptr);
    pMetricGroup->initialize(properties, metricSet, concurrentGroup, metrics);
    return pMetricGroup;
}

Metric *Metric::create(zet_metric_properties_t &properties) {
    auto pMetric = new MetricImp();
    UNRECOVERABLE_IF(pMetric == nullptr);
    pMetric->initialize(properties);
    return pMetric;
}

} // namespace L0
