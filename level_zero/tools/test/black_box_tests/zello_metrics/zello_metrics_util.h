/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"

#pragma once

#define MAX_DEVICES_IN_MACHINE (64u)

namespace ZelloMetricsUtility {

// Simplistically assuming one level descendant configuration
struct TestMachineConfiguration {
    struct {
        uint32_t deviceIndex;
        uint32_t subDeviceCount;
    } devices[MAX_DEVICES_IN_MACHINE];
    uint32_t deviceCount = 0;
};

void createL0();
ze_driver_handle_t getDriver();
ze_context_handle_t createContext(ze_driver_handle_t &driverHandle);
ze_device_handle_t getDevice(ze_driver_handle_t &driverHandle, uint32_t deviceIndex);
ze_device_handle_t getSubDevice(ze_device_handle_t &deviceHandle, uint32_t subDeviceIndex);
ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &contextHandle,
                                             ze_device_handle_t &deviceHandle);
ze_command_list_handle_t createCommandList(ze_context_handle_t &contextHandle, ze_device_handle_t &deviceHandle);
void printMetricGroupProperties(const zet_metric_group_properties_t &properties);
void printMetricProperties(const zet_metric_properties_t &properties);
void sleep(uint32_t milliseconds);
bool getTestMachineConfiguration(TestMachineConfiguration &machineConfig);
zet_metric_group_handle_t findMetricGroup(const char *groupName,
                                          const zet_metric_group_sampling_type_flag_t samplingType,
                                          ze_device_handle_t deviceHandle);
ze_event_pool_handle_t createHostVisibleEventPool(ze_context_handle_t contextHandle, ze_device_handle_t deviceHandle);
ze_event_handle_t createHostVisibleEvent(ze_event_pool_handle_t hostVisibleEventPool);
void obtainCalculatedMetrics(zet_metric_group_handle_t metricGroup, uint8_t *rawData, uint32_t rawDataSize);
} // namespace ZelloMetricsUtility
