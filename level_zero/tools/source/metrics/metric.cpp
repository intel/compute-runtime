/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/source/inc/ze_intel_gpu.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include <map>
#include <utility>

namespace L0 {

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
    metricSources[MetricSource::SourceType::IpSampling] = IpSamplingMetricSourceImp::create(*this);
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

        if (!metricSource->isAvailable()) {
            continue;
        }

        result = metricSource->metricGroupGet(&requestCount, phMetricGroups);
        if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
            result = ZE_RESULT_SUCCESS;
            continue;
        }

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
        zet_metric_group_properties_t properties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES};
        MetricGroup::fromHandle(hMetricGroup)->getProperties(&properties);
        auto domain = properties.domain;
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

    bool isSuccess = false;
    for (auto const &entry : metricSources) {
        auto const &metricSource = entry.second;
        if (!metricSource->isAvailable()) {
            continue;
        }

        ze_result_t result = metricSource->appendMetricMemoryBarrier(commandList);
        if (result == ZE_RESULT_SUCCESS) {
            isSuccess = true;
        } else if (result != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
            return result;
        }
    }
    return isSuccess == false ? ZE_RESULT_ERROR_UNSUPPORTED_FEATURE : ZE_RESULT_SUCCESS;
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
            break;
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
    return MetricGroup::fromHandle(hMetricGroup)->streamerOpen(hContext, hDevice, pDesc, hNotificationEvent, phMetricStreamer);
}

} // namespace L0
