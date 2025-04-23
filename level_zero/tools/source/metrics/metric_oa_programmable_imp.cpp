/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_programmable_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace L0 {

static const std::map<MetricsDiscovery::TOptionDescriptorType, zet_metric_programmable_param_type_exp_t> optionTypeToParameterTypeMap{
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_DISAGGREGATION, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_DISAGGREGATION},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_LATENCY, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_LATENCY},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_UTILIZATION, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_NORMALIZATION_UTILIZATION},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_AVERAGE, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_NORMALIZATION_AVERAGE},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_RATE, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_NORMALIZATION_RATE},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_BYTE, ZET_METRIC_PROGRAMMABLE_PARAM_TYPE_EXP_NORMALIZATION_BYTES}};

static const std::map<MetricsDiscovery::TOptionDescriptorType, uint64_t> optionTypeToDefaultValueMap{
    // Default value for disaggregation is disabled (which means aggregated).
    // However, MDAPI accepts values for disaggregation which represent slice index.
    // So having a special default-value (UINT64_MAX) to represent aggregated case
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_DISAGGREGATION, UINT64_MAX},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_LATENCY, 0u},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_UTILIZATION, 0u},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_AVERAGE, 0u},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_RATE, 0u},
    {MetricsDiscovery::OPTION_DESCRIPTOR_TYPE_NORMALIZATION_BYTE, 0u}};

static const std::map<MetricsDiscovery::TValueType, zet_value_info_type_exp_t> valueTypeToValueInfoTypeMap{
    {MetricsDiscovery::VALUE_TYPE_UINT32, ZET_VALUE_INFO_TYPE_EXP_UINT32},
    {MetricsDiscovery::VALUE_TYPE_UINT64, ZET_VALUE_INFO_TYPE_EXP_UINT64},
    {MetricsDiscovery::VALUE_TYPE_FLOAT, ZET_VALUE_INFO_TYPE_EXP_FLOAT32},
    {MetricsDiscovery::VALUE_TYPE_BOOL, ZET_VALUE_INFO_TYPE_EXP_BOOL8},
    {MetricsDiscovery::VALUE_TYPE_UINT32_RANGE, ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE},
    {MetricsDiscovery::VALUE_TYPE_UINT64_RANGE, ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE}};

