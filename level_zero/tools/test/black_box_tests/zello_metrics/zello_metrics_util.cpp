/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"

#include <cstring>
#include <iomanip>
#include <mutex>
#include <thread>

namespace ZelloMetricsUtility {
//////////////////////////
/// createL0
///////////////////////////
void createL0() {

    VALIDATECALL(zeInit(ZE_INIT_FLAG_GPU_ONLY));
}

ze_driver_handle_t getDriver() {
    uint32_t driverCount = 0;
    ze_driver_handle_t driverHandle = {};
    // Obtain driver.
    VALIDATECALL(zeDriverGet(&driverCount, nullptr));
    EXPECT(driverCount > 0);
    VALIDATECALL(zeDriverGet(&driverCount, &driverHandle));
    return driverHandle;
}

ze_context_handle_t createContext(ze_driver_handle_t &driverHandle) {
    // Obtain context.
    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_context_handle_t contextHandle = {};
    VALIDATECALL(zeContextCreate(driverHandle, &contextDesc, &contextHandle));
    return contextHandle;
}

bool getTestMachineConfiguration(TestMachineConfiguration &machineConfig) {

    std::vector<ze_device_handle_t> devices = {};

    createL0();
    ze_driver_handle_t driverHandle = getDriver();
    EXPECT((driverHandle != nullptr));

    uint32_t deviceCount = 0;
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, nullptr));
    deviceCount = std::min(deviceCount, MAX_DEVICES_IN_MACHINE);
    devices.resize(deviceCount);
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, devices.data()));

    machineConfig.deviceCount = deviceCount;
    for (uint32_t deviceId = 0; deviceId < deviceCount; deviceId++) {
        machineConfig.devices[deviceId].deviceIndex = deviceId;

        uint32_t subDevicesCount = 0;
        VALIDATECALL(zeDeviceGetSubDevices(devices[deviceId], &subDevicesCount, nullptr));
        machineConfig.devices[deviceId].subDeviceCount = subDevicesCount;
    }
    return true;
}

ze_device_handle_t getDevice(ze_driver_handle_t &driverHandle, uint32_t deviceIndex) {

    uint32_t deviceCount = 0;
    std::vector<ze_device_handle_t> devices;

    // Obtain all devices.
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, nullptr));
    EXPECT((deviceCount > deviceIndex));
    devices.resize(deviceCount);
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, devices.data()));

    return devices[deviceIndex];
}

ze_device_handle_t getSubDevice(ze_device_handle_t &deviceHandle, uint32_t subDeviceIndex) {

    uint32_t subDevicesCount = 0;
    std::vector<ze_device_handle_t> subDevices = {};

    // Sub devices count.
    VALIDATECALL(zeDeviceGetSubDevices(deviceHandle, &subDevicesCount, nullptr));
    EXPECT(subDevicesCount > subDeviceIndex);

    subDevices.resize(subDevicesCount);
    VALIDATECALL(zeDeviceGetSubDevices(deviceHandle, &subDevicesCount, subDevices.data()));
    return subDevices[subDeviceIndex];
}

ze_command_queue_handle_t createCommandQueue(ze_context_handle_t &contextHandle,
                                             ze_device_handle_t &deviceHandle) {

    uint32_t queueGroupsCount = 0;
    ze_command_queue_desc_t queueDescription = {};

    VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(deviceHandle, &queueGroupsCount, nullptr));

    if (queueGroupsCount == 0) {
        std::cout << "No queue groups found!\n";
        std::terminate();
    }

    std::vector<ze_command_queue_group_properties_t> queueProperties(queueGroupsCount);
    VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(deviceHandle, &queueGroupsCount,
                                                        queueProperties.data()));

    for (uint32_t i = 0; i < queueGroupsCount; ++i) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            queueDescription.ordinal = i;
        }
    }

    queueDescription.index = 0;
    queueDescription.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_command_queue_handle_t commandQueue = {};
    VALIDATECALL(zeCommandQueueCreate(contextHandle, deviceHandle, &queueDescription, &commandQueue));
    return commandQueue;
}

