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

    do {
        // Execute workload.
        VALIDATECALL(zeCommandQueueExecuteCommandLists(commandQueue, 1, &commandList, nullptr));

        // If using async command queue, explicit sync must be used for correctness.
        VALIDATECALL(zeCommandQueueSynchronize(commandQueue, std::numeric_limits<uint32_t>::max()));

        auto currentTime = std::chrono::steady_clock::now();
        diff = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count());

        runCount++;
    } while (diff < executionTimeInMilliSeconds);

    LOG(zmu::LogLevel::DEBUG) << "CommandList Run count : " << runCount << "\n";

    return true;
}
