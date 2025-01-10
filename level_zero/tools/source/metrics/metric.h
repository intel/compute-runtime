/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/event/event.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"
#include "level_zero/zet_intel_gpu_metric.h"
#include <level_zero/zet_api.h>

#include "metrics_discovery_api.h"

#include <map>
#include <vector>

struct _zet_metric_group_handle_t {};
struct _zet_metric_handle_t {};
struct _zet_metric_streamer_handle_t {};
struct _zet_metric_query_pool_handle_t {};
struct _zet_metric_query_handle_t {};
struct _zet_metric_programmable_exp_handle_t {};

namespace L0 {

struct METRICS_LOG_BITMASK {                    // NOLINT(readability-identifier-naming)
    constexpr static int32_t LOG_ERROR{1};      // NOLINT(readability-identifier-naming)
    constexpr static int32_t LOG_INFO{1 << 1};  // NOLINT(readability-identifier-naming)
    constexpr static int32_t LOG_DEBUG{1 << 2}; // NOLINT(readability-identifier-naming)
};

#define METRICS_LOG(out, ...) \
    NEO::printDebugString(true, out, __VA_ARGS__);

#define METRICS_LOG_ERR(str, ...)                                                                               \
    if (NEO::debugManager.flags.PrintL0MetricLogs.get() & METRICS_LOG_BITMASK::LOG_ERROR) {                     \
        METRICS_LOG(stderr, "\n\nL0Metrics[E][@fn:%s,ln:%d]: " str "\n\n", __FUNCTION__, __LINE__, __VA_ARGS__) \
    }

#define METRICS_LOG_INFO(str, ...)                                                         \
    if (NEO::debugManager.flags.PrintL0MetricLogs.get() & METRICS_LOG_BITMASK::LOG_INFO) { \
        METRICS_LOG(stdout, "L0Metrics[I]: " str "\n", __VA_ARGS__)                        \
    }

#define METRICS_LOG_DBG(str, ...)                                                                         \
    if (NEO::debugManager.flags.PrintL0MetricLogs.get() & METRICS_LOG_BITMASK::LOG_DEBUG) {               \
        METRICS_LOG(stdout, "L0Metrics[D][@fn:%s,ln:%d]: " str "\n", __FUNCTION__, __LINE__, __VA_ARGS__) \
    }

#define METRICS_SAMPLING_TYPE_TIME_EVENT_BASED (static_cast<zet_metric_group_sampling_type_flag_t>(ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED | ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED)) // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-12901

struct CommandList;
struct MetricStreamer;
struct MetricProgrammable;

static constexpr uint64_t nsecPerSec = 1000000000ull;

class MetricSource {
  public:
    static constexpr uint32_t metricSourceTypeUndefined = 0u;
    static constexpr uint32_t metricSourceTypeOa = 1u;
    static constexpr uint32_t metricSourceTypeIpSampling = 2u;

    virtual void enable() = 0;
    virtual bool isAvailable() = 0;
    virtual ze_result_t appendMetricMemoryBarrier(CommandList &commandList) = 0;
    virtual ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual ze_result_t activateMetricGroupsPreferDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) = 0;
    virtual ze_result_t activateMetricGroupsAlreadyDeferred() = 0;
    virtual ze_result_t metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables) = 0;
    ze_result_t metricGroupCreate(const char name[ZET_MAX_METRIC_GROUP_NAME],
                                  const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                  zet_metric_group_sampling_type_flag_t samplingType,
                                  zet_metric_group_handle_t *pMetricGroupHandle) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    };
    virtual ze_result_t getConcurrentMetricGroups(std::vector<zet_metric_group_handle_t> &hMetricGroups,
                                                  uint32_t *pConcurrentGroupCount,
                                                  uint32_t *pCountPerConcurrentGroup) = 0;
    virtual ~MetricSource() = default;
    uint32_t getType() const {
        return type;
    }
    virtual ze_result_t handleMetricGroupExtendedProperties(zet_metric_group_handle_t hMetricGroup, void *pNext) = 0;
    virtual ze_result_t createMetricGroupsFromMetrics(std::vector<zet_metric_handle_t> &metricList,
                                                      const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                                      const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                      uint32_t *maxMetricGroupCount,
                                                      std::vector<zet_metric_group_handle_t> &metricGroupList) = 0;

  protected:
    uint32_t type = MetricSource::metricSourceTypeUndefined;
};

