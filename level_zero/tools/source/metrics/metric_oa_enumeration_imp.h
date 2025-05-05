/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include <vector>

namespace L0 {

static constexpr std::string_view globalSymbolOaMaxBufferSize = "OABufferMaxSize";
static constexpr std::string_view globalSymbolOaMaxTimestamp = "MaxTimestamp";
static constexpr std::string_view globalSymbolOaGpuTimestampFrequency = "GpuTimestampFrequency";

struct MetricEnumeration {
    MetricEnumeration(OaMetricSourceImp &metricSource);
    virtual ~MetricEnumeration();

    ze_result_t metricGroupGet(uint32_t &count, zet_metric_group_handle_t *phMetricGroups);
    MetricGroup *getMetricGroupByIndex(const uint32_t index);
    uint32_t getMetricGroupCount();

    virtual bool isInitialized();

    virtual ze_result_t loadMetricsDiscovery();
    void getMetricsDiscoveryFilename(std::vector<const char *> &names) const;
    uint32_t getMaxOaBufferSize() const { return maximumOaBufferSize; }
    bool readGlobalSymbol(const char *name, uint32_t &symbolValue);
    bool readGlobalSymbol(const char *name, uint64_t &symbolValue);
    MetricsDiscovery::IMetricsDevice_1_13 *getMdapiDevice() { return pMetricsDevice; }
    MetricsDiscovery::IAdapter_1_13 *getMdapiAdapter() { return pAdapter; }
    MetricsDiscovery::IAdapterGroup_1_13 *getMdapiAdapterGroup() { return pAdapterGroup; }
    OaMetricSourceImp &getMetricSource() { return metricSource; }
    virtual MetricsDiscovery::IAdapter_1_13 *getAdapterFromAdapterGroup(
        MetricsDiscovery::IAdapterGroup_1_13 *adapterGroup,
        uint32_t index) {
        UNRECOVERABLE_IF(pAdapterGroup == nullptr);
        UNRECOVERABLE_IF(pAdapterGroup->GetAdapter(index) == nullptr);
        return pAdapterGroup->GetAdapter(index);
    }
    virtual const MetricsDiscovery::TAdapterGroupParams_1_6 *getAdapterGroupParams(MetricsDiscovery::IAdapterGroup_1_13 *adapterGroup) {
        return adapterGroup->GetParams();
    }
    virtual const MetricsDiscovery::TAdapterParams_1_9 *getAdapterParams(
        MetricsDiscovery::IAdapter_1_13 *pAdapter) {
        return pAdapter->GetParams();
    }

    virtual void openMetricsSubDeviceFromAdapter(MetricsDiscovery::IAdapter_1_13 *pAdapter,
                                                 const uint32_t subDeviceIndex, MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) {
        pAdapter->OpenMetricsSubDevice(subDeviceIndex, metricsDevice);
    }

    virtual void openMetricsDeviceFromAdapter(
        MetricsDiscovery::IAdapter_1_13 *pAdapter,
        MetricsDiscovery::IMetricsDevice_1_13 **metricsDevice) {
        pAdapter->OpenMetricsDevice(metricsDevice);
    }

    virtual MetricsDiscovery::IConcurrentGroup_1_13 *getConcurrentGroupFromDevice(
        MetricsDiscovery::IMetricsDevice_1_13 *metricDevice,
        uint32_t index) {
        return metricDevice->GetConcurrentGroup(index);
    }
    zet_metric_type_t getMetricType(const MetricsDiscovery::TMetricType sourceMetricType) const;
    zet_value_type_t
    getMetricResultType(const MetricsDiscovery::TMetricResultType sourceMetricResultType) const;
    void getL0MetricPropertiesFromMdapiMetric(zet_metric_properties_t &l0MetricProps, MetricsDiscovery::IMetric_1_0 *mdapiMetric);
    void getL0MetricPropertiesFromMdapiInformation(zet_metric_properties_t &l0MetricProps, MetricsDiscovery::IInformation_1_0 *mdapiInformation);
    MetricsDiscovery::IInformationLatest *getOaBufferOverflowInformation() const { return pOaBufferOverflowInformation; }
    ze_result_t metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables);
    MetricsDiscovery::IConcurrentGroup_1_13 *getConcurrentGroup() {
        return pConcurrentGroup;
    }
    void cleanupExtendedMetricInformation();
    void setAdapterGroup(MetricsDiscovery::IAdapterGroup_1_13 *adapterGroup) {
        pAdapterGroup = adapterGroup;
    }
    ze_result_t cacheExtendedMetricInformation(
        MetricsDiscovery::IConcurrentGroup_1_13 &pConcurrentGroup,
        const uint32_t domain);
    void setInitializationState(ze_result_t state) {
        initializationState = state;
    }

