/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_programmable_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_query_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include <algorithm>

namespace L0 {

const char *MetricEnumeration::oaConcurrentGroupName = "OA";

MetricEnumeration::MetricEnumeration(OaMetricSourceImp &metricSourceInput)
    : metricSource(metricSourceInput) {}

MetricEnumeration::~MetricEnumeration() {
    cleanupMetricsDiscovery();
    initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
}

ze_result_t MetricEnumeration::metricGroupGet(uint32_t &count,
                                              zet_metric_group_handle_t *phMetricGroups) {
    ze_result_t result = initialize();
    if (result != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (count == 0) {
        count = static_cast<uint32_t>(metricGroups.size());
        return ZE_RESULT_SUCCESS;
    } else if (count > metricGroups.size()) {
        count = static_cast<uint32_t>(metricGroups.size());
    }

    for (uint32_t i = 0; i < count; i++) {
        phMetricGroups[i] = metricGroups[i]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

MetricGroup *MetricEnumeration::getMetricGroupByIndex(const uint32_t index) {
    return metricGroups[index];
}

uint32_t MetricEnumeration::getMetricGroupCount() {
    return static_cast<uint32_t>(metricGroups.size());
}

bool MetricEnumeration::isInitialized() {

    if (initializationState == ZE_RESULT_ERROR_UNINITIALIZED) {
        initialize();
    }

    return initializationState == ZE_RESULT_SUCCESS;
}

bool MetricEnumeration::readGlobalSymbol(const char *name, uint32_t &symbolValue) {
    auto tempValue = pMetricsDevice->GetGlobalSymbolValueByName(name);
    if (tempValue != nullptr) {
        symbolValue = tempValue->ValueUInt32;
        return true;
    }
    return false;
}

bool MetricEnumeration::readGlobalSymbol(const char *name, uint64_t &symbolValue) {
    auto tempValue = pMetricsDevice->GetGlobalSymbolValueByName(name);
    if (tempValue != nullptr) {
        symbolValue = tempValue->ValueUInt64;
        return true;
    }
    return false;
}

ze_result_t MetricEnumeration::initialize() {
    if (initializationState == ZE_RESULT_ERROR_UNINITIALIZED) {
        if (hMetricsDiscovery &&
            openMetricsDiscovery() == ZE_RESULT_SUCCESS &&
            cacheMetricInformation() == ZE_RESULT_SUCCESS) {

            if (metricSource.isImplicitScalingCapable()) {
                const auto &deviceImp = *static_cast<DeviceImp *>(&metricSource.getDevice());
                for (size_t i = 0; i < deviceImp.numSubDevices; i++) {
                    deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricsLibrary().enableWorkloadPartition();
                }
            }
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
    std::vector<const char *> libnames;
    getMetricsDiscoveryFilename(libnames);

    for (auto &name : libnames) {
        hMetricsDiscovery.reset(NEO::OsLibrary::loadFunc({name}));

        // Load exported functions.
        if (hMetricsDiscovery) {
            openAdapterGroup = reinterpret_cast<MetricsDiscovery::OpenAdapterGroup_fn>(
                hMetricsDiscovery->getProcAddress("OpenAdapterGroup"));
        }

        if (openAdapterGroup == nullptr) {
            METRICS_LOG_ERR("cannot load %s exported functions", name);
        } else {
            METRICS_LOG_DBG("loaded %s exported functions", name);
            break;
        }
    }

    if (openAdapterGroup == nullptr) {
        cleanupMetricsDiscovery();
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    // Return success if exported functions have been loaded.
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricEnumeration::openMetricsDiscovery() {
    UNRECOVERABLE_IF(openAdapterGroup == nullptr);

    // Clean up members.
    pAdapterGroup = nullptr;
    pAdapter = nullptr;
    pMetricsDevice = nullptr;

    // Open adapter group.
    openAdapterGroup((MetricsDiscovery::IAdapterGroupLatest **)&pAdapterGroup);
    if (pAdapterGroup == nullptr) {
        METRICS_LOG_ERR("unable to open metrics adapter groups %s", " ");
        cleanupMetricsDiscovery();
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Obtain metrics adapter that matches adapter used by l0.
    pAdapter = getMetricsAdapter();
    if (pAdapter == nullptr) {
        METRICS_LOG_ERR("unable to open metrics adapter %s", " ");
        cleanupMetricsDiscovery();
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    auto &device = metricSource.getDevice();
    const auto &deviceImp = *static_cast<DeviceImp *>(&device);
    if (metricSource.isImplicitScalingCapable()) {

        // Open metrics device for each sub device.
        for (size_t i = 0; i < deviceImp.numSubDevices; i++) {

            auto &subDeviceMetricEnumeraion = deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricEnumeration();
            auto &metricsDevice = subDeviceMetricEnumeraion.pMetricsDevice;
            auto subDeviceImp = static_cast<DeviceImp *>(deviceImp.subDevices[i]);
            openMetricsSubDeviceFromAdapter(pAdapter, subDeviceImp->getPhysicalSubDeviceId(), &metricsDevice);
            subDeviceMetricEnumeraion.pAdapter = pAdapter;
            if (metricsDevice == nullptr) {
                METRICS_LOG_ERR("unable to open metrics device %u", i);
                cleanupMetricsDiscovery();
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }

            subDeviceMetricEnumeraion.readGlobalSymbol(globalSymbolOaMaxBufferSize.data(), maximumOaBufferSize);
        }

        openMetricsDeviceFromAdapter(pAdapter, &pMetricsDevice);
        if (pMetricsDevice == nullptr) {
            METRICS_LOG_ERR("unable to open metrics device %u", 0);
            cleanupMetricsDiscovery();
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

    } else {
        auto &deviceImp = *static_cast<DeviceImp *>(&metricSource.getDevice());
        const uint32_t subDeviceIndex = deviceImp.getPhysicalSubDeviceId();
        if (subDeviceIndex == 0) {
            // Open metrics device for root device or sub device with index 0.
            openMetricsDeviceFromAdapter(pAdapter, &pMetricsDevice);
        } else {
            // Open metrics device for a given sub device index.
            openMetricsSubDeviceFromAdapter(pAdapter, subDeviceIndex, &pMetricsDevice);
        }

        if (pMetricsDevice == nullptr) {
            METRICS_LOG_ERR("unable to open metrics device %u", subDeviceIndex);
            cleanupMetricsDiscovery();
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        readGlobalSymbol(globalSymbolOaMaxBufferSize.data(), maximumOaBufferSize);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricEnumeration::cleanupMetricsDiscovery() {
    if (pAdapter) {

        auto &device = metricSource.getDevice();
        const auto &deviceImp = *static_cast<DeviceImp *>(&device);
        if (metricSource.isImplicitScalingCapable()) {

            for (size_t i = 0; i < deviceImp.numSubDevices; i++) {
                deviceImp.subDevices[i]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricEnumeration().cleanupMetricsDiscovery();
            }
        }

        if (pMetricsDevice) {

            // Close metrics device for one sub device or root device.
            pAdapter->CloseMetricsDevice(pMetricsDevice);
            pMetricsDevice = nullptr;
        }
    }

    for (size_t i = 0; i < metricGroups.size(); ++i) {
        delete metricGroups[i];
    }
    metricGroups.clear();
    cleanupExtendedMetricInformation();

    if (hMetricsDiscovery != nullptr) {
        if (pAdapterGroup != nullptr) {
            pAdapterGroup->Close();
        }
        pAdapterGroup = nullptr;
        openAdapterGroup = nullptr;
        hMetricsDiscovery.reset();
    }

    return ZE_RESULT_SUCCESS;
} // namespace L0

ze_result_t MetricEnumeration::cacheMetricInformation() {

    auto &device = metricSource.getDevice();
    const auto &deviceImp = *static_cast<DeviceImp *>(&device);
    if (metricSource.isImplicitScalingCapable()) {

        ze_result_t result = ZE_RESULT_SUCCESS;

        // Get metric information from all sub devices.
        for (auto subDevice : deviceImp.subDevices) {
            result = subDevice->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricEnumeration().cacheMetricInformation();
            if (ZE_RESULT_SUCCESS != result) {
                return result;
            }
        }

        // Get metric groups count for one sub device.
        const uint32_t metricGroupCount = deviceImp.subDevices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricEnumeration().getMetricGroupCount();

        // Cache and aggregate all metric groups from all sub devices.
        for (uint32_t i = 0; i < metricGroupCount; i++) {
            auto metricGroupRootDevice = new OaMetricGroupImp(metricSource);
            metricGroupRootDevice->setRootDeviceFlag();

            for (auto subDevice : deviceImp.subDevices) {
                MetricGroup *metricGroupSubDevice = subDevice->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricEnumeration().getMetricGroupByIndex(i);

                metricGroupRootDevice->getMetricGroups().push_back(static_cast<MetricGroupImp *>(metricGroupSubDevice));
            }

            metricGroups.push_back(metricGroupRootDevice);
        }

        return result;
    }

    // Avoid repeated caching for the sub-device
    if (getMetricGroupCount() > 0) {
        return ZE_RESULT_SUCCESS;
    }

    DEBUG_BREAK_IF(pMetricsDevice == nullptr);

    MetricsDiscovery::TMetricsDeviceParams_1_2 *pMetricsDeviceParams = pMetricsDevice->GetParams();
    DEBUG_BREAK_IF(pMetricsDeviceParams == nullptr);

    // Check required Metrics Discovery API version - should be at least 1.13.
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
    MetricsDiscovery::IConcurrentGroup_1_13 *pConcurrentGroup = nullptr;
    for (uint32_t i = 0; i < pMetricsDeviceParams->ConcurrentGroupsCount; ++i) {
        pConcurrentGroup = getConcurrentGroupFromDevice(pMetricsDevice, i);
        DEBUG_BREAK_IF(pConcurrentGroup == nullptr);

        MetricsDiscovery::TConcurrentGroupParams_1_0 *pConcurrentGroupParams =
            pConcurrentGroup->GetParams();
        DEBUG_BREAK_IF(pConcurrentGroupParams == nullptr);

        // 2. Find "OA" concurrent group.
        if (strcmp(pConcurrentGroupParams->SymbolName, oaConcurrentGroupName) == 0) {

            // Find the oaBufferOverflowInformation and store in member variable pOaBufferOverflowInformation
            const uint32_t measurementInfoCount = pConcurrentGroupParams->IoMeasurementInformationCount;
            MetricsDiscovery::IInformationLatest *oaBufferOverflowInformation = nullptr;

            for (uint32_t i = 0; i < measurementInfoCount; ++i) {
                MetricsDiscovery::IInformationLatest *ioMeasurement = pConcurrentGroup->GetIoMeasurementInformation(i);
                DEBUG_BREAK_IF(ioMeasurement == nullptr);

                if (ioMeasurement->GetParams()->SymbolName == std::string("BufferOverflow")) {
                    oaBufferOverflowInformation = ioMeasurement;
                    break;
                }
            }

            // MDAPI checks for proper library initialization
            if (oaBufferOverflowInformation == nullptr ||
                oaBufferOverflowInformation->GetParams()->IoReadEquation->GetEquationElementsCount() != 1 ||
                oaBufferOverflowInformation->GetParams()->IoReadEquation->GetEquationElement(0)->Type != MetricsDiscovery::EQUATION_ELEM_IMM_UINT64) {
                METRICS_LOG_ERR("IoMeasurmentInformation is not as expected for OA %s", " ");
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            pOaBufferOverflowInformation = oaBufferOverflowInformation;

            // Reserve memory for metric groups
            metricGroups.reserve(pConcurrentGroupParams->MetricSetsCount);

            // 3. Iterate over metric sets.
            for (uint32_t j = 0; j < pConcurrentGroupParams->MetricSetsCount; ++j) {
                MetricsDiscovery::IMetricSet_1_5 *pMetricSet = pConcurrentGroup->GetMetricSet(j);
                DEBUG_BREAK_IF(pMetricSet == nullptr);

                cacheMetricGroup(*pMetricSet, *pConcurrentGroup, i,
                                 ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
                cacheMetricGroup(*pMetricSet, *pConcurrentGroup, i,
                                 ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
            }
            cacheExtendedMetricInformation(*pConcurrentGroup, i);
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t
MetricEnumeration::cacheMetricGroup(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                    const uint32_t domain,
                                    const zet_metric_group_sampling_type_flag_t samplingType) {
    MetricsDiscovery::TMetricSetParams_1_4 *pMetricSetParams = metricSet.GetParams();
    DEBUG_BREAK_IF(pMetricSetParams == nullptr);

    const uint32_t sourceApiMask = OaMetricGroupImp::getApiMask(samplingType);

    // Map metric groups to level zero format and cache them.
    if (pMetricSetParams->ApiMask & sourceApiMask) {
        metricSet.SetApiFiltering(sourceApiMask);

        // Obtain params once again - updated after SetApiFiltering
        pMetricSetParams = metricSet.GetParams();

        zet_metric_group_properties_t properties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
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

        auto pMetricGroup = OaMetricGroupImp::create(properties, metricSet, concurrentGroup, metrics, metricSource);
        DEBUG_BREAK_IF(pMetricGroup == nullptr);

        metricGroups.push_back(pMetricGroup);

        // Disable api filtering
        metricSet.SetApiFiltering(MetricsDiscovery::API_TYPE_ALL);
    }

    return ZE_RESULT_SUCCESS;
}

void MetricEnumeration::getL0MetricPropertiesFromMdapiMetric(zet_metric_properties_t &l0MetricProps, MetricsDiscovery::IMetric_1_0 *mdapiMetric) {
    MetricsDiscovery::TMetricParams_1_0 *pSourceMetricParams = mdapiMetric->GetParams();
    DEBUG_BREAK_IF(pSourceMetricParams == nullptr);

    snprintf(l0MetricProps.name, sizeof(l0MetricProps.name), "%s",
             pSourceMetricParams->SymbolName); // To always have a null-terminated string
    snprintf(l0MetricProps.description, sizeof(l0MetricProps.description), "%s",
             pSourceMetricParams->LongName);
    snprintf(l0MetricProps.component, sizeof(l0MetricProps.component), "%s",
             pSourceMetricParams->GroupName);
    snprintf(l0MetricProps.resultUnits, sizeof(l0MetricProps.resultUnits), "%s",
             pSourceMetricParams->MetricResultUnits);
    l0MetricProps.tierNumber = getMetricTierNumber(pSourceMetricParams->UsageFlagsMask);
    l0MetricProps.metricType = getMetricType(pSourceMetricParams->MetricType);
    l0MetricProps.resultType = getMetricResultType(pSourceMetricParams->ResultType);
}

void MetricEnumeration::getL0MetricPropertiesFromMdapiInformation(zet_metric_properties_t &l0MetricProps, MetricsDiscovery::IInformation_1_0 *mdapiInformation) {
    MetricsDiscovery::TInformationParams_1_0 *pSourceInformationParams = mdapiInformation->GetParams();
    DEBUG_BREAK_IF(pSourceInformationParams == nullptr);

    snprintf(l0MetricProps.name, sizeof(l0MetricProps.name), "%s",
             pSourceInformationParams->SymbolName); // To always have a null-terminated string
    snprintf(l0MetricProps.description, sizeof(l0MetricProps.description), "%s",
             pSourceInformationParams->LongName);
    snprintf(l0MetricProps.component, sizeof(l0MetricProps.component), "%s",
             pSourceInformationParams->GroupName);
    snprintf(l0MetricProps.resultUnits, sizeof(l0MetricProps.resultUnits), "%s",
             pSourceInformationParams->InfoUnits);
    l0MetricProps.tierNumber = 1;
    l0MetricProps.metricType = getMetricType(pSourceInformationParams->InfoType);
    l0MetricProps.resultType = l0MetricProps.metricType == ZET_METRIC_TYPE_FLAG
                                   ? ZET_VALUE_TYPE_BOOL8
                                   : ZET_VALUE_TYPE_UINT64;
}

ze_result_t MetricEnumeration::createMetrics(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                             std::vector<Metric *> &metrics) {
    MetricsDiscovery::TMetricSetParams_1_4 *pMetricSetParams = metricSet.GetParams();
    DEBUG_BREAK_IF(pMetricSetParams == nullptr);

    metrics.reserve(pMetricSetParams->MetricsCount + pMetricSetParams->InformationCount);

    // Map metrics to level zero format and add them to 'metrics' vector.
    for (uint32_t i = 0; i < pMetricSetParams->MetricsCount; ++i) {
        MetricsDiscovery::IMetric_1_0 *pSourceMetric = metricSet.GetMetric(i);
        DEBUG_BREAK_IF(pSourceMetric == nullptr);

        zet_metric_properties_t properties = {};
        getL0MetricPropertiesFromMdapiMetric(properties, pSourceMetric);
        auto pMetric = OaMetricImp::create(metricSource, properties);
        UNRECOVERABLE_IF(pMetric == nullptr);

        metrics.push_back(pMetric);
    }

    // Map information to level zero format and add them to 'metrics' vector (as metrics).
    for (uint32_t i = 0; i < pMetricSetParams->InformationCount; ++i) {
        MetricsDiscovery::IInformation_1_0 *pSourceInformation = metricSet.GetInformation(i);
        DEBUG_BREAK_IF(pSourceInformation == nullptr);

        zet_metric_properties_t properties = {};
        getL0MetricPropertiesFromMdapiInformation(properties, pSourceInformation);

        auto pMetric = OaMetricImp::create(metricSource, properties);
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
        DEBUG_BREAK_IF(!false);
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
        DEBUG_BREAK_IF(!false);
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
        DEBUG_BREAK_IF(!false);
        return ZET_VALUE_TYPE_UINT64;
    }
}

ze_result_t MetricEnumeration::cacheExtendedMetricInformation(
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
    const uint32_t domain) {

    if (isMetricProgrammableSupportEnabled()) {
        pConcurrentGroup = &concurrentGroup;
        cacheMetricPrototypes(concurrentGroup, domain);
    }
    return ZE_RESULT_SUCCESS;
}

void MetricEnumeration::cacheMetricPrototypes(MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                              const uint32_t domain) {

    auto metricEnumerator = concurrentGroup.GetMetricEnumerator();
    if (metricEnumerator == nullptr) {
        METRICS_LOG_ERR("MetricPrototype Enumeration Failed for domain %d. MetricProgrammable unavailable", domain);
        return;
    }
    uint32_t metricPrototypeCount = metricEnumerator->GetMetricPrototypeCount();
    if (metricPrototypeCount == 0) {
        METRICS_LOG_DBG("%s", "MetricPrototypeCount is 0");
        return;
    }
    std::vector<MetricsDiscovery::IMetricPrototype_1_13 *> metricPrototypes(metricPrototypeCount);
    metricEnumerator->GetMetricPrototypes(0, &metricPrototypeCount, metricPrototypes.data());
    updateMetricProgrammablesFromPrototypes(concurrentGroup, metricPrototypes, domain);
}

void MetricEnumeration::updateMetricProgrammablesFromPrototypes(
    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
    const std::vector<MetricsDiscovery::IMetricPrototype_1_13 *> &metricPrototypes,
    uint32_t domain) {

    for (auto &metricPrototype : metricPrototypes) {
        auto metricPrototypeParams = metricPrototype->GetParams();
        // Any failure just avoids adding programmables
        if (metricPrototypeParams == nullptr) {
            continue;
        }
        zet_metric_programmable_exp_properties_t properties{};
        snprintf(properties.component, sizeof(properties.component), "%s",
                 metricPrototypeParams->GroupName);
        snprintf(properties.name, sizeof(properties.name), "%s",
                 metricPrototypeParams->SymbolName); // To always have a null-terminated string
        snprintf(properties.description, sizeof(properties.description), "%s",
                 metricPrototypeParams->LongName);
        properties.domain = domain;
        properties.tierNumber = getMetricTierNumber(metricPrototypeParams->UsageFlagsMask);
        properties.samplingType = getSamplingTypeFromApiMask(metricPrototypeParams->ApiMask);
        properties.parameterCount = metricPrototypeParams->OptionDescriptorCount;
        properties.sourceId = MetricSource::metricSourceTypeOa;
        auto pMetricProgrammable = OaMetricProgrammableImp::create(properties, concurrentGroup, *metricPrototype, metricSource);
        metricProgrammables.push_back(pMetricProgrammable);
    }
}

ze_result_t MetricEnumeration::metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) {
    ze_result_t result = initialize();
    if (result != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    // For Root device, create multi device programmables
    if (metricProgrammables.size() == 0 && metricSource.isImplicitScalingCapable()) {
        auto &device = metricSource.getDevice();
        const auto &deviceImp = *static_cast<DeviceImp *>(&device);
        MetricEnumeration &metricEnumeration = deviceImp.subDevices[0]->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricEnumeration();
        const uint32_t programmableCount = static_cast<uint32_t>(metricEnumeration.getProgrammables().size());
        metricProgrammables.reserve(programmableCount);

        for (uint32_t index = 0; index < programmableCount; index++) {
            std::vector<MetricProgrammable *> subDeviceProgrammables{};
            // Get metric programmables from all sub devices.
            for (auto subDevice : deviceImp.subDevices) {
                MetricEnumeration &subDeviceEnumeration = subDevice->getMetricDeviceContext().getMetricSource<OaMetricSourceImp>().getMetricEnumeration();
                subDeviceProgrammables.push_back(subDeviceEnumeration.getProgrammables()[index]);
            }
            metricProgrammables.push_back(HomogeneousMultiDeviceMetricProgrammable::create(metricSource, subDeviceProgrammables));
        }
    }

    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(metricProgrammables.size());
        return ZE_RESULT_SUCCESS;
    }

    *pCount = std::min<uint32_t>(*pCount, static_cast<uint32_t>(metricProgrammables.size()));
    for (uint32_t i = 0; i < *pCount; i++) {
        phMetricProgrammables[i] = metricProgrammables[i]->toHandle();
    }
    return ZE_RESULT_SUCCESS;
}

zet_metric_group_sampling_type_flags_t MetricEnumeration::getSamplingTypeFromApiMask(const uint32_t apiMask) {
    const uint32_t checkMask = MetricsDiscovery::API_TYPE_IOSTREAM | MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_OGL4_X;
    if ((apiMask & checkMask) == checkMask) {
        return METRICS_SAMPLING_TYPE_TIME_EVENT_BASED;
    }

    if (apiMask & MetricsDiscovery::API_TYPE_IOSTREAM) {
        return ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;
    } else {
        return ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;
    }
}

void MetricEnumeration::cleanupExtendedMetricInformation() {
    for (size_t i = 0; i < metricProgrammables.size(); ++i) {
        delete metricProgrammables[i];
    }
    metricProgrammables.clear();
}

OaMetricGroupImp ::~OaMetricGroupImp() {

    for (size_t i = 0; i < metrics.size(); ++i) {
        // Destroy all immutable metrics
        if (!static_cast<OaMetricImp *>(metrics[i])->isImmutable()) {
            DEBUG_BREAK_IF(true);
            continue;
        }
        delete metrics[i];
    }

    metrics.clear();
};

ze_result_t OaMetricGroupImp::getProperties(zet_metric_group_properties_t *pProperties) {

    ze_result_t status = ZE_RESULT_SUCCESS;
    if (metricGroups.size() > 0) {
        status = OaMetricGroupImp::getProperties(metricGroups[0], pProperties);
    } else {

        void *pNext = pProperties->pNext;
        copyProperties(properties, *pProperties);
        pProperties->pNext = pNext;
        if (pNext) {
            status = metricSource.handleMetricGroupExtendedProperties(toHandle(), pProperties, pNext);
        }
    }

    return status;
}

ze_result_t OaMetricGroupImp::getProperties(const zet_metric_group_handle_t handle, zet_metric_group_properties_t *pProperties) {
    auto metricGroup = MetricGroup::fromHandle(handle);
    UNRECOVERABLE_IF(!metricGroup);

    return metricGroup->getProperties(pProperties);
}

ze_result_t OaMetricGroupImp::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {

    if (metricGroups.size() > 0) {
        auto metricGroupSubDevice = MetricGroup::fromHandle(metricGroups[0]);
        return metricGroupSubDevice->metricGet(pCount, phMetrics);
    }

    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(metrics.size());
        return ZE_RESULT_SUCCESS;
    }
    // User is expected to allocate space.
    DEBUG_BREAK_IF(phMetrics == nullptr);

    if (*pCount > metrics.size()) {
        *pCount = static_cast<uint32_t>(metrics.size());
    }
    for (uint32_t i = 0; i < *pCount; i++) {
        phMetrics[i] = metrics[i]->toHandle();
    }

    return ZE_RESULT_SUCCESS;
}

bool OaMetricGroupImp::activate() {

    if (properties.samplingType != ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED) {
        return true;
    }

    auto metricSource = getMetricSource();
    auto hConfiguration = metricSource->getMetricsLibrary().getConfiguration(toHandle());
    // Validate metrics library handle.
    if (!hConfiguration.IsValid()) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    // Write metric group configuration to gpu.
    const bool result = metricSource->getMetricsLibrary().activateConfiguration(hConfiguration);

    DEBUG_BREAK_IF(!result);
    return result;
}

bool OaMetricGroupImp::deactivate() {

    if (properties.samplingType != ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED) {
        return true;
    }

    auto metricSource = getMetricSource();
    auto hConfiguration = metricSource->getMetricsLibrary().getConfiguration(toHandle());
    // Deactivate metric group configuration using metrics library.
    metricSource->getMetricsLibrary().deactivateConfiguration(hConfiguration);

    // Release Metrics Library is not used.
    if (metricSource->getMetricsLibrary().getMetricQueryCount() == 0) {
        if (metricSource->getMetricsLibrary().getInitializationState() != ZE_RESULT_ERROR_UNINITIALIZED) {
            metricSource->getMetricsLibrary().release();
        }
    }
    return true;
}

bool OaMetricGroupImp::activateMetricSet() {
    DEBUG_BREAK_IF(pReferenceMetricSet == nullptr);
    const bool result = pReferenceMetricSet->Activate() == MetricsDiscovery::CC_OK;
    DEBUG_BREAK_IF(!result);
    return result;
}

bool OaMetricGroupImp::deactivateMetricSet() {
    DEBUG_BREAK_IF(pReferenceMetricSet == nullptr);
    const bool result = pReferenceMetricSet->Deactivate() == MetricsDiscovery::CC_OK;
    return result;
}

uint32_t OaMetricGroupImp::getApiMask(const zet_metric_group_sampling_type_flags_t samplingType) {

    switch (samplingType) {
    case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED:
        return MetricsDiscovery::API_TYPE_IOSTREAM;
    case ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED:
        return MetricsDiscovery::API_TYPE_OCL | MetricsDiscovery::API_TYPE_OGL4_X;
    default:
        DEBUG_BREAK_IF(true);
        return 0;
    }
}

zet_metric_group_handle_t OaMetricGroupImp::getMetricGroupForSubDevice(const uint32_t subDeviceIndex) {
    if (metricGroups.size() > 0) {
        return metricGroups[subDeviceIndex];
    }
    return toHandle();
}

ze_result_t OaMetricGroupImp::openIoStream(uint32_t &timerPeriodNs, uint32_t &oaBufferSize) {
    const auto openResult = pReferenceConcurrentGroup->OpenIoStream(pReferenceMetricSet, 0,
                                                                    &timerPeriodNs, &oaBufferSize);
    return (openResult == MetricsDiscovery::CC_OK) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t OaMetricGroupImp::waitForReports(const uint32_t timeoutMs) {
    return (pReferenceConcurrentGroup->WaitForReports(timeoutMs) == MetricsDiscovery::TCompletionCode::CC_OK)
               ? ZE_RESULT_SUCCESS
               : ZE_RESULT_NOT_READY;
}

ze_result_t OaMetricGroupImp::readIoStream(uint32_t &reportCount, uint8_t &reportData) {
    char *castedReportData = reinterpret_cast<char *>(&reportData);

    const auto readResult =
        pReferenceConcurrentGroup->ReadIoStream(&reportCount, castedReportData, 0);

    switch (readResult) {
    case MetricsDiscovery::CC_OK:
    case MetricsDiscovery::CC_READ_PENDING: {
        MetricsDiscovery::IInformationLatest *oaBufferOverflowInformation = getMetricEnumeration().getOaBufferOverflowInformation();
        const bool oaBufferOverflow = oaBufferOverflowInformation->GetParams()->IoReadEquation->GetEquationElement(0)->ImmediateUInt64 != 0;
        return oaBufferOverflow ? ZE_RESULT_WARNING_DROPPED_DATA : ZE_RESULT_SUCCESS;
    }

    default:
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

ze_result_t OaMetricGroupImp::closeIoStream() {
    const auto closeResult = pReferenceConcurrentGroup->CloseIoStream();
    return (closeResult == MetricsDiscovery::CC_OK) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t OaMetricGroupImp::calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                    const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                                    zet_typed_value_t *pMetricValues) {

    const MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<const MetricGroupCalculateHeader *>(pRawData);
    if (pRawHeader->magic == MetricGroupCalculateHeader::magicValue) {
        METRICS_LOG_ERR("%s", "The call is not supported for multiple devices");
        METRICS_LOG_ERR("%s", "Please use zetMetricGroupCalculateMultipleMetricValuesExp instead");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const bool calculateCountOnly = *pMetricValueCount == 0;
    const ze_result_t result = calculateCountOnly
                                   ? getCalculatedMetricCount(rawDataSize, *pMetricValueCount)
                                   : getCalculatedMetricValues(type, rawDataSize, pRawData, *pMetricValueCount, pMetricValues);

    return result;
}

ze_result_t OaMetricGroupImp::calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                       const uint8_t *pRawData, uint32_t *pSetCount,
                                                       uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                       zet_typed_value_t *pMetricValues) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    const MetricGroupCalculateHeader *pRawHeader = reinterpret_cast<const MetricGroupCalculateHeader *>(pRawData);

    if (pRawHeader->magic != MetricGroupCalculateHeader::magicValue) {

        const bool calculationCountOnly = *pTotalMetricValueCount == 0;
        result = calculateMetricValues(type, rawDataSize, pRawData, pTotalMetricValueCount, pMetricValues);

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

    const size_t metricGroupCount = metricGroups.size();

    if (*pSetCount == 0 || *pTotalMetricValueCount == 0) {

        const uint32_t *pRawDataSizesUnpacked = reinterpret_cast<const uint32_t *>(pRawData + pRawHeader->rawDataSizes);

        if (metricGroupCount == 0) {
            result = getCalculatedMetricCount(*pRawDataSizesUnpacked, *pTotalMetricValueCount);
            *pSetCount = result == ZE_RESULT_SUCCESS
                             ? 1
                             : 0;
        } else {
            *pSetCount = static_cast<uint32_t>(metricGroupCount);
            *pTotalMetricValueCount = 0;

            for (size_t i = 0; i < metricGroupCount; i++) {
                uint32_t metricCount = 0;
                auto &metricGroup = *static_cast<OaMetricGroupImp *>(metricGroups[i]);
                result = metricGroup.getCalculatedMetricCount(pRawDataSizesUnpacked[i], metricCount);

                if (result == ZE_RESULT_NOT_READY) {
                    continue;
                } else if (result != ZE_RESULT_SUCCESS) {
                    *pSetCount = 0;
                    *pTotalMetricValueCount = 0;
                    break;
                }

                *pTotalMetricValueCount += metricCount;
            }

            if (result == ZE_RESULT_NOT_READY) {
                if (*pTotalMetricValueCount == 0) {
                    result = ZE_RESULT_ERROR_INVALID_SIZE;
                    *pSetCount = 0;
                } else {
                    result = ZE_RESULT_SUCCESS;
                }
            }
        }
    } else {

        const uint32_t *pRawDataSizesUnpacked = reinterpret_cast<const uint32_t *>(pRawData + pRawHeader->rawDataSizes);
        const uint32_t *pRawDataOffsetsUnpacked = reinterpret_cast<const uint32_t *>(pRawData + pRawHeader->rawDataOffsets);
        const uint8_t *pRawDataOffsetUnpacked = reinterpret_cast<const uint8_t *>(pRawData + pRawHeader->rawDataOffset);

        if (metricGroupCount == 0) {
            result = getCalculatedMetricValues(type, pRawDataSizesUnpacked[0], pRawDataOffsetUnpacked, *pTotalMetricValueCount, pMetricValues);
            pMetricCounts[0] = *pTotalMetricValueCount;

        } else {
            UNRECOVERABLE_IF(*pSetCount > metricGroupCount);
            const uint32_t maxTotalMetricValueCount = *pTotalMetricValueCount;
            *pTotalMetricValueCount = 0;
            for (size_t i = 0; i < *pSetCount; i++) {
                auto &metricGroup = *static_cast<OaMetricGroupImp *>(metricGroups[i]);
                const uint32_t dataSize = pRawDataSizesUnpacked[i];
                const uint8_t *pRawDataOffset = pRawDataOffsetUnpacked + pRawDataOffsetsUnpacked[i];

                pMetricCounts[i] = maxTotalMetricValueCount;
                result = metricGroup.getCalculatedMetricValues(type, dataSize, pRawDataOffset, pMetricCounts[i], pMetricValues);

                if (result == ZE_RESULT_NOT_READY) {
                    pMetricCounts[i] = 0;
                    continue;
                } else if (result != ZE_RESULT_SUCCESS) {
                    for (size_t j = 0; j < *pSetCount; j++) {
                        pMetricCounts[j] = 0;
                    }
                    *pTotalMetricValueCount = 0;
                    break;
                }

                *pTotalMetricValueCount += pMetricCounts[i];
                pMetricValues += pMetricCounts[i];
            }

            if (result == ZE_RESULT_NOT_READY) {
                if (*pTotalMetricValueCount == 0) {
                    result = ZE_RESULT_ERROR_INVALID_SIZE;
                    for (size_t i = 0; i < *pSetCount; i++) {
                        pMetricCounts[i] = 0;
                    }
                    *pSetCount = 0;
                } else {
                    result = ZE_RESULT_SUCCESS;
                }
            }
        }
    }

    return result;
}

ze_result_t OaMetricGroupImp::getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                                     uint64_t *globalTimestamp,
                                                     uint64_t *metricTimestamp) {
    ze_result_t result;
    OaMetricSourceImp *metricSource = getMetricSource();
    const auto deviceImp = static_cast<DeviceImp *>(&metricSource->getMetricDeviceContext().getDevice());

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

        if (!metricSource->isFrequencyDataAvailable) {
            metricSource->csTimestampPeriodNs = metricSource->getMetricDeviceContext().getDevice().getNEODevice()->getProfilingTimerResolution();
            result = metricSource->getTimerResolution(metricSource->oaTimestampFrequency);
            if (result != ZE_RESULT_SUCCESS) {
                METRICS_LOG_ERR("Could not fetch oaTimestampFrequency from getTimerResolution(). Return status received %x ", result);
                *globalTimestamp = 0;
                *metricTimestamp = 0;
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            metricSource->isFrequencyDataAvailable = true;
        }

        const uint64_t csTimestampFrequency = static_cast<uint64_t>(CommonConstants::nsecPerSec / metricSource->csTimestampPeriodNs);
        *metricTimestamp = deviceTimestamp * (metricSource->oaTimestampFrequency / csTimestampFrequency);
        result = ZE_RESULT_SUCCESS;
    }

    return result;
}

ze_result_t OaMetricGroupImp::getCalculatedMetricCount(const size_t rawDataSize,
                                                       uint32_t &metricValueCount) {
    metricValueCount = 0;

    if (rawDataSize == 0) {
        return ZE_RESULT_NOT_READY;
    }

    const uint32_t rawReportSize = getRawReportSize();

    if (rawReportSize == 0 ||
        (rawDataSize % rawReportSize) != 0) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    const uint32_t rawReportCount = static_cast<uint32_t>(rawDataSize) / rawReportSize;
    metricValueCount = rawReportCount * properties.metricCount;

    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricGroupImp::getCalculatedMetricValues(const zet_metric_group_calculation_type_t type, const size_t rawDataSize, const uint8_t *pRawData,
                                                        uint32_t &metricValueCount,
                                                        zet_typed_value_t *pCalculatedData) {

    uint32_t calculatedReportCount = 0;
    uint32_t expectedMetricValueCount = 0;

    if (pCalculatedData == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t result = getCalculatedMetricCount(rawDataSize, expectedMetricValueCount);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    // Calculated metrics / maximum values container.
    std::vector<MetricsDiscovery::TTypedValue_1_0> calculatedMetrics(expectedMetricValueCount);
    std::vector<MetricsDiscovery::TTypedValue_1_0> maximumValues(expectedMetricValueCount);

    // Set filtering type.
    pReferenceMetricSet->SetApiFiltering(OaMetricGroupImp::getApiMask(properties.samplingType));

    // Calculate metrics.
    const uint32_t outMetricsSize = static_cast<uint32_t>(calculatedMetrics.size()) * sizeof(MetricsDiscovery::TTypedValue_1_0);
    result = pReferenceMetricSet->CalculateMetrics(
                 reinterpret_cast<unsigned char *>(const_cast<uint8_t *>(pRawData)), static_cast<uint32_t>(rawDataSize),
                 calculatedMetrics.data(),
                 outMetricsSize,
                 &calculatedReportCount, maximumValues.data(), outMetricsSize) == MetricsDiscovery::CC_OK
                 ? ZE_RESULT_SUCCESS
                 : ZE_RESULT_ERROR_UNKNOWN;

    if (result == ZE_RESULT_SUCCESS) {

        // Adjust copied reports to buffer provided by the user.
        metricValueCount = std::min<uint32_t>(metricValueCount, calculatedReportCount * properties.metricCount);

        // Translate metrics from metrics discovery to oneAPI format.
        switch (type) {
        case ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES:
            for (size_t i = 0; i < metricValueCount; ++i) {
                copyValue(calculatedMetrics[i], pCalculatedData[i]);
            }
            break;

        case ZET_METRIC_GROUP_CALCULATION_TYPE_MAX_METRIC_VALUES:
            for (size_t i = 0; i < metricValueCount; ++i) {
                copyValue(maximumValues[i], pCalculatedData[i]);
            }
            break;

        default:
            result = ZE_RESULT_ERROR_UNKNOWN;
            break;
        }
    }

    return result;
}

ze_result_t OaMetricGroupImp::initialize(const zet_metric_group_properties_t &sourceProperties,
                                         MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                         MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                         const std::vector<Metric *> &groupMetrics,
                                         OaMetricSourceImp &metricSource) {
    copyProperties(sourceProperties, properties);
    pReferenceMetricSet = &metricSet;
    pReferenceConcurrentGroup = &concurrentGroup;
    metrics = groupMetrics;
    return ZE_RESULT_SUCCESS;
}

uint32_t OaMetricGroupImp::getRawReportSize() {
    auto pMetricSetParams = pReferenceMetricSet->GetParams();

    return ((properties.samplingType & ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) == ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED)
               ? pMetricSetParams->RawReportSize
               : pMetricSetParams->QueryReportSize;
}

std::vector<MetricGroupImp *> &OaMetricGroupImp::getMetricGroups() {
    return metricGroups;
}

void OaMetricGroupImp::copyProperties(const zet_metric_group_properties_t &source,
                                      zet_metric_group_properties_t &destination) {
    destination = source;
    memcpy_s(destination.name, sizeof(destination.name),
             source.name, sizeof(destination.name));
    memcpy_s(destination.description, sizeof(destination.description),
             source.description, sizeof(destination.description));
}

void OaMetricGroupImp::copyValue(const MetricsDiscovery::TTypedValue_1_0 &source,
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
        DEBUG_BREAK_IF(true);
        break;
    }
}

const MetricEnumeration &OaMetricGroupImp::getMetricEnumeration() const {
    return getMetricSource()->getMetricEnumeration();
}

void OaMetricGroupImp::setCachedExportDataHeapSize(size_t size) {
    for (auto &metricGroupHanlde : metricGroups) {
        auto metricGroup = L0::MetricGroup::fromHandle(metricGroupHanlde);
        auto oaMetricGroupImp = static_cast<OaMetricGroupImp *>(metricGroup);
        oaMetricGroupImp->setCachedExportDataHeapSize(size);
    }
    cachedExportDataHeapSize = size;
}

ze_result_t OaMetricImp::getProperties(zet_metric_properties_t *pProperties) {
    copyProperties(properties, *pProperties);
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricImp::initialize(const zet_metric_properties_t &sourceProperties) {
    copyProperties(sourceProperties, properties);
    return ZE_RESULT_SUCCESS;
}

void OaMetricImp::copyProperties(const zet_metric_properties_t &source,
                                 zet_metric_properties_t &destination) {
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

MetricGroup *OaMetricGroupImp::create(zet_metric_group_properties_t &properties,
                                      MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                      MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                      const std::vector<Metric *> &metrics,
                                      MetricSource &metricSource) {
    auto pMetricGroup = new OaMetricGroupImp(metricSource);
    UNRECOVERABLE_IF(pMetricGroup == nullptr);
    pMetricGroup->initialize(properties, metricSet, concurrentGroup, metrics, static_cast<OaMetricSourceImp &>(metricSource));
    pMetricGroup->isPredefined = true;
    return pMetricGroup;
}

Metric *OaMetricImp::create(MetricSource &metricSource, zet_metric_properties_t &properties) {
    auto pMetric = new OaMetricImp(metricSource);
    UNRECOVERABLE_IF(pMetric == nullptr);
    pMetric->initialize(properties);
    pMetric->isPredefined = true;
    return pMetric;
}

} // namespace L0