class MultiDomainDeferredActivationTracker {
  public:
    MultiDomainDeferredActivationTracker(uint32_t subDeviceIndex) : subDeviceIndex(subDeviceIndex) {}
    virtual ~MultiDomainDeferredActivationTracker() = default;
    ze_result_t activateMetricGroupsAlreadyDeferred();
    virtual bool activateMetricGroupsDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups);
    bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const;
    bool isMetricGroupActivatedInHw() const;

  protected:
    void deActivateDomain(uint32_t domain);
    void deActivateAllDomains();

    std::map<uint32_t, std::pair<zet_metric_group_handle_t, bool>> domains = {};
    uint32_t subDeviceIndex{};
};

class MetricDeviceContext {

  public:
    MetricDeviceContext(Device &device);
    virtual ~MetricDeviceContext() {}
    ze_result_t metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups);
    ze_result_t activateMetricGroupsPreferDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups);
    ze_result_t activateMetricGroups();
    ze_result_t appendMetricMemoryBarrier(CommandList &commandList);
    ze_result_t metricProgrammableGet(uint32_t *pCount, zet_metric_programmable_exp_handle_t *phMetricProgrammables);
    ze_result_t metricGroupCreate(const char name[ZET_MAX_METRIC_GROUP_NAME],
                                  const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                  zet_metric_group_sampling_type_flag_t samplingType,
                                  zet_metric_group_handle_t *pMetricGroupHandle);
    bool isImplicitScalingCapable() const;
    Device &getDevice() const;
    uint32_t getSubDeviceIndex() const;
    template <typename T>
    T &getMetricSource() const;
    void setSubDeviceIndex(uint32_t subDeviceIndex) { this->subDeviceIndex = subDeviceIndex; }

    static std::unique_ptr<MetricDeviceContext> create(Device &device);
    static ze_result_t enableMetricApi();
    ze_result_t getConcurrentMetricGroups(uint32_t metricGroupCount, zet_metric_group_handle_t *phMetricGroups,
                                          uint32_t *pConcurrentGroupCount, uint32_t *pCountPerConcurrentGroup);

    bool isProgrammableMetricsEnabled = true;
    ze_result_t createMetricGroupsFromMetrics(uint32_t metricCount, zet_metric_handle_t *phMetrics,
                                              const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                              const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                              uint32_t *pMetricGroupCount,
                                              zet_metric_group_handle_t *phMetricGroups);
    bool areMetricGroupsFromSameDeviceHierarchy(uint32_t count, zet_metric_group_handle_t *phMetricGroups);

  protected:
    std::map<uint32_t, std::unique_ptr<MetricSource>> metricSources;

  private:
    bool enable();
    struct Device &device;
    bool multiDeviceCapable = false;
    uint32_t subDeviceIndex = 0;
};

struct Metric : _zet_metric_handle_t {
    virtual ~Metric() = default;

    virtual ze_result_t getProperties(zet_metric_properties_t *pProperties) = 0;
    virtual ze_result_t destroy() = 0;
    static Metric *fromHandle(zet_metric_handle_t handle) { return static_cast<Metric *>(handle); }
    inline zet_metric_handle_t toHandle() { return this; }
};

struct MultiDeviceMetricImp;

struct MetricImp : public Metric {

    MetricSource &getMetricSource() {
        return metricSource;
    }
    ~MetricImp() override = default;
    MetricImp(MetricSource &metricSource) : metricSource(metricSource) {}
    bool isImmutable() { return isPredefined; }
    bool isRootDevice() { return isMultiDevice; }

    MultiDeviceMetricImp *getRootDevMetric() { return rootDeviceMetricImp; };
    void setRootDevMetric(MultiDeviceMetricImp *inputRootDevMetricImp) {
        rootDeviceMetricImp = inputRootDevMetricImp;
    };

  protected:
    MetricSource &metricSource;
    bool isPredefined = true;
    bool isMultiDevice = false;
    MultiDeviceMetricImp *rootDeviceMetricImp = nullptr;
};

struct MultiDeviceMetricImp : public MetricImp {
    ~MultiDeviceMetricImp() override = default;
    MultiDeviceMetricImp(MetricSource &metricSource, std::vector<MetricImp *> &subDeviceMetrics) : MetricImp(metricSource), subDeviceMetrics(subDeviceMetrics) {
        isPredefined = true;
        isMultiDevice = true;
        rootDeviceMetricImp = nullptr;

        for (auto &subDeviceMetric : subDeviceMetrics) {
            static_cast<MetricImp *>(subDeviceMetric)->setRootDevMetric(this);
        }
    }

