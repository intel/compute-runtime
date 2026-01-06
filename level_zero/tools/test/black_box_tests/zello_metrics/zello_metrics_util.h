/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"

#include <iostream>

#define MAX_DEVICES_IN_MACHINE (64u)
#define LOG(level) (((level) <= (ZelloMetricsUtility::TestSettings::get())->verboseLevel.get()) \
                        ? (std::cout)                                                           \
                        : (ZelloMetricsUtility::emptyCout))

namespace ZelloMetricsUtility {

// Simplistically assuming one level descendant configuration
struct TestMachineConfiguration {
    struct {
        uint32_t deviceIndex;
        uint32_t subDeviceCount;
    } devices[MAX_DEVICES_IN_MACHINE];
    uint32_t deviceCount = 0;
    uint32_t deviceId;
    std::vector<ze_driver_extension_properties_t> driverExtensionsProperties{};
};

template <class T>
class TestParameter {
  public:
    TestParameter(T param) : param(param) {}
    bool isSet = false;
    T &get() { return param; }
    void set(T value) {
        param = value;
        isSet = true;
    }

  private:
    T param;
};

class TestSettings {
  public:
    static TestSettings *get();
    void parseArguments(int argc, char *argv[]);

    std::string testName = {};
    TestParameter<int32_t> deviceId{-1};
    TestParameter<int32_t> subDeviceId{-1};
    TestParameter<int32_t> verboseLevel{0};
    TestParameter<std::string> metricGroupName{"TestOa"};
    std::vector<TestParameter<std::string>> metricNames{};
    TestParameter<uint32_t> eventNReportCount{1};
    TestParameter<bool> showSystemInfo{false};
    void readMetricNames(char *optArg);

  private:
    TestSettings() = default;
    ~TestSettings() = default;
    static TestSettings *settings;
};

struct LogLevel {
    enum {
        ERROR = -2,
        WARNING = -1,
        INFO = 0,
        DEBUG = 1
    };
};
extern std::ostream emptyCout;

void createL0();
ze_driver_handle_t getDriver();
ze_context_handle_t createContext(ze_driver_handle_t &driverHandle);
bool isDeviceAvailable(uint32_t deviceIndex, int32_t subDeviceIndex);
ze_device_handle_t getDevice(ze_driver_handle_t &driverHandle, uint32_t deviceIndex);
ze_device_handle_t getSubDevice(ze_device_handle_t &deviceHandle, uint32_t subDeviceIndex);
ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &contextHandle,
                                             ze_device_handle_t &deviceHandle);
ze_command_list_handle_t createCommandList(ze_context_handle_t &contextHandle, ze_device_handle_t &deviceHandle);
ze_command_list_handle_t createImmediateCommandList(ze_context_handle_t &contextHandle, ze_device_handle_t &deviceHandle);
void printMetricGroupProperties(const zet_metric_group_properties_t &properties);
void printMetricGroupAndMetricProperties(zet_metric_group_handle_t metricGroup);
void printMetricProperties(const zet_metric_properties_t &properties);
void sleep(uint32_t milliseconds);
bool getTestMachineConfiguration(TestMachineConfiguration &machineConfig);
zet_metric_group_handle_t findMetricGroup(const char *groupName,
                                          const zet_metric_group_sampling_type_flags_t samplingType,
                                          ze_device_handle_t deviceHandle);
ze_event_pool_handle_t createHostVisibleEventPool(ze_context_handle_t contextHandle, ze_device_handle_t deviceHandle);
ze_event_handle_t createHostVisibleEvent(ze_event_pool_handle_t hostVisibleEventPool);
void obtainCalculatedMetrics(zet_metric_group_handle_t metricGroup, uint8_t *rawData, uint32_t rawDataSize);
void showMetricsExportData(uint8_t *pExportData, size_t exportDataSize);

// os specific methods
bool osStreamMpCollectionWorkloadDifferentProcess();
bool osStreamMpCollectionWorkloadSameProcess();
int32_t osRunAllTests(int32_t runStatus);
bool isEnvVariableSet(const char *name);

} // namespace ZelloMetricsUtility