ze_result_t OaMetricProgrammableImp::getProperties(zet_metric_programmable_exp_properties_t *pProperties) {

    memcpy(&pProperties->component, properties.component, sizeof(pProperties->component));
    memcpy(pProperties->description, properties.description, sizeof(pProperties->description));
    pProperties->domain = properties.domain;
    memcpy(pProperties->name, properties.name, sizeof(pProperties->name));
    pProperties->tierNumber = properties.tierNumber;
    pProperties->samplingType = properties.samplingType;
    pProperties->parameterCount = properties.parameterCount;
    pProperties->sourceId = properties.sourceId;
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricProgrammableImp::getParamInfo(uint32_t *pParameterCount, zet_metric_programmable_param_info_exp_t *pParameterInfo) {

    if (*pParameterCount == 0 && properties.parameterCount != 0) {
        METRICS_LOG_ERR("%s", "Error while retrieving Parameter info for 0 parameterCount.");
        METRICS_LOG_ERR("%s", "Use zetMetricProgrammableGetPropertiesExp to retrieve parameterCount.");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *pParameterCount = std::min(*pParameterCount, properties.parameterCount);

    for (uint32_t i = 0; i < *pParameterCount; i++) {
        auto optionDescriptor = pPrototype->GetOptionDescriptor(i);
        if (optionDescriptor == nullptr) {
            METRICS_LOG_ERR("%s", "GetOptionDescriptor returned nullptr");
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        strcpy_s(pParameterInfo[i].name, ZET_MAX_METRIC_PROGRAMMABLE_PARAMETER_NAME_EXP, optionDescriptor->SymbolName);
        pParameterInfo[i].valueInfoCount = optionDescriptor->ValidValueCount;

        {
            auto iterator = optionTypeToParameterTypeMap.find(optionDescriptor->Type);
            if (iterator == optionTypeToParameterTypeMap.end()) {
                METRICS_LOG_ERR("Unsupported Option Description Type %d", optionDescriptor->Type);
                DEBUG_BREAK_IF(true);
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            pParameterInfo[i].type = optionTypeToParameterTypeMap.at(optionDescriptor->Type);
        }
        {
            DEBUG_BREAK_IF(optionDescriptor->ValidValueCount == 0);
            auto iterator = valueTypeToValueInfoTypeMap.find(optionDescriptor->ValidValues[0].ValueType);
            if (iterator == valueTypeToValueInfoTypeMap.end()) {
                METRICS_LOG_ERR("Unsupported ValueType Type %d", optionDescriptor->ValidValues[0].ValueType);
                DEBUG_BREAK_IF(true);
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            pParameterInfo[i].valueInfoType = valueTypeToValueInfoTypeMap.at(optionDescriptor->ValidValues[0].ValueType);
            pParameterInfo[i].defaultValue.ui64 = optionTypeToDefaultValueMap.at(optionDescriptor->Type);
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricProgrammableImp::copyMdapiValidValueToL0ValueInfo(zet_value_info_exp_t &l0Value, const MetricsDiscovery::TValidValue_1_13 &mdapiValue) {

    switch (mdapiValue.ValueType) {
    case MetricsDiscovery::VALUE_TYPE_UINT32:
        l0Value.ui32 = mdapiValue.ValueUInt32;
        break;
    case MetricsDiscovery::VALUE_TYPE_UINT64:
        l0Value.ui64 = mdapiValue.ValueUInt64;
        break;
    case MetricsDiscovery::VALUE_TYPE_UINT32_RANGE:
        l0Value.ui64Range.ui64Min = mdapiValue.ValueUInt32Range.Min;
        l0Value.ui64Range.ui64Max = mdapiValue.ValueUInt32Range.Max;
        break;
    case MetricsDiscovery::VALUE_TYPE_UINT64_RANGE:
        l0Value.ui64Range.ui64Min = mdapiValue.ValueUInt64Range.Min;
        l0Value.ui64Range.ui64Max = mdapiValue.ValueUInt64Range.Max;
        break;
    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    };
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricProgrammableImp::getParamValueInfo(uint32_t parameterOrdinal, uint32_t *pValueInfoCount,
                                                       zet_metric_programmable_param_value_info_exp_t *pValueInfo) {

    if (parameterOrdinal >= properties.parameterCount) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto optionDescriptor = pPrototype->GetOptionDescriptor(parameterOrdinal);

    *pValueInfoCount = std::min(*pValueInfoCount, optionDescriptor->ValidValueCount);
    for (uint32_t i = 0; i < *pValueInfoCount; i++) {
        auto status = copyMdapiValidValueToL0ValueInfo(pValueInfo[i].valueInfo, optionDescriptor->ValidValues[i]);
        if (status != ZE_RESULT_SUCCESS) {
            *pValueInfoCount = 0;
            return status;
        }
        snprintf(pValueInfo[i].description, sizeof(pValueInfo[i].description), "%s", properties.name);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricProgrammableImp::copyL0ValueToMdapiTypedValue(MetricsDiscovery::TTypedValue_1_0 &mdapiValue,
                                                                  const zet_value_t &l0Value) {

    switch (mdapiValue.ValueType) {
    case MetricsDiscovery::VALUE_TYPE_UINT32_RANGE:
    case MetricsDiscovery::VALUE_TYPE_UINT32:
        mdapiValue.ValueUInt32 = l0Value.ui32;
        mdapiValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT32;
        break;
    case MetricsDiscovery::VALUE_TYPE_UINT64_RANGE:
    case MetricsDiscovery::VALUE_TYPE_UINT64:
        mdapiValue.ValueUInt64 = l0Value.ui64;
        mdapiValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT64;
        break;
    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    };

    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricProgrammableImp::createMetric(zet_metric_programmable_param_value_exp_t *pParameterValues,
                                                  uint32_t parameterCount, const char name[ZET_MAX_METRIC_NAME],
                                                  const char description[ZET_MAX_METRIC_DESCRIPTION],
                                                  uint32_t *pMetricHandleCount, zet_metric_handle_t *phMetricHandles) {

    if (parameterCount > properties.parameterCount) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // OA generates 1 metric per programmable for 1 combination of parameters
    const uint32_t maxMetricHandleCount = 1u;

    if (*pMetricHandleCount == 0) {
        *pMetricHandleCount = maxMetricHandleCount;
        return ZE_RESULT_SUCCESS;
    }

    // Clone the prototype and change name
    auto clonedPrototype = pPrototype->Clone();
    if (clonedPrototype == nullptr) {
        DEBUG_BREAK_IF(true);
        *pMetricHandleCount = 0;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto mdapiStatus = clonedPrototype->ChangeNames(name, name, description, nullptr);
    if (mdapiStatus != MetricsDiscovery::CC_OK) {
        DEBUG_BREAK_IF(true);
        *pMetricHandleCount = 0;
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Apply the parameter values on the cloned prototype
    for (uint32_t i = 0; i < parameterCount; i++) {

        auto optionDescriptor = pPrototype->GetOptionDescriptor(i);
        if (optionDescriptor == nullptr) {
            *pMetricHandleCount = 0;
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        // If default value, no setting to MDAPI is necessary
        if (pParameterValues[i].value.ui64 ==
            optionTypeToDefaultValueMap.at(optionDescriptor->Type)) {
            continue;
        }

        MetricsDiscovery::TTypedValue_1_0 mdapiValue{};
        mdapiValue.ValueType = optionDescriptor->ValidValues[0].ValueType;
        auto status = copyL0ValueToMdapiTypedValue(mdapiValue, pParameterValues[i].value);
        if (status != ZE_RESULT_SUCCESS) {
            *pMetricHandleCount = 0;
            return status;
        }

        auto mdapiStatus = clonedPrototype->SetOption(optionDescriptor->Type, &mdapiValue);
        if (mdapiStatus != MetricsDiscovery::CC_OK) {
            *pMetricHandleCount = 0;
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    // Prepare metric properties
    auto prototypeParams = clonedPrototype->GetParams();
    zet_metric_properties_t metricProperties{};
    snprintf(metricProperties.component, sizeof(metricProperties.component), "%s",
             prototypeParams->GroupName);
    snprintf(metricProperties.name, sizeof(metricProperties.name), "%s",
             prototypeParams->SymbolName); // To always have a null-terminated string
    snprintf(metricProperties.description, sizeof(metricProperties.description), "%s",
             prototypeParams->LongName);
    snprintf(metricProperties.resultUnits, sizeof(metricProperties.resultUnits), "%s",
             prototypeParams->MetricResultUnits);
    metricProperties.tierNumber = properties.tierNumber;

    auto oaMetricSourceImp = static_cast<OaMetricSourceImp *>(metricSource);
    metricProperties.metricType = oaMetricSourceImp->getMetricEnumeration().getMetricType(prototypeParams->MetricType);
    metricProperties.resultType = oaMetricSourceImp->getMetricEnumeration().getMetricResultType(prototypeParams->ResultType);

    // Create MetricFromProgrammable using the properties
    *phMetricHandles = OaMetricFromProgrammable::create(*oaMetricSourceImp, metricProperties, clonedPrototype, properties.domain, properties.samplingType)->toHandle();
    *pMetricHandleCount = maxMetricHandleCount;

    return ZE_RESULT_SUCCESS;
}

void OaMetricProgrammableImp::initialize(zet_metric_programmable_exp_properties_t &properties,
                                         MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                         MetricsDiscovery::IMetricPrototype_1_13 &prototype,
                                         OaMetricSourceImp &metricSource) {

    memcpy(&this->properties, &properties, sizeof(zet_metric_programmable_exp_properties_t));
    this->pConcurrentGroup = &concurrentGroup;
    this->pPrototype = &prototype;
    this->metricSource = &metricSource;
}

MetricProgrammable *OaMetricProgrammableImp::create(zet_metric_programmable_exp_properties_t &properties,
                                                    MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                                    MetricsDiscovery::IMetricPrototype_1_13 &prototype,
                                                    MetricSource &metricSource) {
    auto pMetricProgrammable = new OaMetricProgrammableImp();
    pMetricProgrammable->initialize(properties, concurrentGroup, prototype, static_cast<OaMetricSourceImp &>(metricSource));
    return pMetricProgrammable;
}

Metric *OaMetricFromProgrammable::create(MetricSource &metricSource, zet_metric_properties_t &properties,
                                         MetricsDiscovery::IMetricPrototype_1_13 *pClonedPrototype,
                                         uint32_t domain,
                                         zet_metric_group_sampling_type_flags_t supportedSamplingTypes) {
    auto pMetric = new OaMetricFromProgrammable(metricSource);
    UNRECOVERABLE_IF(pMetric == nullptr);
    pMetric->initialize(properties);
    pMetric->pClonedPrototype = pClonedPrototype;
    pMetric->isPredefined = false;
    pMetric->domain = domain;
    pMetric->supportedSamplingTypes = supportedSamplingTypes;

    return pMetric;
}

ze_result_t OaMetricFromProgrammable::destroy() {

    // Destroy pClonedPrototype
    if (refCount > 0) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }
    delete this;
    return ZE_RESULT_SUCCESS;
}

MetricGroup *OaMetricGroupUserDefined::create(zet_metric_group_properties_t &properties,
                                              MetricsDiscovery::IMetricSet_1_13 &metricSet,
                                              MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                                              MetricSource &metricSource) {

    auto pMetricGroup = new OaMetricGroupUserDefined(metricSource);
    std::vector<Metric *> groupMetrics{};
    pMetricGroup->initialize(properties, metricSet, concurrentGroup, groupMetrics, static_cast<OaMetricSourceImp &>(metricSource));

    UNRECOVERABLE_IF(pMetricGroup == nullptr);

    return pMetricGroup;
}

void OaMetricGroupUserDefined::removeMetrics(bool immutable, std::vector<Metric *> &removedMetricList) {
    removedMetricList.clear();

    for (auto &metric : metrics) {
        if (immutable == static_cast<OaMetricImp *>(metric)->isImmutable()) {
            removedMetricList.push_back(metric);
        }
    }

    for (auto &metric : removedMetricList) {
        metrics.erase(std::remove(metrics.begin(), metrics.end(), metric), metrics.end());
    }
}

ze_result_t OaMetricGroupUserDefined::addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) {

    if (isActivated == true) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    auto metricImp = static_cast<MetricImp *>(Metric::fromHandle(hMetric));
    if (metricImp->getMetricSource().getType() != MetricSource::metricSourceTypeOa) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto metric = static_cast<OaMetricImp *>(Metric::fromHandle(hMetric));
    if (metric->isImmutable()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto metricFromProgrammable = static_cast<OaMetricFromProgrammable *>(metric);
    if ((metricFromProgrammable->getSupportedSamplingType() & properties.samplingType) != properties.samplingType) {

        std::string errorString("MetricGroup does not support Metric's samplingtype ");
        MetricGroupUserDefined::updateErrorString(errorString, errorStringSize, pErrorString);

        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    // If metric was already added, do nothing
    if (std::find(metrics.begin(), metrics.end(), metric) != metrics.end()) {
        return ZE_RESULT_SUCCESS;
    }

    auto metricSet = getMetricSet();

    // Trying to add to a finalized Metric group.
    if (!isMetricSetOpened) {
        auto mdapiStatus = metricSet->Open();
        if (mdapiStatus != MetricsDiscovery::CC_OK) {
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        isMetricSetOpened = true;
        // Remove immutable metrics, since MDAPI adds them during finalize
        std::vector<Metric *> removedMetricList{};
        removeMetrics(true, removedMetricList);
        // Delete all removed L0 metric handles, which were previously added
        // during finalize, to correspond with MDAPI metrics.
        for (auto &removedMetric : removedMetricList) {
            delete removedMetric;
        }
    }

    auto mdapiStatus = metricSet->AddMetric(metricFromProgrammable->pClonedPrototype);

    if (mdapiStatus != MetricsDiscovery::CC_OK) {

        // Use a default string, since this is not supported by Mdapi
        std::string errorString("Incompatible Metric");
        MetricGroupUserDefined::updateErrorString(errorString, errorStringSize, pErrorString);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    properties.domain = metricFromProgrammable->getDomain();
    metricFromProgrammable->incrementRefCount();
    metrics.push_back(metric);
    readyToActivate = false;

    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricGroupUserDefined::removeMetric(zet_metric_handle_t hMetric) {

    if (isActivated == true) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    auto metricImp = static_cast<MetricImp *>(Metric::fromHandle(hMetric));
    if (metricImp->getMetricSource().getType() != MetricSource::metricSourceTypeOa) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto metric = static_cast<OaMetricImp *>(Metric::fromHandle(hMetric));
    if (metric->isImmutable()) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (std::find(metrics.begin(), metrics.end(), metric) == metrics.end()) {
        return ZE_RESULT_SUCCESS;
    }

    // Trying to remove from a finalized Metric group
    if (!isMetricSetOpened) {
        auto mdapiStatus = getMetricSet()->Open();
        if (mdapiStatus != MetricsDiscovery::CC_OK) {
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        isMetricSetOpened = true;

        // Remove immutable metrics, since MDAPI adds them during finalize
        std::vector<Metric *> removedMetricList{};
        removeMetrics(true, removedMetricList);
        // Delete all removed L0 metric handles, which were previously added
        // during finalize to correspond with MDAPI metrics.
        for (auto &removedMetric : removedMetricList) {
            delete removedMetric;
        }
    }

    auto metricFromProgrammable = static_cast<OaMetricFromProgrammable *>(metric);
    auto mdapiStatus = getMetricSet()->RemoveMetric(metricFromProgrammable->pClonedPrototype);

    metrics.erase(std::remove(metrics.begin(), metrics.end(), metric), metrics.end());
    metricFromProgrammable->decrementRefCount();
    if (mdapiStatus != MetricsDiscovery::CC_OK) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    readyToActivate = false;

    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricGroupUserDefined::close() {

    // Already closed group
    if (readyToActivate == true) {
        return ZE_RESULT_SUCCESS;
    }

    // Groups are closed by default.
    // Open Activated Groups are not possible.
    DEBUG_BREAK_IF(isActivated == true);

    if (metrics.size() == 0) {
        METRICS_LOG_ERR("%s", "metrics count is 0 for the metric group.")
        return ZE_RESULT_NOT_READY;
    }

    auto metricSet = getMetricSet();
    auto mdapiStatus = metricSet->Finalize();
    if (mdapiStatus != MetricsDiscovery::CC_OK) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    isMetricSetOpened = false;

    // Set API filtering to get metric count as per API
    metricSet->SetApiFiltering(OaMetricGroupImp::getApiMask(properties.samplingType));
    auto metricSetParams = metricSet->GetParams();
    auto defaultMetricCount = metricSetParams->MetricsCount - metrics.size();
    OaMetricSourceImp *metricSource = getMetricSource();
    auto &enumeration = metricSource->getMetricEnumeration();

    // Create and add default metrics
    for (int32_t i = static_cast<int32_t>(defaultMetricCount - 1); i >= 0; i--) {
        auto mdapiMetric = metricSet->GetMetric(static_cast<uint32_t>(i));
        zet_metric_properties_t properties = {};
        enumeration.getL0MetricPropertiesFromMdapiMetric(properties, mdapiMetric);
        auto pMetric = OaMetricImp::create(*metricSource, properties);
        UNRECOVERABLE_IF(pMetric == nullptr);

        // Insert it to the beginning
        metrics.insert(metrics.begin(), pMetric);
    }

    // Create and add default information
    for (uint32_t i = 0; i < metricSetParams->InformationCount; i++) {
        auto mdapiInformation = metricSet->GetInformation(i);
        zet_metric_properties_t properties = {};
        enumeration.getL0MetricPropertiesFromMdapiInformation(properties, mdapiInformation);
        auto pMetric = OaMetricImp::create(*metricSource, properties);
        UNRECOVERABLE_IF(pMetric == nullptr);

        metrics.push_back(pMetric);
    }
    properties.metricCount = static_cast<uint32_t>(metrics.size());

    readyToActivate = true;

    // Disable API filtering
    metricSet->SetApiFiltering(MetricsDiscovery::API_TYPE_ALL);
    return ZE_RESULT_SUCCESS;
}

OaMetricGroupUserDefined::~OaMetricGroupUserDefined() {
    // Remove mutable metrics, since they will destroyed
    // by the application
    std::vector<Metric *> removedMetricList{};
    removeMetrics(false, removedMetricList);
    for (auto mutableMetric : removedMetricList) {
        auto metricFromProgrammable = static_cast<OaMetricFromProgrammable *>(L0::Metric::fromHandle(mutableMetric));
        metricFromProgrammable->decrementRefCount();
    }
}

ze_result_t OaMetricGroupUserDefined::destroy() {

    if (isActivated == true) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

bool OaMetricGroupUserDefined::activate() {

    auto status = false;

    if (readyToActivate) {
        status = OaMetricGroupImp::activate();
        if (status == true) {
            isActivated = true;
        }
    }

    return status;
}

bool OaMetricGroupUserDefined::deactivate() {

    bool status = true;
    if (isActivated) {
        status = OaMetricGroupImp::deactivate();
    }
    isActivated = false;
    return status;
}

OaMultiDeviceMetricGroupUserDefined *OaMultiDeviceMetricGroupUserDefined::create(
    MetricSource &metricSource,
    std::vector<MetricGroupImp *> &subDeviceMetricGroups,
    std::vector<MultiDeviceMetricImp *> &inMultiDeviceMetrics) {
    auto metricGroupUserDefined = new OaMultiDeviceMetricGroupUserDefined(metricSource);
    metricGroupUserDefined->metricGroups = subDeviceMetricGroups;
    metricGroupUserDefined->multiDeviceMetrics = inMultiDeviceMetrics;

    metricGroupUserDefined->createdMetricGroupManager = std::make_unique<MultiDeviceCreatedMetricGroupManager>(
        metricSource,
        metricGroupUserDefined->metricGroups,
        metricGroupUserDefined->multiDeviceMetrics);
    return metricGroupUserDefined;
}

ze_result_t OaMultiDeviceMetricGroupUserDefined::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {
    return createdMetricGroupManager->metricGet(pCount, phMetrics);
}

ze_result_t OaMultiDeviceMetricGroupUserDefined::addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) {
    return createdMetricGroupManager->addMetric(hMetric, errorStringSize, pErrorString);
}

ze_result_t OaMultiDeviceMetricGroupUserDefined::removeMetric(zet_metric_handle_t hMetric) {
    return createdMetricGroupManager->removeMetric(hMetric);
}

ze_result_t OaMultiDeviceMetricGroupUserDefined::close() {
    return createdMetricGroupManager->close();
}

ze_result_t OaMultiDeviceMetricGroupUserDefined::destroy() {
    auto status = createdMetricGroupManager->destroy();
    delete this;
    return status;
}

} // namespace L0