    ze_result_t destroy() override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t getProperties(zet_metric_properties_t *pProperties) override;
    static MultiDeviceMetricImp *create(MetricSource &metricSource, std::vector<MetricImp *> &subDeviceMetrics);
    MetricImp *getMetricAtSubDeviceIndex(uint32_t index);

  protected:
    std::vector<MetricImp *> subDeviceMetrics{};
};

struct MetricGroup : _zet_metric_group_handle_t {
    virtual ~MetricGroup() = default;
    MetricGroup() {}

    virtual ze_result_t getProperties(zet_metric_group_properties_t *pProperties) = 0;
    virtual ze_result_t metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) = 0;
    virtual ze_result_t calculateMetricValues(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                              const uint8_t *pRawData, uint32_t *pMetricValueCount,
                                              zet_typed_value_t *pMetricValues) = 0;
    virtual ze_result_t calculateMetricValuesExp(const zet_metric_group_calculation_type_t type, size_t rawDataSize,
                                                 const uint8_t *pRawData, uint32_t *pSetCount,
                                                 uint32_t *pTotalMetricValueCount, uint32_t *pMetricCounts,
                                                 zet_typed_value_t *pMetricValues) = 0;
    virtual ze_result_t getMetricTimestampsExp(const ze_bool_t synchronizedWithHost,
                                               uint64_t *globalTimestamp,
                                               uint64_t *metricTimestamp) = 0;
    virtual ze_result_t addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) = 0;
    virtual ze_result_t removeMetric(zet_metric_handle_t hMetric) = 0;
    virtual ze_result_t close() = 0;
    virtual ze_result_t destroy() = 0;
    static MetricGroup *fromHandle(zet_metric_group_handle_t handle) {
        return static_cast<MetricGroup *>(handle);
    }
    zet_metric_group_handle_t toHandle() { return this; }
    virtual bool activate() = 0;
    virtual bool deactivate() = 0;
    virtual zet_metric_group_handle_t getMetricGroupForSubDevice(const uint32_t subDeviceIndex) = 0;
    virtual ze_result_t streamerOpen(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        zet_metric_streamer_desc_t *desc,
        ze_event_handle_t hNotificationEvent,
        zet_metric_streamer_handle_t *phMetricStreamer) = 0;
    virtual ze_result_t metricQueryPoolCreate(
        zet_context_handle_t hContext,
        zet_device_handle_t hDevice,
        const zet_metric_query_pool_desc_t *desc,
        zet_metric_query_pool_handle_t *phMetricQueryPool) = 0;
    virtual ze_result_t getExportData(const uint8_t *pRawData, size_t rawDataSize, size_t *pExportDataSize,
                                      uint8_t *pExportData) = 0;
};

struct MetricGroupImp : public MetricGroup {

    MetricSource &getMetricSource() {
        return metricSource;
    }
    ~MetricGroupImp() override = default;
    MetricGroupImp(MetricSource &metricSource) : metricSource(metricSource) {}
    bool isImmutable() { return isPredefined; }
    bool isRootDevice() { return isMultiDevice; }

  protected:
    MetricSource &metricSource;
    bool isPredefined = true;
    bool isMultiDevice = false;
};

struct MetricGroupCalculateHeader {
    static constexpr uint32_t magicValue = 0xFFFEDCBA;

    uint32_t magic;
    uint32_t dataCount;
    uint32_t rawDataOffsets;
    uint32_t rawDataSizes;
    uint32_t rawDataOffset;
};

struct MetricCollectorEventNotify {
    virtual Event::State getNotificationState() = 0;
    void attachEvent(ze_event_handle_t hNotificationEvent);
    void detachEvent();

  protected:
    Event *pNotificationEvent = nullptr;
};

struct MetricStreamer : _zet_metric_streamer_handle_t, MetricCollectorEventNotify {
    virtual ~MetricStreamer() = default;
    virtual ze_result_t readData(uint32_t maxReportCount, size_t *pRawDataSize,
                                 uint8_t *pRawData) = 0;
    virtual ze_result_t close() = 0;
    static MetricStreamer *fromHandle(zet_metric_streamer_handle_t handle) {
        return static_cast<MetricStreamer *>(handle);
    }
    virtual ze_result_t appendStreamerMarker(CommandList &commandList, uint32_t value) = 0;
    inline zet_metric_streamer_handle_t toHandle() { return this; }
};