  protected:
    ze_result_t initialize();
    virtual ze_result_t openMetricsDiscovery();
    virtual bool getAdapterId(uint32_t &major, uint32_t &minor);
    virtual MetricsDiscovery::IAdapter_1_13 *getMetricsAdapter();
    ze_result_t cleanupMetricsDiscovery();

    bool isMetricProgrammableSupportEnabled() {
        return metricSource.getMetricDeviceContext().isProgrammableMetricsEnabled;
    }
    ze_result_t cacheMetricInformation();
    ze_result_t cacheMetricGroup(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                                 MetricsDiscovery::IConcurrentGroup_1_13 &pConcurrentGroup,
                                 const uint32_t domain,
                                 const zet_metric_group_sampling_type_flag_t samplingType);
    ze_result_t createMetrics(MetricsDiscovery::IMetricSet_1_5 &metricSet,
                              std::vector<Metric *> &metrics);

    // Metrics Discovery types mapping.
    uint32_t getMetricTierNumber(const uint32_t sourceUsageFlagsMask) const;
    zet_metric_type_t
    getMetricType(const MetricsDiscovery::TInformationType sourceInformationType) const;
    zet_metric_group_sampling_type_flags_t getSamplingTypeFromApiMask(const uint32_t apiMask);
    std::vector<MetricProgrammable *> &getProgrammables() {
        return metricProgrammables;
    }

    OaMetricSourceImp &metricSource;
    std::vector<MetricGroup *> metricGroups; // Cached metric groups
    std::vector<MetricProgrammable *> metricProgrammables{};
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;

    // Metrics Discovery API.
    std::unique_ptr<NEO::OsLibrary> hMetricsDiscovery = nullptr;
    MetricsDiscovery::OpenAdapterGroup_fn openAdapterGroup = nullptr;
    MetricsDiscovery::IAdapterGroup_1_13 *pAdapterGroup = nullptr;
    MetricsDiscovery::IAdapter_1_13 *pAdapter = nullptr;
    MetricsDiscovery::IMetricsDevice_1_13 *pMetricsDevice = nullptr;
    uint32_t maximumOaBufferSize = 0u;
    MetricsDiscovery::IInformationLatest *pOaBufferOverflowInformation = nullptr;
    void cacheMetricPrototypes(
        MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
        const uint32_t domain);
    void updateMetricProgrammablesFromPrototypes(
        MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
        const std::vector<MetricsDiscovery::IMetricPrototype_1_13 *> &metricPrototypes,
        uint32_t domain);
    MetricsDiscovery::IConcurrentGroup_1_13 *pConcurrentGroup = nullptr;

  public:
    // Metrics Discovery version should be at least 1.13.
    static const uint32_t requiredMetricsDiscoveryMajorVersion = 1;
    static const uint32_t requiredMetricsDiscoveryMinorVersion = 13;
    static const char *oaConcurrentGroupName;
};

struct OaMetricGroupImp : public MetricGroupImp {
    ~OaMetricGroupImp() override;
    OaMetricGroupImp(MetricSource &metricSource) : MetricGroupImp(metricSource) {}