ze_command_list_handle_t createCommandList(ze_context_handle_t &contextHandle, ze_device_handle_t &deviceHandle) {

    ze_command_queue_desc_t queueDescription = {};
    uint32_t queueGroupsCount = 0;
    zeDeviceGetCommandQueueGroupProperties(deviceHandle, &queueGroupsCount, nullptr);
    std::vector<ze_command_queue_group_properties_t> queueProperties(queueGroupsCount);
    zeDeviceGetCommandQueueGroupProperties(deviceHandle, &queueGroupsCount, queueProperties.data());

    for (uint32_t i = 0; i < queueGroupsCount; ++i) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            queueDescription.ordinal = i;
        }
    }

    ze_command_list_desc_t commandListDesc = {};
    commandListDesc.commandQueueGroupOrdinal = queueDescription.ordinal;

    // Create command list.
    ze_command_list_handle_t commandList = {};
    VALIDATECALL(zeCommandListCreate(contextHandle, deviceHandle, &commandListDesc, &commandList));
    return commandList;
}

void printMetricGroupProperties(const zet_metric_group_properties_t &properties) {
    std::cout << "METRIC GROUP: "
              << "name: " << properties.name << ", "
              << "desc: " << properties.description << ", "
              << "samplingType: " << properties.samplingType << ", "
              << "domain: " << properties.domain << ", "
              << "metricCount: " << properties.metricCount << std::endl;
}

///////////////////////////
/// printMetricProperties
///////////////////////////
void printMetricProperties(const zet_metric_properties_t &properties) {
    std::cout << "\tMETRIC: "
              << "name: " << properties.name << ", "
              << "desc: " << properties.description << ", "
              << "component: " << properties.component << ", "
              << "tier: " << properties.tierNumber << ", "
              << "metricType: " << properties.metricType << ", "
              << "resultType: " << properties.resultType << ", "
              << "units: " << properties.resultUnits << std::endl;
}

///////////////////////////
/// wait
///////////////////////////
void sleep(uint32_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

zet_metric_group_handle_t findMetricGroup(const char *groupName,
                                          const zet_metric_group_sampling_type_flag_t samplingType,
                                          ze_device_handle_t deviceHandle) {

    uint32_t metricGroupCount = 0;
    std::vector<zet_metric_group_handle_t> metricGroups = {};

    // Obtain metric group count for a given device.
    VALIDATECALL(zetMetricGroupGet(deviceHandle, &metricGroupCount, nullptr));

    // Obtain all metric groups.
    metricGroups.resize(metricGroupCount);
    VALIDATECALL(zetMetricGroupGet(deviceHandle, &metricGroupCount, metricGroups.data()));

    // Enumerate metric groups to find a particular one with a given group name
    // and sampling type requested by the user.
    for (uint32_t i = 0; i < metricGroupCount; ++i) {

        const zet_metric_group_handle_t metricGroupHandle = metricGroups[i];
        zet_metric_group_properties_t metricGroupProperties = {};

        // Obtain metric group properties to check the group name and sampling type.
        VALIDATECALL(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties));

        const bool validGroupName = strcmp(metricGroupProperties.name, groupName) == 0;
        const bool validSamplingType = (metricGroupProperties.samplingType & samplingType);

        // Validating the name and sampling type.
        if (validSamplingType) {

            // Obtain metric group handle.
            if (validGroupName) {
                return metricGroupHandle;
            }
        }
    }

    // Unable to find metric group.
    VALIDATECALL(ZE_RESULT_ERROR_UNKNOWN);
    return nullptr;
}

ze_event_pool_handle_t createHostVisibleEventPool(ze_context_handle_t contextHandle, ze_device_handle_t deviceHandle) {

    ze_event_pool_handle_t eventPool = {};
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ze_event_pool_flag_t::ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    // Optional notification event to know if Streamer reports are ready to read.
    VALIDATECALL(zeEventPoolCreate(contextHandle, &eventPoolDesc, 1, &deviceHandle, &eventPool));
    return eventPool;
}

ze_event_handle_t createHostVisibleEvent(ze_event_pool_handle_t hostVisibleEventPool) {

    ze_event_desc_t notificationEventDesc = {};
    ze_event_handle_t notificationEvent = {};
    notificationEventDesc.index = 0;
    notificationEventDesc.wait = ze_event_scope_flag_t::ZE_EVENT_SCOPE_FLAG_HOST;
    notificationEventDesc.signal = ze_event_scope_flag_t::ZE_EVENT_SCOPE_FLAG_DEVICE;

    VALIDATECALL(zeEventCreate(hostVisibleEventPool, &notificationEventDesc, &notificationEvent));
    return notificationEvent;
}

