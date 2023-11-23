/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric.h"

#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/metrics/metric_ip_sampling_source.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include <map>
#include <utility>

namespace L0 {

MetricDeviceContext::MetricDeviceContext(Device &inputDevice) : device(inputDevice) {
    auto deviceNeo = device.getNEODevice();
    std::tuple<uint32_t, uint32_t, uint32_t> subDeviceMap;
    uint32_t hwSubDeviceIndex = 0u;
    bool requiresSubDeviceHierarchy = false;
    if (deviceNeo->getExecutionEnvironment()->getSubDeviceHierarchy(deviceNeo->getRootDeviceIndex(), &subDeviceMap)) {
        hwSubDeviceIndex = std::get<1>(subDeviceMap);
        requiresSubDeviceHierarchy = true;
    }
    if (requiresSubDeviceHierarchy) {
        subDeviceIndex = hwSubDeviceIndex;
        multiDeviceCapable = false;
    } else {
        bool isSubDevice = deviceNeo->isSubDevice();
        subDeviceIndex = isSubDevice
                             ? static_cast<NEO::SubDevice *>(deviceNeo)->getSubDeviceIndex()
                             : 0;

        multiDeviceCapable = !isSubDevice && device.isImplicitScalingCapable();
    }
    metricSources[MetricSource::metricSourceTypeOa] = OaMetricSourceImp::create(*this);
    metricSources[MetricSource::metricSourceTypeIpSampling] = IpSamplingMetricSourceImp::create(*this);
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

void MetricDeviceContext::activateMetricGroupsDeferred(uint32_t count, zet_metric_group_handle_t *phMetricGroups) {

    // Activation: postpone until zetMetricStreamerOpen or zeCommandQueueExecuteCommandLists
    // Deactivation: execute immediately.
    if (phMetricGroups == nullptr) {
        deActivateAllDomains();
        return;
    }

    auto isMetricGroupProvided = [phMetricGroups, count](const zet_metric_group_handle_t hMetricGroup) {
        for (auto index = 0u; index < count; index++) {
            if (hMetricGroup == phMetricGroups[index]) {
                return true;
            }
        }
        return false;
    };

    // Deactive existing metric groups which are not provided in phMetricGroups
    std::vector<uint32_t> deactivateList = {};
    for (const auto &[domainId, metricGroupPair] : domains) {
        const auto &hMetricGroup = metricGroupPair.first;
        if (isMetricGroupProvided(hMetricGroup) == false) {
            deActivateDomain(domainId);
            deactivateList.push_back(domainId);
        }
    }

    // Remove deactivated ones from the map
    for (const auto &domainId : deactivateList) {
        domains.erase(domainId);
    }

    // Activate-deferred new metric groups if any
    for (auto index = 0u; index < count; index++) {

        zet_metric_group_handle_t hMetricGroup = MetricGroup::fromHandle(phMetricGroups[index])->getMetricGroupForSubDevice(subDeviceIndex);
        zet_metric_group_properties_t properties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
        MetricGroup::fromHandle(hMetricGroup)->getProperties(&properties);
        auto domain = properties.domain;
        // Domain already associated with the same handle.
        if (domains[domain].first == hMetricGroup) {
            continue;
        }

        domains[domain].first = hMetricGroup;
        domains[domain].second = false;
    }
}

ze_result_t MetricDeviceContext::activateAllDomains() {
    for (auto &entry : domains) {
        auto &metricGroup = entry.second;
        DEBUG_BREAK_IF(metricGroup.first == nullptr);
        MetricGroup::fromHandle(metricGroup.first)->activate();
        metricGroup.second = true;
    }
    return ZE_RESULT_SUCCESS;
}

void MetricDeviceContext::deActivateDomain(uint32_t domain) {
    auto &metricGroupPair = domains[domain];
    if (metricGroupPair.second == true) {
        MetricGroup::fromHandle(metricGroupPair.first)->deactivate();
    }
}

void MetricDeviceContext::deActivateAllDomains() {
    for (auto &entry : domains) {
        deActivateDomain(entry.first);
    }
    domains.clear();
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

    auto driverHandle = L0::DriverHandle::fromHandle(globalDriverHandle);
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

ze_result_t MetricGroup::getMetricGroupExtendedProperties(MetricSource &metricSource, void *pNext) {
    ze_result_t retVal = ZE_RESULT_ERROR_INVALID_ARGUMENT;

    while (pNext) {
        zet_base_desc_t *extendedProperties = reinterpret_cast<zet_base_desc_t *>(pNext);

        if (extendedProperties->stype == ZET_STRUCTURE_TYPE_METRIC_GLOBAL_TIMESTAMPS_RESOLUTION_EXP) {

            zet_metric_global_timestamps_resolution_exp_t *metricsTimestampProperties =
                reinterpret_cast<zet_metric_global_timestamps_resolution_exp_t *>(extendedProperties);

            retVal = metricSource.getTimerResolution(metricsTimestampProperties->timerResolution);
            if (retVal != ZE_RESULT_SUCCESS) {
                metricsTimestampProperties->timerResolution = 0;
                metricsTimestampProperties->timestampValidBits = 0;
                return retVal;
            }

            retVal = metricSource.getTimestampValidBits(metricsTimestampProperties->timestampValidBits);
            if (retVal != ZE_RESULT_SUCCESS) {
                metricsTimestampProperties->timerResolution = 0;
                metricsTimestampProperties->timestampValidBits = 0;
                return retVal;
            }
        }

        pNext = const_cast<void *>(extendedProperties->pNext);
    }

    return retVal;
}

} // namespace L0
