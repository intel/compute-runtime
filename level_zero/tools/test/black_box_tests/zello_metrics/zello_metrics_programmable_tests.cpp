/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"
#include "level_zero/zet_intel_gpu_metric.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace zmu = ZelloMetricsUtility;

//////////////////////////
/// displayAllProgrammables
//////////////////////////
bool displayAllProgrammables() {
    // This test shows all programmables available on the device along with its parameter information
    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto printValueInfo = [](zet_value_info_type_exp_t type, zet_value_info_exp_t valueInfo) {
        switch (static_cast<int32_t>(type)) {
        case ZET_VALUE_INFO_TYPE_EXP_UINT32:
            LOG(zmu::LogLevel::INFO) << valueInfo.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64:
            LOG(zmu::LogLevel::INFO) << valueInfo.ui64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:
            LOG(zmu::LogLevel::INFO) << valueInfo.fp32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
            LOG(zmu::LogLevel::INFO) << valueInfo.fp64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_BOOL8:
            LOG(zmu::LogLevel::INFO) << valueInfo.b8;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT8:
            LOG(zmu::LogLevel::INFO) << valueInfo.ui8;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT16:
            LOG(zmu::LogLevel::INFO) << valueInfo.ui16;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:
            LOG(zmu::LogLevel::INFO) << "[" << valueInfo.ui64Range.ui64Min << "," << valueInfo.ui64Range.ui64Max << "]";
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE: {
            double minVal = 0;
            memcpy(&minVal, &valueInfo.ui64Range.ui64Min, sizeof(uint64_t));
            double maxVal = 0;
            memcpy(&maxVal, &valueInfo.ui64Range.ui64Max, sizeof(uint64_t));
            LOG(zmu::LogLevel::INFO) << "[" << minVal << "," << maxVal << "]";
        } break;
        default:
            VALIDATECALL(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            break;
        }
    };

    auto printValue = [](zet_value_info_type_exp_t type, zet_value_t value) {
        switch (static_cast<int32_t>(type)) {
        case ZET_VALUE_INFO_TYPE_EXP_UINT32:
            LOG(zmu::LogLevel::INFO) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64:
            LOG(zmu::LogLevel::INFO) << value.ui64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:
            LOG(zmu::LogLevel::INFO) << value.fp32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
            LOG(zmu::LogLevel::INFO) << value.fp64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_BOOL8:
            LOG(zmu::LogLevel::INFO) << static_cast<bool>(value.b8);
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT8:
            LOG(zmu::LogLevel::INFO) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT16:
            LOG(zmu::LogLevel::INFO) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:
            LOG(zmu::LogLevel::INFO) << value.ui64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE:
            LOG(zmu::LogLevel::INFO) << value.fp64;
            break;
        default:
            VALIDATECALL(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            break;
        }
    };

    auto showProgrammableInfo = [&](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Displaying Programmable Info : Device [" << deviceId << ", " << subDeviceId << " ] \n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);

        uint32_t programmableCount = 0;
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, nullptr));
        LOG(zmu::LogLevel::INFO) << "Programmable Count" << programmableCount << "\n";
        std::vector<zet_metric_programmable_exp_handle_t> metricProgrammables(programmableCount);
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, metricProgrammables.data()));

        // Print Programmable information
        for (auto &programmable : metricProgrammables) {
            zet_metric_programmable_exp_properties_t props{};
            VALIDATECALL(zetMetricProgrammableGetPropertiesExp(programmable, &props));
            LOG(zmu::LogLevel::INFO) << "name:" << props.name << " | desc:" << props.description << " | comp:" << props.component << "\n";
            LOG(zmu::LogLevel::INFO) << "\t paramCount:" << props.parameterCount << " | samplingType:" << props.samplingType << " | sourceId:" << props.sourceId << "\n";
            LOG(zmu::LogLevel::INFO) << "\t tierNum:" << props.tierNumber << " | domain:" << props.domain << "\n";

            std::vector<zet_metric_programmable_param_info_exp_t> parameterInfos(props.parameterCount);
            uint32_t parameterCount = props.parameterCount;
            zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, parameterInfos.data());

            // Print Parameter information in the programmable
            for (uint32_t paramIndex = 0; paramIndex < props.parameterCount; paramIndex++) {
                LOG(zmu::LogLevel::INFO) << "\t\t [" << paramIndex << "] "
                                         << "name:" << parameterInfos[paramIndex].name << " | type:" << parameterInfos[paramIndex].type << " | valInfoCount:" << parameterInfos[paramIndex].valueInfoCount
                                         << " | valInfoType:" << parameterInfos[paramIndex].valueInfoType << " | default ";
                printValue(parameterInfos[paramIndex].valueInfoType, parameterInfos[paramIndex].defaultValue);

                uint32_t paramValueInfoCount = parameterInfos[paramIndex].valueInfoCount;
                std::vector<zet_metric_programmable_param_value_info_exp_t> paramValueInfos(paramValueInfoCount);
                VALIDATECALL(zetMetricProgrammableGetParamValueInfoExp(programmable, paramIndex, &paramValueInfoCount, paramValueInfos.data()));

                // Print Parameter Value Information in the programmable
                for (uint32_t valInfoIndex = 0; valInfoIndex < parameterInfos[paramIndex].valueInfoCount; valInfoIndex++) {
                    LOG(zmu::LogLevel::INFO) << "\n";
                    LOG(zmu::LogLevel::INFO) << "\t\t\t [" << valInfoIndex << "] "
                                             << "desc:" << paramValueInfos[paramIndex].description << " | value:";
                    printValueInfo(parameterInfos[paramIndex].valueInfoType, paramValueInfos[valInfoIndex].valueInfo);
                }
                LOG(zmu::LogLevel::INFO) << "\n\n";
            }
            LOG(zmu::LogLevel::INFO) << " --------------------------------------------------------\n";
        }

        return true;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= showProgrammableInfo(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= showProgrammableInfo(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= showProgrammableInfo(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

//////////////////////////
/// CreateMetricFromProgrammables
//////////////////////////
bool createMetricFromProgrammables() {
    // This test creates metric from programmables
    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto printValue = [](zet_value_info_type_exp_t type, zet_value_t value) {
        switch (static_cast<int32_t>(type)) {
        case ZET_VALUE_INFO_TYPE_EXP_UINT32:
            LOG(zmu::LogLevel::DEBUG) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64:
            LOG(zmu::LogLevel::DEBUG) << value.ui64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:
            LOG(zmu::LogLevel::DEBUG) << value.fp32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
            LOG(zmu::LogLevel::DEBUG) << value.fp64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_BOOL8:
            LOG(zmu::LogLevel::DEBUG) << static_cast<bool>(value.b8);
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT8:
            LOG(zmu::LogLevel::DEBUG) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT16:
            LOG(zmu::LogLevel::DEBUG) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:
            LOG(zmu::LogLevel::DEBUG) << value.ui64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE:
            LOG(zmu::LogLevel::DEBUG) << value.fp64;
            break;
        default:
            VALIDATECALL(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            break;
        }
    };

    auto showProgrammableInfo = [&](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Displaying Programmable Info : Device [" << deviceId << ", " << subDeviceId << " ] \n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);

        uint32_t programmableCount = 0;
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, nullptr));
        LOG(zmu::LogLevel::INFO) << "Programmable Count" << programmableCount << "\n";
        std::vector<zet_metric_programmable_exp_handle_t> metricProgrammables(programmableCount);
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, metricProgrammables.data()));

        std::vector<zet_metric_programmable_param_value_exp_t> parameterValues{};

        // Print Programmable information
        for (auto &programmable : metricProgrammables) {
            zet_metric_programmable_exp_properties_t props{};
            VALIDATECALL(zetMetricProgrammableGetPropertiesExp(programmable, &props));
            LOG(zmu::LogLevel::INFO) << "Programmable name:" << props.name << " | desc:" << props.description << "|";
            LOG(zmu::LogLevel::INFO) << " paramCount:" << props.parameterCount << " | samplingType:" << props.samplingType << "| ";
            LOG(zmu::LogLevel::INFO) << " tierNum:" << props.tierNumber << " | domain:" << props.domain << "\n";

            std::vector<zet_metric_programmable_param_info_exp_t> parameterInfos(props.parameterCount);
            uint32_t parameterCount = props.parameterCount;
            VALIDATECALL(zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, parameterInfos.data()));
            EXPECT(parameterCount == props.parameterCount);

            parameterValues.clear();
            parameterValues.resize(props.parameterCount);

            // Print Parameter information in the programmable
            for (uint32_t paramIndex = 0; paramIndex < props.parameterCount; paramIndex++) {
                LOG(zmu::LogLevel::DEBUG) << "\t\t [" << paramIndex << "] "
                                          << " Param name:" << parameterInfos[paramIndex].name << " | type:" << parameterInfos[paramIndex].type << " | valInfoCount:" << parameterInfos[paramIndex].valueInfoCount
                                          << " | valInfoType:" << parameterInfos[paramIndex].valueInfoType << " | default ";
                printValue(parameterInfos[paramIndex].valueInfoType, parameterInfos[paramIndex].defaultValue);
                parameterValues[paramIndex].value = parameterInfos[paramIndex].defaultValue;
                LOG(zmu::LogLevel::DEBUG) << "\n";
            }

            uint32_t metricHandleCount = 0;
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, props.parameterCount, parameterValues.data(),
                                                             props.name, props.description,
                                                             &metricHandleCount, nullptr));
            std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, props.parameterCount, parameterValues.data(),
                                                             props.name, props.description,
                                                             &metricHandleCount, metricHandles.data()));
            LOG(zmu::LogLevel::INFO) << "MetricHandle Count: " << metricHandleCount << "\n";
            for (uint32_t j = 0; j < metricHandleCount; ++j) {
                const zet_metric_handle_t metric = metricHandles[j];
                zet_metric_properties_t metricProperties = {};
                VALIDATECALL(zetMetricGetProperties(metric, &metricProperties));
                LOG(zmu::LogLevel::INFO) << "\tMETRIC[" << j << "]: "
                                         << "desc: " << metricProperties.description << "\n";
                LOG(zmu::LogLevel::INFO) << "\t\t -> name: " << metricProperties.name << " | "
                                         << "metricType: " << metricProperties.metricType << " | "
                                         << "resultType: " << metricProperties.resultType << " | "
                                         << "units: " << metricProperties.resultUnits << " | "
                                         << "component: " << metricProperties.component << " | "
                                         << "tier: " << metricProperties.tierNumber << " | " << std::endl;
            }

            for (uint32_t j = 0; j < metricHandleCount; ++j) {
                VALIDATECALL(zetMetricDestroyExp(metricHandles[j]));
            }

            LOG(zmu::LogLevel::DEBUG) << " --------------------------------------------------------\n";
        }

        return true;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= showProgrammableInfo(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= showProgrammableInfo(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= showProgrammableInfo(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

//////////////////////////
/// testProgrammableMetricGroupCreateDestroy
//////////////////////////
bool testProgrammableMetricGroupCreateDestroy() {
    // This test creates metric from programmables
    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto printValue = [](zet_value_info_type_exp_t type, zet_value_t value) {
        switch (static_cast<int32_t>(type)) {
        case ZET_VALUE_INFO_TYPE_EXP_UINT32:
            LOG(zmu::LogLevel::DEBUG) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64:
            LOG(zmu::LogLevel::DEBUG) << value.ui64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT32:
            LOG(zmu::LogLevel::DEBUG) << value.fp32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64:
            LOG(zmu::LogLevel::DEBUG) << value.fp64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_BOOL8:
            LOG(zmu::LogLevel::DEBUG) << static_cast<bool>(value.b8);
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT8:
            LOG(zmu::LogLevel::DEBUG) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT16:
            LOG(zmu::LogLevel::DEBUG) << value.ui32;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_UINT64_RANGE:
            LOG(zmu::LogLevel::DEBUG) << value.ui64;
            break;
        case ZET_VALUE_INFO_TYPE_EXP_FLOAT64_RANGE:
            LOG(zmu::LogLevel::DEBUG) << value.fp64;
            break;
        default:
            VALIDATECALL(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            break;
        }
    };

    auto testMetricGroupCreateDestroy = [&](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Test Programmable Metric Group Create and Destroy: Device [" << deviceId << ", " << subDeviceId << " ] \n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);

        uint32_t programmableCount = 0;
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, nullptr));
        LOG(zmu::LogLevel::DEBUG) << "Programmable Count" << programmableCount << "\n";
        std::vector<zet_metric_programmable_exp_handle_t> metricProgrammables(programmableCount);
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, metricProgrammables.data()));

        std::vector<zet_metric_programmable_param_value_exp_t> parameterValues{};

        auto &programmable = metricProgrammables[0];
        {
            zet_metric_programmable_exp_properties_t props{};
            VALIDATECALL(zetMetricProgrammableGetPropertiesExp(programmable, &props));
            std::vector<zet_metric_programmable_param_info_exp_t> parameterInfos(props.parameterCount);
            uint32_t parameterCount = props.parameterCount;
            VALIDATECALL(zetMetricProgrammableGetParamInfoExp(programmable, &parameterCount, parameterInfos.data()));
            EXPECT(parameterCount == props.parameterCount);

            parameterValues.clear();
            parameterValues.resize(props.parameterCount);

            // Print Parameter information in the programmable
            for (uint32_t paramIndex = 0; paramIndex < props.parameterCount; paramIndex++) {
                LOG(zmu::LogLevel::DEBUG) << "\t\t [" << paramIndex << "] "
                                          << " Param name:" << parameterInfos[paramIndex].name << " | type:" << parameterInfos[paramIndex].type << " | valInfoCount:" << parameterInfos[paramIndex].valueInfoCount
                                          << " | valInfoType:" << parameterInfos[paramIndex].valueInfoType << " | default ";
                printValue(parameterInfos[paramIndex].valueInfoType, parameterInfos[paramIndex].defaultValue);
                parameterValues[paramIndex].value = parameterInfos[paramIndex].defaultValue;
                LOG(zmu::LogLevel::DEBUG) << "\n";
            }

            uint32_t metricHandleCount = 0;
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, props.parameterCount, parameterValues.data(),
                                                             props.name, props.description,
                                                             &metricHandleCount, nullptr));
            std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, props.parameterCount, parameterValues.data(),
                                                             props.name, props.description,
                                                             &metricHandleCount, metricHandles.data()));
            LOG(zmu::LogLevel::DEBUG) << "MetricHandle Count: " << metricHandleCount << "\n";

            zet_metric_group_handle_t metricGroup{};
            uint32_t metricGroupCount = 1;
            VALIDATECALL(zetDeviceCreateMetricGroupsFromMetricsExp(executionCtxt->getDeviceHandle(0), 1, &metricHandles[0], "name", "desc", &metricGroupCount, &metricGroup));
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
            zmu::printMetricGroupProperties(metricGroupProperties);

            VALIDATECALL(zetMetricGroupCloseExp(metricGroup));
            VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
            LOG(zmu::LogLevel::INFO) << " Metric Count After initial close " << metricGroupProperties.metricCount << "\n";

            VALIDATECALL(zetMetricGroupCloseExp(metricGroup));

            VALIDATECALL(zetContextActivateMetricGroups(executionCtxt->getContextHandle(0),
                                                        executionCtxt->getDeviceHandle(0), 1,
                                                        &metricGroup));

            VALIDATECALL(zetContextActivateMetricGroups(executionCtxt->getContextHandle(0),
                                                        executionCtxt->getDeviceHandle(0), 0,
                                                        nullptr));
            VALIDATECALL(zetMetricGroupDestroyExp(metricGroup));

            for (uint32_t j = 0; j < metricHandleCount; ++j) {
                VALIDATECALL(zetMetricDestroyExp(metricHandles[j]));
            }

            LOG(zmu::LogLevel::DEBUG) << " --------------------------------------------------------\n";
        }

        return true;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= testMetricGroupCreateDestroy(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= testMetricGroupCreateDestroy(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= testMetricGroupCreateDestroy(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

/////////////////////////////////////////////////
/// testProgrammableMetricGroupAddRemoveMetric
/////////////////////////////////////////////////
bool testProgrammableMetricGroupAddRemoveMetric() {
    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto testMetricGroupAddRemoveMetric = [&](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Test Programmable Metric Group for adding and removing metrics: Device [" << deviceId << ", " << subDeviceId << " ] \n";
        LOG(zmu::LogLevel::INFO) << "1. Creates default metrics (programmable with default values) \n";
        LOG(zmu::LogLevel::INFO) << "2. Creates multiple metric groups from the metrics \n";
        LOG(zmu::LogLevel::INFO) << "3. Remove half of the metrics from each group and re-add them \n";
        LOG(zmu::LogLevel::INFO) << "4. Verify that metric count is as expected\n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);

        uint32_t programmableCount = 0;
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, nullptr));
        LOG(zmu::LogLevel::DEBUG) << "Programmable Count" << programmableCount << "\n";
        std::vector<zet_metric_programmable_exp_handle_t> metricProgrammables(programmableCount);
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, metricProgrammables.data()));

        // Create Metrics from all programmables
        std::vector<zet_metric_handle_t> metricHandlesFromProgrammables{};
        for (auto &programmable : metricProgrammables) {
            uint32_t metricHandleCount = 0;
            zet_metric_programmable_exp_properties_t programmableProperties{};
            VALIDATECALL(zetMetricProgrammableGetPropertiesExp(programmable, &programmableProperties));
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, programmableProperties.name, programmableProperties.description, &metricHandleCount, nullptr));
            std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, programmableProperties.name, programmableProperties.description, &metricHandleCount, metricHandles.data()));
            metricHandles.resize(metricHandleCount);
            metricHandlesFromProgrammables.insert(metricHandlesFromProgrammables.end(), metricHandles.begin(), metricHandles.end());
        }

        // Create metric groups from metrics
        uint32_t metricGroupCount = 0;
        VALIDATECALL(zetDeviceCreateMetricGroupsFromMetricsExp(executionCtxt->getDeviceHandle(0),
                                                               static_cast<uint32_t>(metricHandlesFromProgrammables.size()),
                                                               metricHandlesFromProgrammables.data(),
                                                               "metricGroupName",
                                                               "metricGroupDesc",
                                                               &metricGroupCount, nullptr));
        std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
        VALIDATECALL(zetDeviceCreateMetricGroupsFromMetricsExp(executionCtxt->getDeviceHandle(0),
                                                               static_cast<uint32_t>(metricHandlesFromProgrammables.size()),
                                                               metricHandlesFromProgrammables.data(),
                                                               "metricGroupName",
                                                               "metricGroupDesc",
                                                               &metricGroupCount, metricGroupHandles.data()));
        metricGroupHandles.resize(metricGroupCount);

        // From each metric group remove half of the metrics and re-add them
        bool runStatus = true;
        for (auto &metricGroup : metricGroupHandles) {

            // Get metric handles which were created from programmables

            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
            uint32_t actualMetricCount = metricGroupProperties.metricCount;
            LOG(zmu::LogLevel::INFO) << "Metric Group: " << metricGroupProperties.name << " | metric count : " << metricGroupProperties.metricCount << "\n";
            std::vector<zet_metric_handle_t> allMetricHandles(actualMetricCount);
            VALIDATECALL(zetMetricGet(metricGroup, &actualMetricCount, allMetricHandles.data()));

            // Get the list of removable metrics
            std::vector<zet_metric_handle_t> removableMetricsFromMetricGroup{};
            for (auto &metricHandle : allMetricHandles) {
                if (std::find(metricHandlesFromProgrammables.begin(), metricHandlesFromProgrammables.end(), metricHandle) !=
                    metricHandlesFromProgrammables.end()) {
                    removableMetricsFromMetricGroup.push_back(metricHandle);
                }
            }

            const uint32_t originalMetricCount = static_cast<uint32_t>(removableMetricsFromMetricGroup.size());
            LOG(zmu::LogLevel::INFO) << "Removable metric count : " << originalMetricCount << "\n";
            uint32_t halfMetricCount = originalMetricCount / 2u;

            for (uint32_t index = 0; index < halfMetricCount; index++) {
                VALIDATECALL(zetMetricGroupRemoveMetricExp(metricGroup, removableMetricsFromMetricGroup[index]));
            }
            VALIDATECALL(zetMetricGroupCloseExp(metricGroup));
            VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
            LOG(zmu::LogLevel::INFO) << "Metric count after removal: " << metricGroupProperties.metricCount << "\n";

            for (uint32_t index = 0; index < halfMetricCount; index++) {
                size_t errorStringSize = 0;
                VALIDATECALL(zetMetricGroupAddMetricExp(metricGroup, removableMetricsFromMetricGroup[index], &errorStringSize, nullptr));
            }
            VALIDATECALL(zetMetricGroupCloseExp(metricGroup));
            VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
            LOG(zmu::LogLevel::INFO) << "After adding all the metrics back, metric count : " << metricGroupProperties.metricCount << "\n";
            EXPECT(metricGroupProperties.metricCount == actualMetricCount);
            runStatus &= metricGroupProperties.metricCount == actualMetricCount;
        }

        for (auto &metricGroup : metricGroupHandles) {
            VALIDATECALL(zetMetricGroupDestroyExp(metricGroup));
        }
        metricGroupHandles.clear();

        for (auto &metricHandle : metricHandlesFromProgrammables) {
            VALIDATECALL(zetMetricDestroyExp(metricHandle));
        }
        metricHandlesFromProgrammables.clear();

        LOG(zmu::LogLevel::DEBUG) << " --------------------------------------------------------\n";
        return runStatus;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= testMetricGroupAddRemoveMetric(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= testMetricGroupAddRemoveMetric(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= testMetricGroupAddRemoveMetric(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

//////////////////////////////////////////
/// testProgrammableMetricGroupStreamer
//////////////////////////////////////////
bool testProgrammableMetricGroupStreamer() {
    // This test creates metric from programmables
    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto testMetricProgrammableStreamer = [&](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Test Programmable Metric Group for streamer: Device [" << deviceId << ", " << subDeviceId << " ] \n";
        LOG(zmu::LogLevel::INFO) << "1. Creates default metrics (programmable with default values) \n";
        LOG(zmu::LogLevel::INFO) << "2. Creates multiple metric groups from the metrics \n";
        LOG(zmu::LogLevel::INFO) << "3. Creates Concurrent metric groups \n";
        LOG(zmu::LogLevel::INFO) << "4. For each concurrent metric groups do streamer mode testing\n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);

        uint32_t programmableCount = 0;
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, nullptr));
        LOG(zmu::LogLevel::DEBUG) << "Programmable Count" << programmableCount << "\n";
        std::vector<zet_metric_programmable_exp_handle_t> metricProgrammables(programmableCount);
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, metricProgrammables.data()));

        // Create Metrics from all programmables
        std::vector<zet_metric_handle_t> metricHandlesFromProgrammables{};
        for (auto &programmable : metricProgrammables) {
            uint32_t metricHandleCount = 0;
            zet_metric_programmable_exp_properties_t programmableProperties{};
            VALIDATECALL(zetMetricProgrammableGetPropertiesExp(programmable, &programmableProperties));
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, programmableProperties.name, programmableProperties.description, &metricHandleCount, nullptr));
            std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, programmableProperties.name, programmableProperties.description, &metricHandleCount, metricHandles.data()));
            metricHandles.resize(metricHandleCount);
            metricHandlesFromProgrammables.insert(metricHandlesFromProgrammables.end(), metricHandles.begin(), metricHandles.end());
        }

        // Create metric groups from metrics
        uint32_t metricGroupCount = 0;
        VALIDATECALL(zetDeviceCreateMetricGroupsFromMetricsExp(executionCtxt->getDeviceHandle(0),
                                                               static_cast<uint32_t>(metricHandlesFromProgrammables.size()),
                                                               metricHandlesFromProgrammables.data(),
                                                               "metricGroupName",
                                                               "metricGroupDesc",
                                                               &metricGroupCount, nullptr));
        std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
        VALIDATECALL(zetDeviceCreateMetricGroupsFromMetricsExp(executionCtxt->getDeviceHandle(0),
                                                               static_cast<uint32_t>(metricHandlesFromProgrammables.size()),
                                                               metricHandlesFromProgrammables.data(),
                                                               "metricGroupName",
                                                               "metricGroupDesc",
                                                               &metricGroupCount, metricGroupHandles.data()));
        metricGroupHandles.resize(metricGroupCount);
        // filter out non-streamer metric groups
        metricGroupHandles.erase(std::remove_if(metricGroupHandles.begin(), metricGroupHandles.end(),
                                                [&](zet_metric_group_handle_t metricGroup) {
                                                    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
                                                    VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
                                                    auto toBeRemoved = metricGroupProperties.samplingType != ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED;
                                                    if (toBeRemoved) {
                                                        VALIDATECALL(zetMetricGroupDestroyExp(metricGroup));
                                                    }
                                                    return toBeRemoved;
                                                }),
                                 metricGroupHandles.end());

        // print the finalized list of metric groups
        for (auto &metricGroup : metricGroupHandles) {
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
            zmu::printMetricGroupProperties(metricGroupProperties);
        }

        // Create concurrent metric groups
        uint32_t concurrentGroupCount = 0;
        VALIDATECALL(zetDeviceGetConcurrentMetricGroupsExp(executionCtxt->getDeviceHandle(0),
                                                           static_cast<uint32_t>(metricGroupHandles.size()),
                                                           metricGroupHandles.data(),
                                                           nullptr, &concurrentGroupCount));
        std::vector<uint32_t> countPerConcurrentGroupList(concurrentGroupCount);
        VALIDATECALL(zetDeviceGetConcurrentMetricGroupsExp(executionCtxt->getDeviceHandle(0),
                                                           static_cast<uint32_t>(metricGroupHandles.size()),
                                                           metricGroupHandles.data(),
                                                           countPerConcurrentGroupList.data(), &concurrentGroupCount));
        countPerConcurrentGroupList.resize(concurrentGroupCount);
        // Activate and collect
        // Streamer allows single metric group collection.
        bool runStatus = true;

        for (auto &metricGroup : metricGroupHandles) {
            executionCtxt->reset();
            executionCtxt->setExecutionTimeInMilliseconds(5);
            std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload1 =
                std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
            std::unique_ptr<SingleMetricStreamerCollector> collector =
                std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroup);
            std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
            testRunner->addCollector(collector.get());
            testRunner->addWorkload(workload1.get());

            runStatus &= testRunner->run();

            if (!runStatus) {
                break;
            }
        }

        for (auto &metricGroup : metricGroupHandles) {
            VALIDATECALL(zetMetricGroupDestroyExp(metricGroup));
        }
        metricGroupHandles.clear();

        for (auto &metricHandle : metricHandlesFromProgrammables) {
            VALIDATECALL(zetMetricDestroyExp(metricHandle));
        }
        metricHandlesFromProgrammables.clear();

        LOG(zmu::LogLevel::DEBUG) << " --------------------------------------------------------\n";
        return runStatus;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= testMetricProgrammableStreamer(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= testMetricProgrammableStreamer(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= testMetricProgrammableStreamer(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

//////////////////////////////////////////
/// testProgrammableMetricGroupQuery
//////////////////////////////////////////
bool testProgrammableMetricGroupQuery() {
    // This test creates metric from programmables
    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto testMetricProgrammableQuery = [&](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Test Programmable Metric Group For Query Mode: Device [" << deviceId << ", " << subDeviceId << " ] \n";
        LOG(zmu::LogLevel::INFO) << "1. Creates default metrics (programmable with default values) \n";
        LOG(zmu::LogLevel::INFO) << "2. Creates multiple metric groups from the metrics \n";
        LOG(zmu::LogLevel::INFO) << "3. Creates Concurrent metric groups \n";
        LOG(zmu::LogLevel::INFO) << "4. For each concurrent metric groups do Query mode testing\n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);

        uint32_t programmableCount = 0;
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, nullptr));
        LOG(zmu::LogLevel::DEBUG) << "Programmable Count" << programmableCount << "\n";
        std::vector<zet_metric_programmable_exp_handle_t> metricProgrammables(programmableCount);
        VALIDATECALL(zetMetricProgrammableGetExp(executionCtxt->getDeviceHandle(0), &programmableCount, metricProgrammables.data()));

        // Create Metrics from all programmables
        std::vector<zet_metric_handle_t> metricHandlesFromProgrammables{};
        for (auto &programmable : metricProgrammables) {
            uint32_t metricHandleCount = 0;
            zet_metric_programmable_exp_properties_t programmableProperties{};
            VALIDATECALL(zetMetricProgrammableGetPropertiesExp(programmable, &programmableProperties));
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, programmableProperties.name, programmableProperties.description, &metricHandleCount, nullptr));
            std::vector<zet_metric_handle_t> metricHandles(metricHandleCount);
            VALIDATECALL(zetMetricCreateFromProgrammableExp2(programmable, 0, nullptr, programmableProperties.name, programmableProperties.description, &metricHandleCount, metricHandles.data()));
            metricHandles.resize(metricHandleCount);
            metricHandlesFromProgrammables.insert(metricHandlesFromProgrammables.end(), metricHandles.begin(), metricHandles.end());
        }

        // Create metric groups from metrics
        uint32_t metricGroupCount = 0;
        VALIDATECALL(zetDeviceCreateMetricGroupsFromMetricsExp(executionCtxt->getDeviceHandle(0),
                                                               static_cast<uint32_t>(metricHandlesFromProgrammables.size()),
                                                               metricHandlesFromProgrammables.data(),
                                                               "metricGroupName",
                                                               "metricGroupDesc",
                                                               &metricGroupCount, nullptr));
        std::vector<zet_metric_group_handle_t> metricGroupHandles(metricGroupCount);
        VALIDATECALL(zetDeviceCreateMetricGroupsFromMetricsExp(executionCtxt->getDeviceHandle(0),
                                                               static_cast<uint32_t>(metricHandlesFromProgrammables.size()),
                                                               metricHandlesFromProgrammables.data(),
                                                               "metricGroupName",
                                                               "metricGroupDesc",
                                                               &metricGroupCount, metricGroupHandles.data()));
        metricGroupHandles.resize(metricGroupCount);
        // filter out non-query metric groups
        metricGroupHandles.erase(std::remove_if(metricGroupHandles.begin(), metricGroupHandles.end(),
                                                [](zet_metric_group_handle_t metricGroup) {
                                                    zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
                                                    VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
                                                    auto toBeRemoved = metricGroupProperties.samplingType != ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED;
                                                    if (toBeRemoved) {
                                                        VALIDATECALL(zetMetricGroupDestroyExp(metricGroup));
                                                    }
                                                    return toBeRemoved;
                                                }),
                                 metricGroupHandles.end());

        // print the finalized list of metric groups
        for (auto &metricGroup : metricGroupHandles) {
            zet_metric_group_properties_t metricGroupProperties = {ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES, nullptr};
            VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));
            zmu::printMetricGroupProperties(metricGroupProperties);
        }

        // Create concurrent metric groups
        uint32_t concurrentGroupCount = 0;
        VALIDATECALL(zetDeviceGetConcurrentMetricGroupsExp(executionCtxt->getDeviceHandle(0),
                                                           static_cast<uint32_t>(metricGroupHandles.size()),
                                                           metricGroupHandles.data(),
                                                           nullptr, &concurrentGroupCount));
        std::vector<uint32_t> countPerConcurrentGroupList(concurrentGroupCount);
        VALIDATECALL(zetDeviceGetConcurrentMetricGroupsExp(executionCtxt->getDeviceHandle(0),
                                                           static_cast<uint32_t>(metricGroupHandles.size()),
                                                           metricGroupHandles.data(),
                                                           countPerConcurrentGroupList.data(), &concurrentGroupCount));
        countPerConcurrentGroupList.resize(concurrentGroupCount);
        // Activate and collect
        // Streamer allows single metric group collection.
        bool runStatus = true;

        for (auto &metricGroup : metricGroupHandles) {
            executionCtxt->reset();
            executionCtxt->setExecutionTimeInMilliseconds(5);
            std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload1 =
                std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
            std::unique_ptr<SingleMetricQueryCollector> collector =
                std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), metricGroup);
            std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
            testRunner->addCollector(collector.get());
            testRunner->addWorkload(workload1.get());

            runStatus &= testRunner->run();

            if (!runStatus) {
                break;
            }
        }

        for (auto &metricGroup : metricGroupHandles) {
            VALIDATECALL(zetMetricGroupDestroyExp(metricGroup));
        }
        metricGroupHandles.clear();

        for (auto &metricHandle : metricHandlesFromProgrammables) {
            VALIDATECALL(zetMetricDestroyExp(metricHandle));
        }
        metricHandlesFromProgrammables.clear();

        LOG(zmu::LogLevel::DEBUG) << " --------------------------------------------------------\n";
        return runStatus;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= testMetricProgrammableQuery(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= testMetricProgrammableQuery(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= testMetricProgrammableQuery(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

ZELLO_METRICS_ADD_TEST(displayAllProgrammables)
ZELLO_METRICS_ADD_TEST(createMetricFromProgrammables)
ZELLO_METRICS_ADD_TEST(testProgrammableMetricGroupCreateDestroy)
ZELLO_METRICS_ADD_TEST(testProgrammableMetricGroupStreamer)
ZELLO_METRICS_ADD_TEST(testProgrammableMetricGroupQuery)
ZELLO_METRICS_ADD_TEST(testProgrammableMetricGroupAddRemoveMetric)