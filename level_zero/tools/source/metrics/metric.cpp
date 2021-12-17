/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/source/inc/ze_intel_gpu.h"
#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_query_imp.h"

#include <map>
#include <utility>

namespace L0 {

struct MetricGroupDomains {

  public:
    MetricGroupDomains(MetricContext &metricContext);
    ze_result_t activateDeferred(const uint32_t subDeviceIndex, const uint32_t count, zet_metric_group_handle_t *phMetricGroups);
    ze_result_t activate();
    ze_result_t deactivate();
    bool isActivated(const zet_metric_group_handle_t hMetricGroup);
    uint32_t getActivatedCount();

  protected:
    bool activateMetricGroupDeferred(const zet_metric_group_handle_t hMetricGroup);
    bool activateEventMetricGroup(const zet_metric_group_handle_t hMetricGroup);

  protected:
    MetricContext &metricContext;

    // Map holds activated domains and associated metric groups.
    // Content: <domain number, pair<metric group, is activated on gpu flag>
    std::map<uint32_t, std::pair<zet_metric_group_handle_t, bool>> domains;
};

struct MetricContextImp : public MetricContext {
  public:
    MetricContextImp(Device &device);
    ~MetricContextImp() override;

    bool loadDependencies() override;
    bool isInitialized() override;
    void setInitializationState(const ze_result_t state) override;
    Device &getDevice() override;
    MetricsLibrary &getMetricsLibrary() override;
    MetricEnumeration &getMetricEnumeration() override;
    MetricStreamer *getMetricStreamer() override;
    void setMetricStreamer(MetricStreamer *pMetricStreamer) override;
    void setMetricsLibrary(MetricsLibrary &metricsLibrary) override;
    void setMetricEnumeration(MetricEnumeration &metricEnumeration) override;

    ze_result_t activateMetricGroups() override;
    ze_result_t activateMetricGroupsDeferred(const uint32_t count,
                                             zet_metric_group_handle_t *phMetricGroups) override;
    bool isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) override;
    bool isMetricGroupActivated() override;

    void setUseCompute(const bool useCompute) override;
    bool isComputeUsed() override;
    uint32_t getSubDeviceIndex() override;
    void setSubDeviceIndex(const uint32_t index) override;
    bool isImplicitScalingCapable() override;

  protected:
    ze_result_t initializationState = ZE_RESULT_ERROR_UNINITIALIZED;
    struct Device &device;
    std::unique_ptr<MetricEnumeration> metricEnumeration = nullptr;
    std::unique_ptr<MetricsLibrary> metricsLibrary = nullptr;
    MetricGroupDomains metricGroupDomains;
    MetricStreamer *pMetricStreamer = nullptr;
    uint32_t subDeviceIndex = 0;
    bool useCompute = false;
    bool implicitScalingCapable = false;
};

MetricContextImp::MetricContextImp(Device &deviceInput)
    : device(deviceInput),
      metricEnumeration(std::unique_ptr<MetricEnumeration>(new (std::nothrow) MetricEnumeration(*this))),
      metricsLibrary(std::unique_ptr<MetricsLibrary>(new (std::nothrow) MetricsLibrary(*this))),
      metricGroupDomains(*this) {

    auto deviceNeo = deviceInput.getNEODevice();
    bool isSubDevice = deviceNeo->isSubDevice();

    subDeviceIndex = isSubDevice
                         ? static_cast<NEO::SubDevice *>(deviceNeo)->getSubDeviceIndex()
                         : 0;

    implicitScalingCapable = !isSubDevice && device.isImplicitScalingCapable();
}

MetricContextImp::~MetricContextImp() {
    metricsLibrary.reset();
    metricEnumeration.reset();
}

bool MetricContextImp::loadDependencies() {
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
                               : ZE_RESULT_ERROR_UNKNOWN);

    return result;
}

bool MetricContextImp::isInitialized() {
    return initializationState == ZE_RESULT_SUCCESS;
}

void MetricContextImp::setInitializationState(const ze_result_t state) {
    initializationState = state;
}

Device &MetricContextImp::getDevice() { return device; }

MetricsLibrary &MetricContextImp::getMetricsLibrary() { return *metricsLibrary; }

MetricEnumeration &MetricContextImp::getMetricEnumeration() { return *metricEnumeration; }

MetricStreamer *MetricContextImp::getMetricStreamer() { return pMetricStreamer; }

void MetricContextImp::setMetricStreamer(MetricStreamer *pMetricStreamer) {
    this->pMetricStreamer = pMetricStreamer;
}

