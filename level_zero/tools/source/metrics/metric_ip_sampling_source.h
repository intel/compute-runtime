/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

namespace L0 {

struct IpSamplingMetricImp;
struct IpSamplingMetricGroupImp;
struct IpSamplingMetricStreamerImp;
struct IpSamplingCalculation;

class IpSamplingMetricSourceImp : public MetricSource {

  public:
    IpSamplingMetricSourceImp(const MetricDeviceContext &metricDeviceContext);
    ~IpSamplingMetricSourceImp() override = default;
    void enable() override;
    bool isAvailable() override;
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) override;
    ze_result_t appendMetricMemoryBarrier(CommandList &commandList) override;
    ze_result_t activateMetricGroupsAlreadyDeferred() override;
    ze_result_t activateMetricGroupsPreferDeferred(const uint32_t count,
                                                   zet_metric_group_handle_t *phMetricGroups) override;
    ze_result_t metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                          uint32_t *pConcurrentGroupCount,
                                          uint32_t *pCountPerConcurrentGroup) override;
    bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const;
    void setMetricOsInterface(std::unique_ptr<MetricIpSamplingOsInterface> &metricIPSamplingpOsInterface);
    static std::unique_ptr<IpSamplingMetricSourceImp> create(const MetricDeviceContext &metricDeviceContext);
    MetricIpSamplingOsInterface *getMetricOsInterface() { return metricIPSamplingpOsInterface.get(); }
    IpSamplingMetricStreamerImp *pActiveStreamer = nullptr;
    const MetricDeviceContext &getMetricDeviceContext() const { return metricDeviceContext; }
    ze_result_t handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup,
                                                    zet_metric_group_properties_t *pBaseProperties,
                                                    void *pNext) override;
    ze_result_t createMetricGroupsFromMetrics(std::vector<zet_metric_handle_t> &metricList,
                                              const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                              const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                              uint32_t *maxMetricGroupCount,
                                              std::vector<zet_metric_group_handle_t> &metricGroupList) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t appendMarker(zet_command_list_handle_t hCommandList, zet_metric_group_handle_t hMetricGroup, uint32_t value) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    void setActivationTracker(MultiDomainDeferredActivationTracker *inputActivationTracker) {
        activationTracker.reset(inputActivationTracker);
    }
    ze_result_t calcOperationCreate(MetricDeviceContext &metricDeviceContext,
                                    zet_intel_metric_calculation_exp_desc_t *pCalculationDesc,
                                    const std::vector<MetricScopeImp *> &metricScopes,
                                    zet_intel_metric_calculation_operation_exp_handle_t *phCalculationOperation) override;

    uint32_t metricSourceCount = 0;
    bool canDisable() override;
    void initMetricScopes(MetricDeviceContext &metricDeviceContext) override;

    std::unique_ptr<IpSamplingCalculation> ipSamplingCalculation = nullptr;

  protected:
    ze_result_t cacheMetricGroup();
    bool isEnabled = false;

    const MetricDeviceContext &metricDeviceContext;
    std::unique_ptr<MetricIpSamplingOsInterface> metricIPSamplingpOsInterface = nullptr;
    std::unique_ptr<MetricGroup> cachedMetricGroup = nullptr;
    std::unique_ptr<MultiDomainDeferredActivationTracker> activationTracker{};
    ze_result_t getTimerResolution(uint64_t &resolution);
    ze_result_t getTimestampValidBits(uint64_t &validBits);
};

struct IpSamplingMetricGroupBase : public MetricGroupImp {
    IpSamplingMetricGroupBase(MetricSource &metricSource) : MetricGroupImp(metricSource) {}
    static constexpr uint32_t rawReportSize = 64u;
    bool activate() override { return true; }
    bool deactivate() override { return true; };
    ze_result_t metricQueryPoolCreate(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        const zet_metric_query_pool_desc_t *desc,
        zet_metric_query_pool_handle_t *phMetricQueryPool) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t getExportData(const uint8_t *pRawData, size_t rawDataSize, size_t *pExportDataSize, uint8_t *pExportData) override;
    ze_result_t addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t removeMetric(zet_metric_handle_t hMetric) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t close() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t destroy() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    static bool isMultiDeviceCaptureData(const size_t rawDataSize, const uint8_t *pRawData);
    IpSamplingMetricSourceImp &getMetricSource() { return static_cast<IpSamplingMetricSourceImp &>(metricSource); }
};

