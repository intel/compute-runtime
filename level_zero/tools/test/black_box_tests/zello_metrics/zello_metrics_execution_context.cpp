/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

namespace zmu = ZelloMetricsUtility;

///////////////////////////
/////ExecutionContext
///////////////////////////
void ExecutionContext::addActiveMetricGroup(zet_metric_group_handle_t &metricGroup) {

    // Add only if not available already
    if (std::find(activeMetricGroups.begin(), activeMetricGroups.end(), metricGroup) == activeMetricGroups.end()) {
        activeMetricGroups.push_back(metricGroup);
    }
}

void ExecutionContext::removeActiveMetricGroup(zet_metric_group_handle_t metricGroup) {

    if (metricGroup == nullptr) {
        deactivateMetricGroups();
        activeMetricGroups.clear();
        return;
    }

    // Remove only if available already
    auto pos = std::find(activeMetricGroups.begin(), activeMetricGroups.end(), metricGroup);
    if (pos != activeMetricGroups.end()) {
        activeMetricGroups.erase(pos);
    }
}

bool ExecutionContext::activateMetricGroups() {
    if (activeMetricGroups.size()) {
        VALIDATECALL(zetContextActivateMetricGroups(getContextHandle(0), getDeviceHandle(0),
                                                    static_cast<uint32_t>(activeMetricGroups.size()), activeMetricGroups.data()));
    }
    return true;
}

bool ExecutionContext::deactivateMetricGroups() {
    VALIDATECALL(zetContextActivateMetricGroups(getContextHandle(0), getDeviceHandle(0), 0, nullptr));
    return true;
}

//////////////////////////////////////////
/////SingleDeviceSingleQueueExecutionCtxt
//////////////////////////////////////////
bool SingleDeviceSingleQueueExecutionCtxt::initialize(uint32_t deviceIndex, int32_t subDeviceIndex) {

    zmu::createL0();
    driverHandle = zmu::getDriver();
    EXPECT(driverHandle != nullptr);
    contextHandle = zmu::createContext(driverHandle);
    EXPECT(contextHandle != nullptr);

    deviceHandle = zmu::getDevice(driverHandle, deviceIndex);
    EXPECT(deviceHandle != nullptr);
    if (subDeviceIndex != -1) {
        deviceHandle = zmu::getSubDevice(deviceHandle, subDeviceIndex);
        EXPECT(deviceHandle != nullptr);
    }
    commandQueue = zmu::createCommandQueue(contextHandle, deviceHandle);
    EXPECT(commandQueue != nullptr);
    commandList = zmu::createCommandList(contextHandle, deviceHandle);
    EXPECT(commandList != nullptr);
    return true;
}

bool SingleDeviceSingleQueueExecutionCtxt::finalize() {

    VALIDATECALL(zeCommandListDestroy(commandList));
    VALIDATECALL(zeCommandQueueDestroy(commandQueue));
    VALIDATECALL(zeContextDestroy(contextHandle));

    deviceHandle = nullptr;
    driverHandle = nullptr;
    return true;
}

bool SingleDeviceSingleQueueExecutionCtxt::run() {

    // Close command list.
    VALIDATECALL(zeCommandListClose(commandList));

    auto startTime = std::chrono::steady_clock::now();
    uint32_t diff = 0;
    uint32_t runCount = 0;

    Power capturePower(getDeviceHandle(0));
    Frequency captureFrequency(getDeviceHandle(0));
    if (systemParameterList.size() > 0) {
        for (const auto &systemParameter : systemParameterList) {
            systemParameter->capture("initial");
        }
    }

    do {
        // Execute workload.
        VALIDATECALL(zeCommandQueueExecuteCommandLists(commandQueue, 1, &commandList, nullptr));

        // If using async command queue, explicit sync must be used for correctness.
        VALIDATECALL(zeCommandQueueSynchronize(commandQueue, std::numeric_limits<uint32_t>::max()));
        auto currentTime = std::chrono::steady_clock::now();
        diff = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count());

        runCount++;

        if (systemParameterList.size() > 0) {
            for (const auto &systemParameter : systemParameterList) {
                systemParameter->capture(std::to_string(runCount));
            }
        }
    } while (diff < executionTimeInMilliSeconds);

    LOG(zmu::LogLevel::DEBUG) << "CommandList Run count : " << runCount << "\n";

    return true;
}

Power::Power(ze_device_handle_t device) : SystemParameter(device) {
    std::vector<zes_pwr_handle_t> handles = {};
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumPowerDomains(device, &count, nullptr));
    handles.resize(count);
    VALIDATECALL(zesDeviceEnumPowerDomains(device, &count, handles.data()));
    handle = handles[0];
}