struct MetricQueryPool : _zet_metric_query_pool_handle_t {
    virtual ~MetricQueryPool() = default;

    virtual ze_result_t destroy() = 0;
    virtual ze_result_t metricQueryCreate(uint32_t index,
                                          zet_metric_query_handle_t *phMetricQuery) = 0;

    static MetricQueryPool *fromHandle(zet_metric_query_pool_handle_t handle);

    zet_metric_query_pool_handle_t toHandle();
};

struct MetricQuery : _zet_metric_query_handle_t {
    virtual ~MetricQuery() = default;

    virtual ze_result_t appendBegin(CommandList &commandList) = 0;
    virtual ze_result_t appendEnd(CommandList &commandList, ze_event_handle_t hSignalEvent,
                                  uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) = 0;
    virtual ze_result_t getData(size_t *pRawDataSize, uint8_t *pRawData) = 0;

    virtual ze_result_t reset() = 0;
    virtual ze_result_t destroy() = 0;

    static MetricQuery *fromHandle(zet_metric_query_handle_t handle);
    zet_metric_query_handle_t toHandle();
};

struct MetricProgrammable : _zet_metric_programmable_exp_handle_t {
    virtual ~MetricProgrammable() = default;
    virtual ze_result_t getProperties(zet_metric_programmable_exp_properties_t *pProperties) = 0;
    virtual ze_result_t getParamInfo(uint32_t *pParameterCount, zet_metric_programmable_param_info_exp_t *pParameterInfo) = 0;
    virtual ze_result_t getParamValueInfo(uint32_t parameterOrdinal, uint32_t *pValueInfoCount,
                                          zet_metric_programmable_param_value_info_exp_t *pValueInfo) = 0;
    virtual ze_result_t createMetric(zet_metric_programmable_param_value_exp_t *pParameterValues,
                                     uint32_t parameterCount, const char name[ZET_MAX_METRIC_NAME],
                                     const char description[ZET_MAX_METRIC_DESCRIPTION],
                                     uint32_t *pMetricHandleCount, zet_metric_handle_t *phMetricHandles) = 0;
    static MetricProgrammable *fromHandle(zet_metric_programmable_exp_handle_t handle) {
        return static_cast<MetricProgrammable *>(handle);
    }
    zet_metric_programmable_exp_handle_t toHandle() { return this; }
};

struct MetricCreated {
    virtual ~MetricCreated() = default;
    void incrementRefCount() {
        refCount++;
    }
    void decrementRefCount() {
        refCount -= 1;
        DEBUG_BREAK_IF(refCount < 0);
        refCount = std::max(0, refCount);
    }

  protected:
    int32_t refCount = 0;
};

struct MetricGroupUserDefined {
    virtual ~MetricGroupUserDefined() = default;
    static void updateErrorString(std::string &errorString, size_t *errorStringSize, char *pErrorString);
};

struct HomogeneousMultiDeviceMetricProgrammable : public MetricProgrammable {
    HomogeneousMultiDeviceMetricProgrammable(MetricSource &metricSource,
                                             std::vector<MetricProgrammable *> &subDeviceProgrammables) : metricSource(metricSource), subDeviceProgrammables(subDeviceProgrammables) {}
    ~HomogeneousMultiDeviceMetricProgrammable() override = default;
    ze_result_t getProperties(zet_metric_programmable_exp_properties_t *pProperties) override;
    ze_result_t getParamInfo(uint32_t *pParameterCount, zet_metric_programmable_param_info_exp_t *pParameterInfo) override;
    ze_result_t getParamValueInfo(uint32_t parameterOrdinal, uint32_t *pValueInfoCount,
                                  zet_metric_programmable_param_value_info_exp_t *pValueInfo) override;
    ze_result_t createMetric(zet_metric_programmable_param_value_exp_t *pParameterValues,
                             uint32_t parameterCount, const char name[ZET_MAX_METRIC_NAME],
                             const char description[ZET_MAX_METRIC_DESCRIPTION],
                             uint32_t *pMetricHandleCount, zet_metric_handle_t *phMetricHandles) override;
    static MetricProgrammable *create(MetricSource &metricSource,
                                      std::vector<MetricProgrammable *> &subDeviceProgrammables);

