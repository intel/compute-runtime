/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

#include "shared/source/os_interface/os_library.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/source/inc/ze_intel_gpu.h"
#include "level_zero/tools/source/metrics/metric_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_query_imp.h"
#include "level_zero/tools/source/metrics/metric_source_oa.h"

#include <map>
#include <utility>

namespace L0 {

OaMetricSourceImp::OsLibraryLoadPtr OaMetricSourceImp::osLibraryLoadFunction(NEO::OsLibrary::load);

std::unique_ptr<OaMetricSourceImp> OaMetricSourceImp::create(const MetricDeviceContext &metricDeviceContext) {
    return std::unique_ptr<OaMetricSourceImp>(new (std::nothrow) OaMetricSourceImp(metricDeviceContext));
}

OaMetricSourceImp::OaMetricSourceImp(const MetricDeviceContext &metricDeviceContext) : metricDeviceContext(metricDeviceContext),
                                                                                       metricEnumeration(std::unique_ptr<MetricEnumeration>(new (std::nothrow) MetricEnumeration(*this))),
                                                                                       metricsLibrary(std::unique_ptr<MetricsLibrary>(new (std::nothrow) MetricsLibrary(*this))) {
}

OaMetricSourceImp::~OaMetricSourceImp() = default;

bool OaMetricSourceImp::checkDependencies() {

    std::unique_ptr<NEO::OsLibrary> library = nullptr;

    // Check Metrics Discovery availability.
    library.reset(osLibraryLoadFunction(MetricEnumeration::getMetricsDiscoveryFilename()));
    if (library == nullptr) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find metrics discovery %s\n", MetricEnumeration::getMetricsDiscoveryFilename());
        return false;
    }

    // Check Metrics Library availability.
    library.reset(osLibraryLoadFunction(MetricsLibrary::getFilename()));
    if (library == nullptr) {
        PRINT_DEBUG_STRING(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr, "Unable to find metrics library %s\n", MetricsLibrary::getFilename());
        return false;
    }

    return true;
}

void OaMetricSourceImp::enable() {
    available = false;
    if (loadDependencies()) {
        available = true;
    }
}

bool OaMetricSourceImp::isAvailable() {
    return available;
}

ze_result_t OaMetricSourceImp::appendMetricMemoryBarrier(CommandList &commandList) {
    DeviceImp *pDeviceImp = static_cast<DeviceImp *>(commandList.device);

    if (pDeviceImp->metricContext->isImplicitScalingCapable()) {
        // Use one of the sub-device contexts to append to command list.
        pDeviceImp = static_cast<DeviceImp *>(pDeviceImp->subDevices[0]);
    }

    auto &metricContext = pDeviceImp->getMetricDeviceContext();
    auto &metricsLibrary = metricContext.getMetricSource<OaMetricSourceImp>().getMetricsLibrary();

    // Obtain gpu commands.
    CommandBufferData_1_0 commandBuffer = {};
    commandBuffer.CommandsType = MetricsLibraryApi::ObjectType::OverrideFlushCaches;
    commandBuffer.Override.Enable = true;
    commandBuffer.Type = metricContext.getMetricSource<OaMetricSourceImp>().isComputeUsed()
                             ? MetricsLibraryApi::GpuCommandBufferType::Compute
                             : MetricsLibraryApi::GpuCommandBufferType::Render;

    return metricsLibrary.getGpuCommands(commandList, commandBuffer) ? ZE_RESULT_SUCCESS
                                                                     : ZE_RESULT_ERROR_UNKNOWN;
}

bool OaMetricSourceImp::loadDependencies() {
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
bool OaMetricSourceImp::isInitialized() {
    return initializationState == ZE_RESULT_SUCCESS;
}

void OaMetricSourceImp::setInitializationState(const ze_result_t state) {
    initializationState = state;
}

Device &OaMetricSourceImp::getDevice() {
    return metricDeviceContext.getDevice();
}

MetricsLibrary &OaMetricSourceImp::getMetricsLibrary() {
    return *metricsLibrary;
}
MetricEnumeration &OaMetricSourceImp::getMetricEnumeration() {
    return *metricEnumeration;
}
MetricStreamer *OaMetricSourceImp::getMetricStreamer() {
    return pMetricStreamer;
}

void OaMetricSourceImp::setMetricStreamer(MetricStreamer *pMetricStreamer) {
    this->pMetricStreamer = pMetricStreamer;
}

void OaMetricSourceImp::setMetricsLibrary(MetricsLibrary &metricsLibrary) {
    this->metricsLibrary.release();
    this->metricsLibrary.reset(&metricsLibrary);
}

void OaMetricSourceImp::setMetricEnumeration(MetricEnumeration &metricEnumeration) {
    this->metricEnumeration.release();
    this->metricEnumeration.reset(&metricEnumeration);
}

void OaMetricSourceImp::setUseCompute(const bool useCompute) {
    this->useCompute = useCompute;
}

bool OaMetricSourceImp::isComputeUsed() const {
    return useCompute;
}

ze_result_t OaMetricSourceImp::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    return getMetricEnumeration().metricGroupGet(*pCount, phMetricGroups);
}

uint32_t OaMetricSourceImp::getSubDeviceIndex() {
    return metricDeviceContext.getSubDeviceIndex();
}

bool OaMetricSourceImp::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
    return metricDeviceContext.isMetricGroupActivated(hMetricGroup);
}

bool OaMetricSourceImp::isMetricGroupActivated() const {
    return metricDeviceContext.isMetricGroupActivated();
}

