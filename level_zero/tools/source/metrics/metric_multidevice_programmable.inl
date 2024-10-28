/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_multidevice_programmable.h"

namespace L0 {

template <typename T>
ze_result_t MultiDeviceCreatedMetricGroupManager::createMultipleMetricGroupsFromMetrics(const MetricDeviceContext &metricDeviceContext,
                                                                                        MetricSource &metricSource,
                                                                                        std::vector<zet_metric_handle_t> &metricList,
                                                                                        const char metricGroupNamePrefix[ZET_INTEL_MAX_METRIC_GROUP_NAME_PREFIX_EXP],
                                                                                        const char description[ZET_MAX_METRIC_GROUP_DESCRIPTION],
                                                                                        uint32_t *maxMetricGroupCount,
                                                                                        std::vector<zet_metric_group_handle_t> &metricGroupList) {
    const auto isCountCalculationPath = *maxMetricGroupCount == 0;

    uint32_t subDeviceCount = static_cast<uint32_t>(static_cast<DeviceImp *>(&metricDeviceContext.getDevice())->subDevices.size());
    std::vector<std::vector<zet_metric_group_handle_t>> metricGroupsPerSubDevice(subDeviceCount);

    auto cleanupCreatedGroups = [](std::vector<zet_metric_group_handle_t> &createdMetricGroupList) {
        for (auto &metricGroup : createdMetricGroupList) {
            zetMetricGroupDestroyExp(metricGroup);
        }
        createdMetricGroupList.clear();
    };

    auto cleanupCreatedGroupsFromSubDevices = [&](uint32_t subDeviceLimit) {
        for (uint32_t index = 0; index < subDeviceLimit; index++) {
            cleanupCreatedGroups(metricGroupsPerSubDevice[index]);
        }
        metricGroupsPerSubDevice.clear();
    };

    uint32_t expectedMetricGroupCount = 0;
    auto isExpectedGroupCount = [&](const uint32_t actualGroupCount) {
        if (expectedMetricGroupCount != 0 && expectedMetricGroupCount != actualGroupCount) {
            METRICS_LOG_ERR("Unexpected Metric Group Count for subdevice expected:%d, actual:%d", expectedMetricGroupCount, actualGroupCount);
            return false;
        }
        expectedMetricGroupCount = actualGroupCount;
        return true;
    };

    // For each of the sub-devices, create Metric Groups
    for (uint32_t subDeviceIndex = 0; subDeviceIndex < subDeviceCount; subDeviceIndex++) {
        std::vector<zet_metric_handle_t> metricHandles(metricList.size());

        // Collect the metric handles at each sub-device index
        for (uint32_t inputMetricIndex = 0; inputMetricIndex < static_cast<uint32_t>(metricList.size()); inputMetricIndex++) {
            auto &multiDeviceMetric = metricList[inputMetricIndex];
            auto metricImp = static_cast<MetricImp *>(multiDeviceMetric);
            DEBUG_BREAK_IF(metricImp->isImmutable() == true);
            if (!metricImp->isRootDevice()) {
                METRICS_LOG_ERR("%s", "Metric from sub-device cannot be used for creating group at root-device");
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            auto homogenousMetric = static_cast<HomogeneousMultiDeviceMetricCreated *>(metricImp);
            metricHandles[inputMetricIndex] = homogenousMetric->getMetricAtSubDeviceIndex(subDeviceIndex)->toHandle();
        }

        // Get the source from one of the Metrics
        auto &subDeviceMetricSource = static_cast<MetricImp *>(metricHandles[0])->getMetricSource();

        uint32_t metricGroupCountPerSubDevice = *maxMetricGroupCount;
        auto status = subDeviceMetricSource.createMetricGroupsFromMetrics(metricHandles,
                                                                          metricGroupNamePrefix, description,
                                                                          &metricGroupCountPerSubDevice,
                                                                          metricGroupsPerSubDevice[subDeviceIndex]);
        if (status != ZE_RESULT_SUCCESS) {
            // cleanup till the current sub device Index
            cleanupCreatedGroupsFromSubDevices(subDeviceIndex);
            *maxMetricGroupCount = 0;
            METRICS_LOG_ERR("%s", "createMetricGroupsFromMetrics for subdevices returned error");
            return status;
        }

        // Count calculation path
        if (isCountCalculationPath) {
            *maxMetricGroupCount = metricGroupCountPerSubDevice;
            // We can return by looking at only one sub device
            return ZE_RESULT_SUCCESS;
        }

        if (!isExpectedGroupCount(metricGroupCountPerSubDevice)) {
            // +1 to cleanup current subdevice as well
            cleanupCreatedGroupsFromSubDevices(subDeviceIndex + 1);
            *maxMetricGroupCount = 0;
            METRICS_LOG_ERR("%s", "Different Metric group counts received from the sub-devices");
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }

    for (uint32_t groupIndex = 0; groupIndex < static_cast<uint32_t>(metricGroupsPerSubDevice[0].size()); groupIndex++) {
        std::vector<MetricGroupImp *> metricGroupsAcrossSubDevices(subDeviceCount);
        for (uint32_t subDeviceIndex = 0; subDeviceIndex < subDeviceCount; subDeviceIndex++) {
            metricGroupsAcrossSubDevices[subDeviceIndex] = static_cast<MetricGroupImp *>(metricGroupsPerSubDevice[subDeviceIndex][groupIndex]);
        }
        // All metric groups use the input metric list. A call to close() will finalize the list of metrics included in each group
        std::vector<MultiDeviceMetricImp *> multiDeviceMetricImp{};
        multiDeviceMetricImp.reserve(metricList.size());
        for (auto &hMetric : metricList) {
            multiDeviceMetricImp.push_back(static_cast<MultiDeviceMetricImp *>(Metric::fromHandle(hMetric)));
        }
        auto metricGroup = T::create(metricSource, metricGroupsAcrossSubDevices, multiDeviceMetricImp);
        if (metricGroup) {
            auto closeStatus = metricGroup->close();
            if (closeStatus != ZE_RESULT_SUCCESS) {
                // Cleanup and exit
                metricGroup->destroy();
                for (auto &metricGroupCreated : metricGroupList) {
                    MetricGroup::fromHandle(metricGroupCreated)->destroy();
                }
                metricGroupList.clear();

                // Delete the remaining subdevice groups
                for (uint32_t subDevGroupIndex = groupIndex + 1; subDevGroupIndex < static_cast<uint32_t>(metricGroupsPerSubDevice[0].size()); subDevGroupIndex++) {
                    for (uint32_t subDeviceIndex = 0; subDeviceIndex < subDeviceCount; subDeviceIndex++) {
                        [[maybe_unused]] auto status = zetMetricGroupDestroyExp(metricGroupsPerSubDevice[subDeviceIndex][subDevGroupIndex]);
                        DEBUG_BREAK_IF(status != ZE_RESULT_SUCCESS);
                    }
                }
                *maxMetricGroupCount = 0;
                return closeStatus;
            }
            metricGroupList.push_back(metricGroup);
        }
    }

    *maxMetricGroupCount = static_cast<uint32_t>(metricGroupList.size());

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