  protected:
    MetricSource &metricSource;
    std::vector<MetricProgrammable *> subDeviceProgrammables{};
};

struct HomogeneousMultiDeviceMetricCreated : public MultiDeviceMetricImp {
    ~HomogeneousMultiDeviceMetricCreated() override {}
    HomogeneousMultiDeviceMetricCreated(MetricSource &metricSource, std::vector<MetricImp *> &subDeviceMetrics) : MultiDeviceMetricImp(metricSource, subDeviceMetrics) {
        isPredefined = false;
        isMultiDevice = true;
    }
    ze_result_t destroy() override;
    static MetricImp *create(MetricSource &metricSource, std::vector<MetricImp *> &subDeviceMetrics);
};

// MetricGroup.
ze_result_t metricGroupGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups);

// MetricStreamer.
ze_result_t metricStreamerOpen(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                               zet_metric_streamer_desc_t *pDesc, ze_event_handle_t hNotificationEvent,
                               zet_metric_streamer_handle_t *phMetricStreamer);

// MetricQueryPool.
ze_result_t metricQueryPoolCreate(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                                  const zet_metric_query_pool_desc_t *pDesc, zet_metric_query_pool_handle_t *phMetricQueryPool);

// MetricProgrammable
ze_result_t metricProgrammableGet(zet_device_handle_t hDevice, uint32_t *pCount,
                                  zet_metric_programmable_exp_handle_t *phMetricProgrammables);

ze_result_t metricProgrammableGetProperties(zet_metric_programmable_exp_handle_t hMetricProgrammable,
                                            zet_metric_programmable_exp_properties_t *pProperties);

ze_result_t metricProgrammableGetParamInfo(zet_metric_programmable_exp_handle_t hMetricProgrammable,
                                           uint32_t *pParameterCount, zet_metric_programmable_param_info_exp_t *pParameterInfo);

ze_result_t metricProgrammableGetParamValueInfo(zet_metric_programmable_exp_handle_t hMetricProgrammable,
                                                uint32_t parameterOrdinal, uint32_t *pValueInfoCount, zet_metric_programmable_param_value_info_exp_t *pValueInfo);

ze_result_t metricCreateFromProgrammable(zet_metric_programmable_exp_handle_t hMetricProgrammable,
                                         zet_metric_programmable_param_value_exp_t *pParameterValues, uint32_t parameterCount,
                                         const char name[ZET_MAX_METRIC_NAME], const char description[ZET_MAX_METRIC_DESCRIPTION],
                                         uint32_t *pMetricHandleCount, zet_metric_handle_t *phMetricHandles);

ze_result_t metricTracerCreate(zet_context_handle_t hContext, zet_device_handle_t hDevice, uint32_t metricGroupCount,
                               zet_metric_group_handle_t *phMetricGroups, zet_metric_tracer_exp_desc_t *desc, ze_event_handle_t hNotificationEvent,
                               zet_metric_tracer_exp_handle_t *phMetricTracer);

ze_result_t metricTracerDestroy(zet_metric_tracer_exp_handle_t hMetricTracer);

ze_result_t metricTracerEnable(zet_metric_tracer_exp_handle_t hMetricTracer, ze_bool_t synchronous);

ze_result_t metricTracerDisable(zet_metric_tracer_exp_handle_t hMetricTracer, ze_bool_t synchronous);

ze_result_t metricTracerReadData(zet_metric_tracer_exp_handle_t hMetricTracer, size_t *pRawDataSize, uint8_t *pRawData);

ze_result_t metricDecoderCreate(zet_metric_tracer_exp_handle_t hMetricTracer, zet_metric_decoder_exp_handle_t *phMetricDecoder);

ze_result_t metricDecoderDestroy(zet_metric_decoder_exp_handle_t hMetricDecoder);

ze_result_t metricDecoderGetDecodableMetrics(zet_metric_decoder_exp_handle_t hMetricDecoder, uint32_t *pCount, zet_metric_handle_t *phMetrics);

ze_result_t metricTracerDecode(zet_metric_decoder_exp_handle_t hMetricDecoder, size_t *pRawDataSize, uint8_t *pRawData,
                               uint32_t metricsCount, zet_metric_handle_t *phMetrics, uint32_t *pSetCount, uint32_t *pMetricEntriesCountPerSet,
                               uint32_t *pMetricEntriesCount, zet_metric_entry_exp_t *pMetricEntries);

} // namespace L0