void MetricContextImp::setMetricsLibrary(MetricsLibrary &metricsLibrary) {
    this->metricsLibrary.release();
    this->metricsLibrary.reset(&metricsLibrary);
}

void MetricContextImp::setMetricEnumeration(MetricEnumeration &metricEnumeration) {
    this->metricEnumeration.release();
    this->metricEnumeration.reset(&metricEnumeration);
}

void MetricContextImp::setUseCompute(const bool useCompute) {
    this->useCompute = useCompute;
}

bool MetricContextImp::isComputeUsed() {
    return useCompute;
}

uint32_t MetricContextImp::getSubDeviceIndex() {
    return subDeviceIndex;
}

void MetricContextImp::setSubDeviceIndex(const uint32_t index) {
    subDeviceIndex = index;
}

bool MetricContextImp::isImplicitScalingCapable() {
    return implicitScalingCapable;
}

ze_result_t
MetricContextImp::activateMetricGroupsDeferred(const uint32_t count,
                                               zet_metric_group_handle_t *phMetricGroups) {

    // Activation: postpone until zetMetricStreamerOpen or zeCommandQueueExecuteCommandLists
    // Deactivation: execute immediately.
    return phMetricGroups ? metricGroupDomains.activateDeferred(subDeviceIndex, count, phMetricGroups)
                          : metricGroupDomains.deactivate();
}

bool MetricContextImp::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) {
    return metricGroupDomains.isActivated(hMetricGroup);
}

bool MetricContextImp::isMetricGroupActivated() {
    return metricGroupDomains.getActivatedCount() > 0;
}

ze_result_t MetricContextImp::activateMetricGroups() { return metricGroupDomains.activate(); }

ze_result_t MetricContext::enableMetricApi() {

    if (!isMetricApiAvailable()) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    bool failed = false;

    auto driverHandle = L0::DriverHandle::fromHandle(GlobalDriverHandle);
    auto rootDevices = std::vector<ze_device_handle_t>();
    auto subDevices = std::vector<ze_device_handle_t>();

    // Obtain root devices.
    uint32_t rootDeviceCount = 0;
    driverHandle->getDevice(&rootDeviceCount, nullptr);
    rootDevices.resize(rootDeviceCount);
    driverHandle->getDevice(&rootDeviceCount, rootDevices.data());

    for (auto rootDeviceHandle : rootDevices) {

        // Initialize root device.
        auto rootDevice = static_cast<DeviceImp *>(L0::Device::fromHandle(rootDeviceHandle));
        failed |= !rootDevice->metricContext->loadDependencies();

        // Initialize sub devices.
        for (uint32_t i = 0; i < rootDevice->numSubDevices; ++i) {
            failed |= !rootDevice->subDevices[i]->getMetricContext().loadDependencies();
        }
    }

    return failed
               ? ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE
               : ZE_RESULT_SUCCESS;
}

std::unique_ptr<MetricContext> MetricContext::create(Device &device) {
    auto metricContextImp = new (std::nothrow) MetricContextImp(device);
    std::unique_ptr<MetricContext> metricContext{metricContextImp};
    return metricContext;
}

bool MetricContext::isMetricApiAvailable() {

    std::unique_ptr<NEO::OsLibrary> library = nullptr;

    // Check Metrics Discovery availability.
    library.reset(NEO::OsLibrary::load(MetricEnumeration::getMetricsDiscoveryFilename()));
    if (library == nullptr) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find metrics discovery %s\n", MetricEnumeration::getMetricsDiscoveryFilename());
        return false;
    }

    // Check Metrics Library availability.
    library.reset(NEO::OsLibrary::load(MetricsLibrary::getFilename()));
    if (library == nullptr) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find metrics library %s\n", MetricsLibrary::getFilename());
        return false;
    }

    return true;
}

MetricGroupDomains::MetricGroupDomains(MetricContext &metricContext)
    : metricContext(metricContext) {}

ze_result_t MetricGroupDomains::activateDeferred(const uint32_t subDeviceIndex,
                                                 const uint32_t count,
                                                 zet_metric_group_handle_t *phMetricGroups) {
    // For each metric group:
    for (uint32_t i = 0; i < count; ++i) {
        DEBUG_BREAK_IF(!phMetricGroups[i]);

        zet_metric_group_handle_t handle = phMetricGroups[i];
        auto pMetricGroupImp = static_cast<OaMetricGroupImp *>(MetricGroup::fromHandle(handle));
        if (pMetricGroupImp->getMetricGroups().size() > 0) {
            handle = pMetricGroupImp->getMetricGroups()[subDeviceIndex];
        }

        // Try to associate it with a domain (oa, ...).
        if (!activateMetricGroupDeferred(handle)) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }
    return ZE_RESULT_SUCCESS;
}