    ze_result_t getProperties(zet_metric_group_properties_t *pProperties) override;
    ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) override;
    ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize, const uint8_t *pRawData,
                                      uint32_t *pMetricValueCount,
                                      zet_typed_value_t *pCalculatedData) override;
    ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                         const uint8_t *pRawData, uint32_t *pSetCount,
                                         uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                         zet_typed_value_t *pMetricValues) override;
    ze_result_t getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                       uint64_t *globalTimestamp,
                                       uint64_t *metricTimestamp) override;
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
    ze_result_t initialize(const zet_metric_group_properties_t &sourceProperties,
                           MetricsDiscovery::IMetricSet_1_5 &metricSet,
                           MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                           const std::vector<Metric *> &groupMetrics,
                           OaMetricSourceImp &metricSource);

    bool activate() override;
    bool deactivate() override;

    bool activateMetricSet();
    bool deactivateMetricSet();
    void setRootDeviceFlag() {
        isMultiDevice = true;
    }

    static uint32_t getApiMask(const zet_metric_group_sampling_type_flags_t samplingType);
    zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) override;

    // Time based measurements.
    ze_result_t openIoStream(uint32_t &timerPeriodNs, uint32_t &oaBufferSize);
    ze_result_t waitForReports(const uint32_t timeoutMs);
    ze_result_t readIoStream(uint32_t &reportCount, uint8_t &reportData);
    ze_result_t closeIoStream();

    std::vector<MetricGroupImp *> &getMetricGroups();
    ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) override;

    ze_result_t metricQueryPoolCreate(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        const zet_metric_query_pool_desc_t *desc,
        zet_metric_query_pool_handle_t *phMetricQueryPool) override;
    ze_result_t getExportData(const uint8_t *pRawData, size_t rawDataSize, size_t *pExportDataSize,
                              uint8_t *pExportData) override;
    static MetricGroup *create(zet_metric_group_properties_t &properties,
                               MetricsDiscovery::IMetricSet_1_5 &metricSet,
                               MetricsDiscovery::IConcurrentGroup_1_13 &concurrentGroup,
                               const std::vector<Metric *> &metrics,
                               MetricSource &metricSource);
    OaMetricSourceImp *getMetricSource() const { return static_cast<OaMetricSourceImp *>(&metricSource); }
    static ze_result_t getProperties(const zet_metric_group_handle_t handle, zet_metric_group_properties_t *pProperties);
    uint32_t getRawReportSize();
    const MetricEnumeration &getMetricEnumeration() const;
    void setCachedExportDataHeapSize(size_t size);

  protected:
    void copyProperties(const zet_metric_group_properties_t &source,
                        zet_metric_group_properties_t &destination);
    void copyValue(const MetricsDiscovery::TTypedValue_1_0 &source,
                   zet_typed_value_t &destination) const;

    ze_result_t getCalculatedMetricCount(const size_t rawDataSize,
                                         uint32_t &metricValueCount);

    ze_result_t getCalculatedMetricValues(const zet_metric_group_calculation_type_t, const size_t rawDataSize, const uint8_t *pRawData,
                                          uint32_t &metricValueCount,
                                          zet_typed_value_t *pCalculatedData);
    ze_result_t getExportDataHeapSize(size_t &exportDataHeapSize);

    // Cached metrics.
    std::vector<Metric *> metrics;
    zet_metric_group_properties_t properties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
    MetricsDiscovery::IMetricSet_1_5 *pReferenceMetricSet = nullptr;
    MetricsDiscovery::IConcurrentGroup_1_13 *pReferenceConcurrentGroup = nullptr;

    std::vector<MetricGroupImp *> metricGroups;
    size_t cachedExportDataHeapSize = 0;

  private:
    ze_result_t openForDevice(Device *pDevice, zet_metric_streamer_desc_t &desc,
                              zet_metric_streamer_handle_t *phMetricStreamer);
};

struct OaMetricImp : public MetricImp {
    ~OaMetricImp() override{};

    OaMetricImp(MetricSource &metricSource) : MetricImp(metricSource) {}

    ze_result_t getProperties(zet_metric_properties_t *pProperties) override;

    ze_result_t destroy() override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t initialize(const zet_metric_properties_t &sourceProperties);

    static Metric *create(MetricSource &metricSource, zet_metric_properties_t &properties);

  protected:
    void copyProperties(const zet_metric_properties_t &source,
                        zet_metric_properties_t &destination);

    zet_metric_properties_t properties{
        ZET_STRUCTURE_TYPE_METRIC_PROPERTIES};
};

} // namespace L0
