/*
 * Copyright (C) 2022-2025 Intel Corporation
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
    std::vector<ze_driver_handle_t> driverHandles = {};
    // Obtain driver.
    VALIDATECALL(zeDriverGet(&driverCount, nullptr));
    EXPECT(driverCount > 0);
    driverHandles.resize(driverCount);

    VALIDATECALL(zeDriverGet(&driverCount, driverHandles.data()));
    return driverHandles[0];
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

    ze_device_properties_t deviceProperties = {};
    deviceProperties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    deviceProperties.pNext = nullptr;
    VALIDATECALL(zeDeviceGetProperties(devices[0], &deviceProperties));
    machineConfig.deviceId = deviceProperties.deviceId;
    return true;
}

bool isDeviceAvailable(uint32_t deviceIndex, int32_t subDeviceIndex) {
    bool checkStatus = false;

    TestMachineConfiguration config;
    getTestMachineConfiguration(config);
    if (deviceIndex < config.deviceCount) {
        if (subDeviceIndex < static_cast<int32_t>(config.devices[deviceIndex].subDeviceCount)) {
            checkStatus = true;
        }
    }

    if (checkStatus == false) {
        LOG(LogLevel::WARNING) << "Warning:: Unsupported Configuration: Device :" << deviceIndex << "  Sub Device :" << subDeviceIndex << "\n";
    }
    return checkStatus;
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
        LOG(LogLevel::ERROR) << "No queue groups found!\n";
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

ze_command_list_handle_t createImmediateCommandList(ze_context_handle_t &contextHandle, ze_device_handle_t &deviceHandle) {

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

    queueDescription.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDescription.pNext = nullptr;
    queueDescription.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    queueDescription.flags = ZE_COMMAND_QUEUE_FLAG_EXPLICIT_ONLY;
    queueDescription.index = queueDescription.ordinal;

    // Create command list.
    ze_command_list_handle_t commandList = {};
    VALIDATECALL(zeCommandListCreateImmediate(contextHandle, deviceHandle, &queueDescription, &commandList));
    return commandList;
}

void printMetricGroupAndMetricProperties(zet_metric_group_handle_t metricGroup) {
    if (TestSettings::get()->verboseLevel.get() >= LogLevel::DEBUG) {
        zet_metric_group_properties_t metricGroupProperties = {};
        // Obtain metric group properties to check the group name and sampling type.
        VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
        printMetricGroupProperties(metricGroupProperties);

        // Print metrics from metric group.
        uint32_t metricCount = 0;
        std::vector<zet_metric_handle_t> metrics = {};

        // Obtain metrics count for verbose purpose.
        VALIDATECALL(zetMetricGet(metricGroup, &metricCount, nullptr));

        // Obtain metrics for verbose purpose.
        metrics.resize(metricCount);
        VALIDATECALL(zetMetricGet(metricGroup, &metricCount, metrics.data()));

        // Enumerate metric group metrics for verbose purpose.
        for (uint32_t j = 0; j < metricCount; ++j) {

            const zet_metric_handle_t metric = metrics[j];
            zet_metric_properties_t metricProperties = {};
            VALIDATECALL(zetMetricGetProperties(metric, &metricProperties));
            printMetricProperties(metricProperties);
        }
    }
}

void printMetricGroupProperties(const zet_metric_group_properties_t &properties) {
    LOG(LogLevel::DEBUG) << "METRIC GROUP: "
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
    LOG(LogLevel::DEBUG) << "\tMETRIC: "
                         << "name: " << properties.name << ", "
                         << "desc: " << properties.description << ", "
                         << "component: " << properties.component << ", "
                         << "tier: " << properties.tierNumber << ", "
                         << "metricType: " << properties.metricType << ", "
                         << "resultType: " << properties.resultType << ", "
                         << "units: " << properties.resultUnits << std::endl;
}

/////////
/// wait
/////////
void sleep(uint32_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

zet_metric_group_handle_t findMetricGroup(const char *groupName,
                                          const zet_metric_group_sampling_type_flags_t samplingType,
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

    LOG(LogLevel::ERROR) << "Warning : Metric Group " << groupName << " with sampling type " << samplingType << " could not be found !\n";

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

bool shouldMetricBeCaptured(zet_metric_handle_t hMetric) {

    auto testSettings = TestSettings::get();

    zet_metric_properties_t metricProperties = {};
    VALIDATECALL(zetMetricGetProperties(hMetric, &metricProperties));

    if (testSettings->metricNames.size() > 0) {
        for (auto &metricName : testSettings->metricNames) {
            if (strncmp(metricName.get().c_str(), metricProperties.name, sizeof(metricProperties.name)) == 0) {
                return true;
            }
        }
        return false;
    }

    return true;
}

void obtainCalculatedMetrics(zet_metric_group_handle_t metricGroup, uint8_t *rawData, uint32_t rawDataSize) {

    uint32_t setCount = 0;
    uint32_t totalCalculatedMetricCount = 0;
    zet_metric_group_properties_t properties = {};
    std::vector<uint32_t> metricCounts = {};
    std::vector<zet_typed_value_t> results = {};
    std::vector<zet_metric_handle_t> metrics = {};

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

    // Obtain metric group properties to show each metric.
    VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &properties));

    // Allocate space for all metrics from a given metric group.
    metrics.resize(properties.metricCount);

    // Obtain metrics from a given metric group.
    VALIDATECALL(zetMetricGet(metricGroup, &properties.metricCount, metrics.data()));

    for (uint32_t i = 0; i < setCount; ++i) {

        LOG(LogLevel::DEBUG) << "\r\nSet " << i;

        const uint32_t metricCount = properties.metricCount;
        const uint32_t metricCountForSet = metricCounts[i];

        for (uint32_t j = 0; j < metricCountForSet; j++) {

            const uint32_t resultIndex = j + metricCount * i;
            const uint32_t metricIndex = j % metricCount;
            zet_metric_properties_t metricProperties = {};

            VALIDATECALL((resultIndex < totalCalculatedMetricCount) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN)

            // Obtain single metric properties to learn output value type.
            VALIDATECALL(zetMetricGetProperties(metrics[metricIndex], &metricProperties));

            if (!shouldMetricBeCaptured(metrics[metricIndex])) {
                continue;
            }

            VALIDATECALL((results[resultIndex].type == metricProperties.resultType) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN)

            if (metricIndex == 0) {
                LOG(LogLevel::DEBUG) << "\r\n";
            }

            LOG(LogLevel::DEBUG) << "\r\n";
            LOG(LogLevel::DEBUG) << std::setw(25) << metricProperties.name << ": ";

            switch (results[resultIndex].type) {
            case zet_value_type_t::ZET_VALUE_TYPE_BOOL8:
                LOG(LogLevel::DEBUG) << std::setw(12);
                LOG(LogLevel::DEBUG) << (results[resultIndex].value.b8 ? "true" : "false");
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_FLOAT32:
                LOG(LogLevel::DEBUG) << std::setw(12);
                LOG(LogLevel::DEBUG) << results[resultIndex].value.fp32;
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_FLOAT64:
                LOG(LogLevel::DEBUG) << std::setw(12);
                LOG(LogLevel::DEBUG) << results[resultIndex].value.fp64;
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_UINT32:
                LOG(LogLevel::DEBUG) << std::setw(12);
                LOG(LogLevel::DEBUG) << results[resultIndex].value.ui32;
                break;

            case zet_value_type_t::ZET_VALUE_TYPE_UINT64:
                LOG(LogLevel::DEBUG) << std::setw(12);
                LOG(LogLevel::DEBUG) << results[resultIndex].value.ui64;
                break;

            default:
                break;
            }
        }

        LOG(LogLevel::DEBUG) << "\r\n";
    }
}

inline bool isParamEnabled(int argc, char *argv[], const char *shortName, const char *longName, int *optind) {
    if (argc < 2) {
        return false;
    }
    int index = 1;
    char **arg = &argv[1];
    char **argE = &argv[argc];

    for (; arg != argE; ++arg) {
        if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
            *optind = index;
            return true;
        }
        index++;
    }

    return false;
}

inline std::string copyArg(int argc, char *argv[], int optind) {
    std::string valToReturn;
    char *optarg = nullptr;
    optind = optind + 1;
    if (optind < argc) {
        optarg = argv[optind];
    }
    if (optarg != nullptr) {
        valToReturn = optarg;
    }
    return valToReturn;
}

void TestSettings::readMetricNames(char *optArg) {
    std::string args = optArg;
    std::string delim(",");
    size_t pos = 0, prevPos = 0;
    metricNames.clear();
    while (1) {
        pos = args.find(delim, prevPos);
        if (pos == std::string::npos) {
            metricNames.push_back(args.substr(prevPos));
            break;
        }
        metricNames.push_back(args.substr(prevPos, pos - prevPos));
        prevPos = pos + delim.length();
    }

    for (auto &name : metricNames) {
        LOG(LogLevel::DEBUG) << "XX1: " << name.get() << "\n";
    }
}

bool isEnvVariableSet(const char *name) {
    const char *env = getenv(name);
    if ((nullptr == env) || (0 == strcmp("0", env)))
        return false;
    return (0 == strcmp("1", env));
}

////////////////
// Test Settings
////////////////
void TestSettings::parseArguments(int argc, char *argv[]) {
    int optind = 0;

    auto printUsage = []() {
        std::cout << "\n set Environment variable ZET_ENABLE_METRICS=1"
                     "\n"
                     "\n zello_metrics [OPTIONS]"
                     "\n"
                     "\n OPTIONS:"
                     "\n  -t,   --test <test name>              run the specific test(\"all\" runs all tests)"
                     "\n  -d,   --device <deviceId>             device ID to run the test"
                     "\n  -s,   --subdevice <subdeviceId>       sub-device ID to run the test"
                     "\n  -v,   --verboseLevel <verboseLevel>   verbosity level(-2:error|-1:warning|(default)0:info|1:debug)"
                     "\n  -m,   --metricName <name1,name2,..>   metric names seperated by a comma"
                     "\n  -M,   --metricGroupName <name>        metric group name"
                     "\n  -e,   --eventNReports <report count>  report count threshold for event generation"
                     "\n  -y,   --showSystemInfo                capture and show system info like frequency and power"
                     "\n                                        (requires ZES_ENABLE_SYSMAN=1)"
                     "\n  -h,   --help                          display help message"
                     "\n";
    };

    if (argc < 2) {
        printUsage();
        return;
    }

    if (isParamEnabled(argc, argv, "-h", "--help", &optind)) {
        printUsage();
        exit(0);
    }

    if (isParamEnabled(argc, argv, "-t", "--test", &optind)) {
        testName = copyArg(argc, argv, optind);
    }

    if (isParamEnabled(argc, argv, "-d", "--device", &optind)) {
        deviceId.set(std::atoi(copyArg(argc, argv, optind).c_str()));
    }

    if (isParamEnabled(argc, argv, "-s", "--subdevice", &optind)) {
        subDeviceId.set(std::atoi(copyArg(argc, argv, optind).c_str()));
    }

    if (isParamEnabled(argc, argv, "-v", "--verboseLevel", &optind)) {
        verboseLevel.set(std::atoi(copyArg(argc, argv, optind).c_str()));
    }

    if (isParamEnabled(argc, argv, "-M", "--metricGroupName", &optind)) {
        metricGroupName.set(copyArg(argc, argv, optind));
    }

    if (isParamEnabled(argc, argv, "-m", "--metricName", &optind)) {
        readMetricNames(&copyArg(argc, argv, optind)[0]);
    }

    if (isParamEnabled(argc, argv, "-e", "--eventNReports", &optind)) {
        eventNReportCount.set(std::atoi(copyArg(argc, argv, optind).c_str()));
    }

    if (isParamEnabled(argc, argv, "-y", "--showSystemInfo", &optind)) {
        showSystemInfo.set(true);
    }
}

TestSettings *TestSettings::get() {
    if (!settings) {
        settings = new TestSettings;
    }
    return settings;
}

TestSettings *TestSettings::settings = nullptr;

class DummyStreamBuf : public std::streambuf {};
DummyStreamBuf emptyStreamBuf;
std::ostream emptyCout(&emptyStreamBuf);

} // namespace ZelloMetricsUtility