bool OaMetricSourceImp::isImplicitScalingCapable() const {
    return metricDeviceContext.isImplicitScalingCapable();
}

std::unique_ptr<MetricDeviceContext> MetricDeviceContext::create(Device &device) {
    return std::make_unique<MetricDeviceContext>(device);
}

MetricDeviceContext::MetricDeviceContext(Device &inputDevice) : device(inputDevice) {
    auto deviceNeo = device.getNEODevice();
    bool isSubDevice = deviceNeo->isSubDevice();
    subDeviceIndex = isSubDevice
                         ? static_cast<NEO::SubDevice *>(deviceNeo)->getSubDeviceIndex()
                         : 0;

    multiDeviceCapable = !isSubDevice && device.isImplicitScalingCapable();
    metricSources[MetricSource::SourceType::Oa] = OaMetricSourceImp::create(*this);
}

bool MetricDeviceContext::enable() {
    bool status = false;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;
        metricSource->enable();
        status |= metricSource->isAvailable();
    }
    return status;
}

ze_result_t MetricDeviceContext::metricGroupGet(uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    uint32_t availableCount = 0;
    uint32_t requestCount = *pCount;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;
        result = metricSource->metricGroupGet(&requestCount, phMetricGroups);
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

ze_result_t MetricDeviceContext::activateMetricGroupsDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) {

    // Activation: postpone until zetMetricStreamerOpen or zeCommandQueueExecuteCommandLists
    // Deactivation: execute immediately.
    if (phMetricGroups == nullptr) {
        return deActivateAllDomains();
    }

    for (auto index = 0u; index < count; index++) {

        zet_metric_group_handle_t hMetricGroup = MetricGroup::fromHandle(phMetricGroups[index])->getMetricGroupForSubDevice(subDeviceIndex);
        auto domain = MetricGroup::getProperties(hMetricGroup).domain;
        // Domain already associated with the same handle.
        if (domains[domain].first == hMetricGroup) {
            continue;
        }

        // Domain empty; So create new deactiavted association.
        if (domains[domain].first == nullptr) {
            domains[domain].first = hMetricGroup;
            domains[domain].second = false;
            continue;
        }

        // Attempt to overwrite a previous association is an error.
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricDeviceContext::activateAllDomains() {
    for (auto &entry : domains) {
        auto &metricGroup = entry.second;
        MetricGroup::fromHandle(metricGroup.first)->activate();
        metricGroup.second = true;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricDeviceContext::deActivateAllDomains() {
    for (auto &entry : domains) {
        auto &metricGroup = entry.second;
        if (metricGroup.second == true) {
            MetricGroup::fromHandle(metricGroup.first)->deactivate();
        }
        metricGroup = {};
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricDeviceContext::appendMetricMemoryBarrier(CommandList &commandList) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;

        result = metricSource->appendMetricMemoryBarrier(commandList);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }
    return result;
}

bool MetricDeviceContext::isMetricGroupActivated(const zet_metric_group_handle_t hMetricGroup) const {
    for (auto const &entry : domains) {
        auto const &metricGroup = entry.second;
        if (metricGroup.first == hMetricGroup) {
            return true;
        }
    }
    return false;
}

bool MetricDeviceContext::isMetricGroupActivated() const {
    for (auto const &entry : domains) {
        auto const &metricGroup = entry.second;
        if (metricGroup.second == true) {
            return true;
        }
    }
    return false;
}

bool MetricDeviceContext::isImplicitScalingCapable() const {
    return multiDeviceCapable;
}

ze_result_t MetricDeviceContext::activateMetricGroups() {
    return activateAllDomains();
}

uint32_t MetricDeviceContext::getSubDeviceIndex() const {
    return subDeviceIndex;
}

Device &MetricDeviceContext::getDevice() const {
    return device;
}

ze_result_t MetricDeviceContext::enableMetricApi() {

    if (!OaMetricSourceImp::checkDependencies()) {
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
        auto rootDevice = static_cast<DeviceImp *>(L0::Device::fromHandle(rootDeviceHandle));
        // Initialize root device.
        failed |= !rootDevice->metricContext->enable();

        if (failed) {
            continue;
        }

        // Initialize sub devices.
        for (uint32_t i = 0; i < rootDevice->numSubDevices; ++i) {
            failed |= !rootDevice->subDevices[i]->getMetricDeviceContext().enable();
        }
    }

    return failed
               ? ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE
               : ZE_RESULT_SUCCESS;
}

ze_result_t metricGroupGet(zet_device_handle_t hDevice, uint32_t *pCount, zet_metric_group_handle_t *phMetricGroups) {
    auto device = Device::fromHandle(hDevice);
    return device->getMetricDeviceContext().metricGroupGet(pCount, phMetricGroups);
}

ze_result_t metricStreamerOpen(zet_context_handle_t hContext, zet_device_handle_t hDevice, zet_metric_group_handle_t hMetricGroup,
                               zet_metric_streamer_desc_t *pDesc, ze_event_handle_t hNotificationEvent,
                               zet_metric_streamer_handle_t *phMetricStreamer) {

    return MetricStreamer::open(hContext, hDevice, hMetricGroup, *pDesc, hNotificationEvent, phMetricStreamer);
}

template <>
OaMetricSourceImp &MetricDeviceContext::getMetricSource<OaMetricSourceImp>() const {
    return static_cast<OaMetricSourceImp &>(*metricSources.at(MetricSource::SourceType::Oa));
}

} // namespace L0
