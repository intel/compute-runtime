/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_multidevice_programmable.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/metric_multidevice_programmable.inl"

namespace L0 {

MultiDeviceCreatedMetricGroupManager::MultiDeviceCreatedMetricGroupManager(MetricSource &metricSource,
                                                                           std::vector<MetricGroupImp *> &subDeviceMetricGroupsCreated,
                                                                           std::vector<MultiDeviceMetricImp *> &inMultiDeviceMetrics) : metricSource(metricSource),
                                                                                                                                        subDeviceMetricGroupsCreated(subDeviceMetricGroupsCreated),
                                                                                                                                        multiDeviceMetrics(inMultiDeviceMetrics) {}

ze_result_t MultiDeviceCreatedMetricGroupManager::destroy() {

    auto status = ZE_RESULT_SUCCESS;

    for (auto &metric : multiDeviceMetrics) {
        deleteMetricAddedDuringClose(metric);
    }
    multiDeviceMetrics.clear();

    for (auto &subDeviceMetricGroup : subDeviceMetricGroupsCreated) {
        [[maybe_unused]] auto destroyStatus = MetricGroup::fromHandle(subDeviceMetricGroup)->destroy();
        DEBUG_BREAK_IF(destroyStatus != ZE_RESULT_SUCCESS);
    }
    return status;
}

ze_result_t MultiDeviceCreatedMetricGroupManager::metricGet(uint32_t *pCount, zet_metric_handle_t *phMetrics) {

    if (*pCount == 0) {
        *pCount = static_cast<uint32_t>(multiDeviceMetrics.size());
    } else {
        *pCount = std::min(*pCount, static_cast<uint32_t>(multiDeviceMetrics.size()));
        for (uint32_t index = 0; index < *pCount; index++) {
            phMetrics[index] = multiDeviceMetrics[index];
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MultiDeviceCreatedMetricGroupManager::addMetric(zet_metric_handle_t hMetric, size_t *errorStringSize, char *pErrorString) {

    MetricImp *metricImp = static_cast<MetricImp *>(Metric::fromHandle(hMetric));
    if (metricImp->isImmutable() || !metricImp->isRootDevice()) {
        METRICS_LOG_ERR("%s", "Cannot add metric which was not created from a programmable");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    HomogeneousMultiDeviceMetricCreated *multiDeviceMetric = static_cast<HomogeneousMultiDeviceMetricCreated *>(metricImp);

    auto cleanupApi = [&](uint32_t subDeviceLimit) {
        for (uint32_t subDeviceIndex = 0; subDeviceIndex < subDeviceLimit; subDeviceIndex++) {
            auto metricGroup = MetricGroup::fromHandle(subDeviceMetricGroupsCreated[subDeviceIndex]);
            [[maybe_unused]] auto result = metricGroup->removeMetric(multiDeviceMetric->getMetricAtSubDeviceIndex(subDeviceIndex));
            DEBUG_BREAK_IF(result != ZE_RESULT_SUCCESS);
        }
    };

    for (uint32_t subDeviceIndex = 0; subDeviceIndex < static_cast<uint32_t>(subDeviceMetricGroupsCreated.size()); subDeviceIndex++) {
        auto metricGroup = MetricGroup::fromHandle(subDeviceMetricGroupsCreated[subDeviceIndex]);
        auto result = metricGroup->addMetric(multiDeviceMetric->getMetricAtSubDeviceIndex(subDeviceIndex),
                                             errorStringSize, pErrorString);
        if (result != ZE_RESULT_SUCCESS) {
            cleanupApi(subDeviceIndex);
            return result;
        }
    }

    multiDeviceMetrics.push_back(multiDeviceMetric);

    return ZE_RESULT_SUCCESS;
}

ze_result_t MultiDeviceCreatedMetricGroupManager::removeMetric(zet_metric_handle_t hMetric) {

    MetricImp *metricImp = static_cast<MetricImp *>(Metric::fromHandle(hMetric));
    if (metricImp->isImmutable() || !metricImp->isRootDevice()) {
        METRICS_LOG_ERR("%s", "Cannot remove metric which was not created from a programmable");
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    HomogeneousMultiDeviceMetricCreated *multiDeviceMetric = static_cast<HomogeneousMultiDeviceMetricCreated *>(metricImp);
    auto cleanupApi = [&](uint32_t subDeviceLimit) {
        for (uint32_t subDeviceIndex = 0; subDeviceIndex < subDeviceLimit; subDeviceIndex++) {
            size_t errorStringSize = 0;
            auto metricGroup = MetricGroup::fromHandle(subDeviceMetricGroupsCreated[subDeviceIndex]);
            [[maybe_unused]] auto result = metricGroup->addMetric(multiDeviceMetric->getMetricAtSubDeviceIndex(subDeviceIndex),
                                                                  &errorStringSize, nullptr);
            DEBUG_BREAK_IF(result != ZE_RESULT_SUCCESS);
        }
    };

    for (uint32_t subDeviceIndex = 0; subDeviceIndex < static_cast<uint32_t>(subDeviceMetricGroupsCreated.size()); subDeviceIndex++) {
        auto metricGroup = MetricGroup::fromHandle(subDeviceMetricGroupsCreated[subDeviceIndex]);
        auto result = metricGroup->removeMetric(multiDeviceMetric->getMetricAtSubDeviceIndex(subDeviceIndex));
        if (result != ZE_RESULT_SUCCESS) {
            cleanupApi(subDeviceIndex);
            return result;
        }
    }

    auto iterator = std::find(multiDeviceMetrics.begin(), multiDeviceMetrics.end(), hMetric);
    if (iterator != multiDeviceMetrics.end()) {
        multiDeviceMetrics.erase(iterator);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MultiDeviceCreatedMetricGroupManager::close() {

    auto closeResult = ZE_RESULT_SUCCESS;
    // Close the subdevice groups to get the updated metric count
    for (auto &subDevMetricGroup : subDeviceMetricGroupsCreated) {
        auto metricGroup = MetricGroup::fromHandle(subDevMetricGroup);
        auto result = metricGroup->close();
        if (closeResult == ZE_RESULT_SUCCESS) {
            closeResult = result;
        }
    }

    if (closeResult != ZE_RESULT_SUCCESS) {
        return closeResult;
    }

    uint32_t expectedMetricHandleCount = 0;
    auto isExpectedHandleCount = [&](const uint32_t actualHandleCount) {
        if (expectedMetricHandleCount != 0 && expectedMetricHandleCount != actualHandleCount) {
            METRICS_LOG_ERR("Unexpected Metric Handle Count for subdevice expected:%d, actual:%d", expectedMetricHandleCount, actualHandleCount);
            return false;
        }
        expectedMetricHandleCount = actualHandleCount;
        return true;
    };

    uint32_t subDeviceCount = static_cast<uint32_t>(subDeviceMetricGroupsCreated.size());
    std::vector<std::vector<zet_metric_handle_t>> subDeviceMetricHandles(subDeviceCount);
    // Get all metric handles from all sub-devices
    for (uint32_t index = 0; index < subDeviceCount; index++) {
        uint32_t count = 0;
        auto metricGroup = MetricGroup::fromHandle(subDeviceMetricGroupsCreated[index]);
        [[maybe_unused]] auto status = metricGroup->metricGet(&count, nullptr);
        DEBUG_BREAK_IF(status != ZE_RESULT_SUCCESS || count == 0);
        if (isExpectedHandleCount(count)) {
            subDeviceMetricHandles[index].resize(count);
            auto metricGroup = MetricGroup::fromHandle(subDeviceMetricGroupsCreated[index]);
            metricGroup->metricGet(&count, subDeviceMetricHandles[index].data());
        } else {
            METRICS_LOG_ERR("%s", "Different Metric counts received from the sub-devices");
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    auto getRelatedHomogenousMultiDeviceHandle = [&](zet_metric_handle_t subDeviceMetricHandle) {
        for (auto &multiDeviceMetric : multiDeviceMetrics) {
            auto mutableMetric = static_cast<HomogeneousMultiDeviceMetricCreated *>(multiDeviceMetric);
            auto matchingMetric = mutableMetric->getMetricAtSubDeviceIndex(0)->toHandle();
            if (matchingMetric == subDeviceMetricHandle) {
                return mutableMetric;
            }
        }
        return static_cast<HomogeneousMultiDeviceMetricCreated *>(nullptr);
    };

    // Arrange metric handles based on sub-device handles
    const uint32_t metricCountPerSubdevice = static_cast<uint32_t>(subDeviceMetricHandles[0].size());
    std::vector<MultiDeviceMetricImp *> arrangedMetricHandles(metricCountPerSubdevice);
    for (uint32_t index = 0; index < metricCountPerSubdevice; index++) {
        auto multiDeviceMetric = getRelatedHomogenousMultiDeviceHandle(subDeviceMetricHandles[0][index]);

        if (multiDeviceMetric != nullptr) {
            arrangedMetricHandles[index] = multiDeviceMetric;
        } else {
            // Create a new multidevice immutable metric for new metrics added during close
            std::vector<MetricImp *> subDeviceMetrics(subDeviceCount);
            for (uint32_t subDeviceIndex = 0; subDeviceIndex < subDeviceCount; subDeviceIndex++) {
                subDeviceMetrics[subDeviceIndex] = static_cast<MetricImp *>(
                    Metric::fromHandle(subDeviceMetricHandles[subDeviceIndex][index]));
            }
            arrangedMetricHandles[index] = MultiDeviceMetricImp::create(metricSource, subDeviceMetrics);
        }
    }

    // Clean up and use the new list
    if (arrangedMetricHandles != multiDeviceMetrics) {
        for (auto &metric : multiDeviceMetrics) {
            if (std::find(arrangedMetricHandles.begin(), arrangedMetricHandles.end(), metric) == arrangedMetricHandles.end()) {
                deleteMetricAddedDuringClose(metric);
            }
        }
    }

    multiDeviceMetrics = std::move(arrangedMetricHandles);

    return ZE_RESULT_SUCCESS;
}

void MultiDeviceCreatedMetricGroupManager::deleteMetricAddedDuringClose(Metric *metric) {
    MetricImp *metricImp = static_cast<MetricImp *>(metric);
    // Only destroy metrics added during Close. Other metrics are managed by application
    if (metricImp->isImmutable()) {
        delete metricImp;
    }
}

} // namespace L0