struct IpSamplingMetricGroupImp : public IpSamplingMetricGroupBase {
    IpSamplingMetricGroupImp(IpSamplingMetricSourceImp &metricSource, std::vector<IpSamplingMetricImp> &metrics);
    ~IpSamplingMetricGroupImp() override = default;

    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override;
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                      const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pMetricValues) override;
    ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                         const uint8_t *pRawData, uint32_t *pSetCount,
                                         uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                         zet_typed_value_t *pMetricValues) override;
    ze_result_t getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                       uint64_t *globalTimestamp,
                                       uint64_t *metricTimestamp) override;
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override;
    ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) override;
    static std::unique_ptr<IpSamplingMetricGroupImp> create(IpSamplingMetricSourceImp &metricSource,
                                                            std::vector<IpSamplingMetricImp> &ipSamplingMetrics);

  private:
    std::vector<std::unique_ptr<IpSamplingMetricImp>> metrics = {};
    zet_metric_group_properties_t properties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
};

struct MultiDeviceIpSamplingMetricGroupImp : public IpSamplingMetricGroupBase {

    MultiDeviceIpSamplingMetricGroupImp(MetricSource &metricSource, std::vector<IpSamplingMetricGroupImp *> &subDeviceMetricGroup) : IpSamplingMetricGroupBase(metricSource), subDeviceMetricGroup(subDeviceMetricGroup) {
        isMultiDevice = true;
    };
    ~MultiDeviceIpSamplingMetricGroupImp() override = default;
    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override;
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                      const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pMetricValues) override;
    ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                         const uint8_t *pRawData, uint32_t *pSetCount,
                                         uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                         zet_typed_value_t *pMetricValues) override;
    ze_result_t getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                       uint64_t *globalTimestamp,
                                       uint64_t *metricTimestamp) override;
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override;
    ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) override;
    static std::unique_ptr<MultiDeviceIpSamplingMetricGroupImp> create(MetricSource &metricSource, std::vector<IpSamplingMetricGroupImp *> &subDeviceMetricGroup);

  private:
    void closeSubDeviceStreamers(std::vector<IpSamplingMetricStreamerImp *> &subDeviceStreamers);
    std::vector<IpSamplingMetricGroupImp *> subDeviceMetricGroup = {};
};

struct IpSamplingMetricImp : public MetricImp {
    ~IpSamplingMetricImp() override = default;
    IpSamplingMetricImp(MetricSource &metricSource, zet_metric_properties_t &properties);
    ze_result_t getProperties(zet_metric_properties_t *pProperties) override;
    ze_result_t destroy() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

  private:
    zet_metric_properties_t properties;
};

struct IpSamplingCalculation {

    IpSamplingCalculation(L0::L0GfxCoreHelper &gfxCoreHelper, IpSamplingMetricSourceImp &metricSource)
        : gfxCoreHelper(gfxCoreHelper), metricSource(metricSource) {}
    ~IpSamplingCalculation() = default;

    static bool isMultiDeviceCaptureData(const size_t rawDataSize, const uint8_t *pRawData);

    ze_result_t getMetricCount(const uint8_t *pRawData, const size_t rawDataSize,
                               uint32_t &metricValueCount);
    ze_result_t getMetricCountSubDevIndex(const uint8_t *pMultiMetricData, const size_t rawDataSize,
                                          uint32_t &metricValueCount, const uint32_t setIndex);

    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, const size_t rawDataSize,
                                      const uint8_t *pRawData, uint32_t &metricValueCount,
                                      zet_typed_value_t *pCalculatedData);
    ze_result_t calculateMetricValuesSubDevIndex(const zet_metric_group_calculation_type_t type, const size_t rawDataSize,
                                                 const uint8_t *pMultiMetricData, uint32_t &metricValueCount,
                                                 zet_typed_value_t *pCalculatedData, const uint32_t setIndex);

    ze_result_t calculateMetricForSubdevice(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                            const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                            zet_typed_value_t *pMetricValues);

    void fillStallDataMap(const size_t rawDataSize, const uint8_t *pRawData, size_t *processedSize,
                          L0GfxCoreHelper &l0GfxCoreHelper,
                          std::map<uint64_t, void *> &stallReportDataMap,
                          bool *dataOverflow);

  protected:
    L0::L0GfxCoreHelper &gfxCoreHelper;
    IpSamplingMetricSourceImp &metricSource;
};

struct IpSamplingMetricDataHeader {
    static constexpr uint32_t magicValue = 0xFEEDBCBA;
    uint32_t magic;
    uint32_t rawDataSize;
    uint32_t setIndex;
    uint32_t reserved1;
};

template <>
IpSamplingMetricSourceImp &MetricDeviceContext::getMetricSource<IpSamplingMetricSourceImp>() const;

} // namespace L0