void obtainCalculatedMetrics(zet_metric_group_handle_t metricGroup, uint8_t *rawData, uint32_t rawDataSize) {

    uint32_t setCount = 0;
    uint32_t totalCalculatedMetricCount = 0;
    zet_metric_group_properties_t properties = {};
    std::vector<uint32_t> metricCounts = {};
    std::vector<zet_typed_value_t> results = {};
    std::vector<zet_metric_handle_t> metrics = {};
    ze_result_t result = ZE_RESULT_SUCCESS;

    // Obtain maximum space for calculated metrics.
    result = zetMetricGroupCalculateMetricValues(
        metricGroup,
        ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
        rawDataSize, rawData,
        &totalCalculatedMetricCount,
        nullptr);

    if (result == ZE_RESULT_ERROR_UNKNOWN) {

        // Try to use calculate for multiple metric values.
        VALIDATECALL(zetMetricGroupCalculateMultipleMetricValuesExp(
            metricGroup,
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            rawDataSize, rawData,
            &setCount, &totalCalculatedMetricCount,
            nullptr, nullptr));

        // Allocate space for calculated reports.
        metricCounts.resize(setCount);
        results.resize(totalCalculatedMetricCount);

        // Obtain calculated metrics and their count.
        VALIDATECALL(zetMetricGroupCalculateMultipleMetricValuesExp(
            metricGroup,
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            rawDataSize, rawData,
            &setCount, &totalCalculatedMetricCount,
            metricCounts.data(), results.data()));

    } else {
        // Allocate space for calculated reports.
        setCount = 1;
        metricCounts.resize(setCount);
        results.resize(totalCalculatedMetricCount);

        // Obtain calculated metrics and their count.
        VALIDATECALL(zetMetricGroupCalculateMetricValues(
            metricGroup,
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            rawDataSize, rawData,
            &totalCalculatedMetricCount, results.data()));

        metricCounts[0] = totalCalculatedMetricCount;
    }

    // Obtain metric group properties to show each metric.
    VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &properties));

    // Allocate space for all metrics from a given metric group.
    metrics.resize(properties.metricCount);

    // Obtain metrics from a given metric group.
    VALIDATECALL(zetMetricGet(metricGroup, &properties.metricCount, metrics.data()));

    for (uint32_t i = 0; i < setCount; ++i) {

        std::cout << "\r\nSet " << i;

        const uint32_t metricCount = properties.metricCount;
        const uint32_t metricCountForSet = metricCounts[i];

        for (uint32_t j = 0; j < metricCountForSet; j++) {

            const uint32_t resultIndex = j + metricCount * i;
            const uint32_t metricIndex = j % metricCount;
            zet_metric_properties_t metricProperties = {};

            VALIDATECALL((resultIndex < totalCalculatedMetricCount) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN)

            // Obtain single metric properties to learn output value type.
            VALIDATECALL(zetMetricGetProperties(metrics[metricIndex], &metricProperties));

            VALIDATECALL((results[resultIndex].type == metricProperties.resultType) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN)

            if (metricIndex == 0) {
                std::cout << "\r\n";
            }

            std::cout << "\r\n";
            std::cout << std::setw(25) << metricProperties.name << ": ";

            switch (results[resultIndex].type) {
            case zet_value_type_t::ZET_VALUE_TYPE_BOOL8:
                std::cout << std::setw(12);
                std::cout << (results[resultIndex].value.b8 ? "true" : "false");
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_FLOAT32:
                std::cout << std::setw(12);
                std::cout << results[resultIndex].value.fp32;
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_FLOAT64:
                std::cout << std::setw(12);
                std::cout << results[resultIndex].value.fp64;
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_UINT32:
                std::cout << std::setw(12);
                std::cout << results[resultIndex].value.ui32;
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_UINT64:
                std::cout << std::setw(12);
                std::cout << results[resultIndex].value.ui64;
                break;

            default:
                break;
            }
        }

        std::cout << "\r\n";
    }
}

} // namespace ZelloMetricsUtility