void Power::capture(std::string s) {
    zes_power_energy_counter_t energyCounter;
    VALIDATECALL(zesPowerGetEnergyCounter(handle, &energyCounter));
    energyCounters.push_back(std::make_pair(energyCounter, s));
}

void Power::showAll(double minDiff) {
    double prevTimestamp = 0.0;
    double prevEnergy = 0.0;
    double prevPower = std::numeric_limits<double>::max();
    bool useInitialValue = true;

    for (const auto &[energyCounter, s] : energyCounters) {

        if (prevTimestamp == 0) {
            prevTimestamp = energyCounter.timestamp;
            prevEnergy = energyCounter.energy;
            continue;
        }
        double powerInWatts = (energyCounter.energy - prevEnergy) / (energyCounter.timestamp - prevTimestamp);
        // Show only power differences greater than a specific value
        auto absDiff = static_cast<uint64_t>(prevPower - powerInWatts);
        if ((absDiff > minDiff) || (useInitialValue)) {
            LOG(zmu::LogLevel::DEBUG) << s << " : " << powerInWatts << " Watts \n";
            prevPower = powerInWatts;
            useInitialValue = false;
        }
    }

    double min = 0.0, max = 0.0;
    getMinMax(min, max);
    LOG(zmu::LogLevel::INFO) << "MinimumPower: " << min << "W | MaximumPower: " << max << "W \n";
}

void Power::getMinMax(double &min, double &max) {

    double prevTimestamp = 0.0;
    double prevEnergy = 0.0;
    std::pair<double, double> minMaxPower(std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::min());

    for (const auto &energyCounterInfo : energyCounters) {

        const auto &energyCounter = energyCounterInfo.first;

        if (prevTimestamp == 0) {
            prevTimestamp = energyCounter.timestamp;
            prevEnergy = energyCounter.energy;
            continue;
        }
        double powerInWatts = (energyCounter.energy - prevEnergy) / (energyCounter.timestamp - prevTimestamp);
        minMaxPower.first = std::min(powerInWatts, minMaxPower.first);
        minMaxPower.second = std::max(powerInWatts, minMaxPower.second);
    }

    min = minMaxPower.first;
    max = minMaxPower.second;
}

Frequency::Frequency(ze_device_handle_t device) : SystemParameter(device) {
    std::vector<zes_freq_handle_t> handles = {};
    uint32_t count = 0;
    VALIDATECALL(zesDeviceEnumFrequencyDomains(device, &count, nullptr));
    handles.resize(count);
    VALIDATECALL(zesDeviceEnumFrequencyDomains(device, &count, handles.data()));
    for (auto const &freqHandle : handles) {
        zes_freq_properties_t freqProperties = {};
        VALIDATECALL(zesFrequencyGetProperties(freqHandle, &freqProperties));
        ;
        if (freqProperties.type == ZES_FREQ_DOMAIN_GPU) {
            handle = freqHandle;
            break;
        }
    }
}

void Frequency::capture(std::string s) {
    zes_freq_state_t freqState = {};
    VALIDATECALL(zesFrequencyGetState(handle, &freqState));
    frequencies.push_back(std::make_pair(freqState.actual, s));
}

void Frequency::showAll(double minDiff) {

    double prevFrequency = std::numeric_limits<double>::max();
    bool useInitialValue = true;

    for (const auto &[frequency, s] : frequencies) {

        // Show only frequency differences greater than a specific value
        auto absDiff = static_cast<uint64_t>(prevFrequency - frequency);
        if (absDiff > minDiff || (useInitialValue == true)) {
            LOG(zmu::LogLevel::DEBUG) << s << ": " << frequency << std::endl;
            prevFrequency = frequency;
            useInitialValue = false;
        }
    }

    double min = 0.0, max = 0.0;
    getMinMax(min, max);

    LOG(zmu::LogLevel::INFO) << "MinimumFrequency: " << min << "Hz | MaximumFrequency: " << max << "Hz \n";
}

void Frequency::getMinMax(double &min, double &max) {

    std::pair<double, double> minMaxFrequency(std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::min());
    for (const auto &frequencyInfo : frequencies) {
        const auto &frequency = frequencyInfo.first;
        minMaxFrequency.first = std::min(frequency, minMaxFrequency.first);
        minMaxFrequency.second = std::max(frequency, minMaxFrequency.second);
    }

    min = minMaxFrequency.first;
    max = minMaxFrequency.second;
}