bool MetricGroupDomains::activateMetricGroupDeferred(const zet_metric_group_handle_t hMetricGroup) {

    const auto properites = MetricGroup::getProperties(hMetricGroup);
    const auto domain = properites.domain;

    const bool isDomainFree = domains[domain].first == nullptr;
    const bool isSameGroup = domains[domain].first == hMetricGroup;

    // The same metric group has been already associated.
    if (isSameGroup) {
        return true;
    }

    // Domain has been already associated with a different metric group.
    if (!isDomainFree) {
        return false;
    }

    // Associate metric group with domain and mark it as not active.
    // Activation will be performed during zeCommandQueueExecuteCommandLists (query)
    // or zetMetricStreamerOpen (time based sampling).
    domains[domain].first = hMetricGroup;
    domains[domain].second = false;

    return true;
}

ze_result_t MetricGroupDomains::activate() {

    // For each domain.
    for (auto &domain : domains) {

        auto hMetricGroup = domain.second.first;
        bool &metricGroupActive = domain.second.second;
        bool metricGroupEventBased =
            hMetricGroup && MetricGroup::getProperties(hMetricGroup).samplingType ==
                                ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;

        // Activate only event based metric groups.
        // Time based metric group will be activated during zetMetricStreamerOpen.
        if (metricGroupEventBased && !metricGroupActive) {

            metricGroupActive = activateEventMetricGroup(hMetricGroup);

            if (metricGroupActive == false) {
                DEBUG_BREAK_IF(true);
                return ZE_RESULT_ERROR_UNKNOWN;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

bool MetricGroupDomains::activateEventMetricGroup(const zet_metric_group_handle_t hMetricGroup) {
    // Obtain metric group configuration handle from metrics library.
    auto hConfiguration = metricContext.getMetricsLibrary().getConfiguration(hMetricGroup);

    // Validate metrics library handle.
    if (!hConfiguration.IsValid()) {
        DEBUG_BREAK_IF(true);
        return false;
    }

    // Write metric group configuration to gpu.
    const bool result = metricContext.getMetricsLibrary().activateConfiguration(hConfiguration);

    DEBUG_BREAK_IF(!result);
    return result;
}

ze_result_t MetricGroupDomains::deactivate() {
    // Deactivate metric group for each domain.
    for (auto &domain : domains) {

        auto hMetricGroup = domain.second.first;
        bool metricGroupActivatedOnGpu = domain.second.second;

        if (metricGroupActivatedOnGpu) {
            // Only event based metric groups are activated on Gpu.
            DEBUG_BREAK_IF(MetricGroup::getProperties(hMetricGroup).samplingType != ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
            auto hConfiguration = metricContext.getMetricsLibrary().getConfiguration(hMetricGroup);
            // Deactivate metric group configuration using metrics library.
            metricContext.getMetricsLibrary().deactivateConfiguration(hConfiguration);
        }
        // Mark domain as free.
        domain.second = {};
    }

    // Check any open queries.
    if (metricContext.getMetricsLibrary().getMetricQueryCount() == 0) {
        if (metricContext.getMetricsLibrary().getInitializationState() != ZE_RESULT_ERROR_UNINITIALIZED) {
            metricContext.getMetricsLibrary().release();
        }
    }

    return ZE_RESULT_SUCCESS;
}

bool MetricGroupDomains::isActivated(const zet_metric_group_handle_t hMetricGroup) {
    auto metricGroupProperties = MetricGroup::getProperties(hMetricGroup);

    // 1. Check whether domain is activated.
    const auto domain = domains.find(metricGroupProperties.domain);
    if (domain == domains.end()) {
        return false;
    }

    // 2. Check whether the specific MetricGroup is activated.
    return domain->second.first == hMetricGroup;
}

uint32_t MetricGroupDomains::getActivatedCount() {
    uint32_t count = 0;
    for (const auto &domain : domains) {
        count += domain.second.second ? 1 : 0;
    }
    return count;
}

ze_result_t metricGroupGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    auto device = Device::fromHandle(hDevice);
    return device->getMetricContext().getMetricEnumeration().metricGroupGet(*pCount,
                                                                            phMetricGroups);
}

ze_result_t metricStreamerOpen(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                               zet_metric_streamer_desc_t *pDesc, ze_event_handle_t hNotificationEvent,
                               zet_metric_streamer_handle_t *phMetricStreamer) {

    return MetricStreamer::open(hContext, hDevice, hMetricGroup, *pDesc, hNotificationEvent, phMetricStreamer);
}

} // namespace L0
