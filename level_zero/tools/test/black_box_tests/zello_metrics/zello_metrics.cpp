/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

////////////////////////
/////SingleDeviceTestRunner
////////////////////////
void SingleDeviceTestRunner::addCollector(Collector *collector) {
    // Add only if not available already
    if (std::find(collectorList.begin(), collectorList.end(), collector) == collectorList.end()) {
        collectorList.push_back(collector);
    }
}

void SingleDeviceTestRunner::addWorkload(Workload *workload) {
    // Add only if not available already
    if (std::find(workloadList.begin(), workloadList.end(), workload) == workloadList.end()) {
        workloadList.push_back(workload);
    }
}

bool SingleDeviceTestRunner::run() {

    bool status = true;

    status &= executionCtxt->activateMetricGroups();

    EXPECT(status == true);
    // Command List prepareation
    for (auto collector : collectorList) {
        status &= collector->start();
    }

    EXPECT(status == true);
    for (auto collector : collectorList) {
        status &= collector->prefixCommands();
    }

    EXPECT(status == true);
    for (auto workload : workloadList) {
        status &= workload->appendCommands();
    }

    EXPECT(status == true);
    for (auto collector : collectorList) {
        status &= collector->suffixCommands();
    }

    if (disableCommandListExecution == false) {
        EXPECT(status == true);
        // Command List execution
        executionCtxt->run();
    }

    EXPECT(status == true);
    for (auto collector : collectorList) {
        status &= collector->isDataAvailable();
    }

    EXPECT(status == true);
    for (auto workload : workloadList) {
        status &= workload->validate();
    }

    EXPECT(status == true);
    for (auto collector : collectorList) {
        collector->showResults();
    }

    for (auto collector : collectorList) {
        status &= collector->stop();
    }

    EXPECT(status == true);
    status &= executionCtxt->deactivateMetricGroups();

    return status;
}

///////////////////////////
/// query_device_0_sub_device_0
///////////////////////////
bool query_device_0_sub_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricQueryCollector> collector =
        std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());
    return testRunner->run();
}

///////////////////////////
/// query_device_root
///////////////////////////
bool query_device_root(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, -1);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricQueryCollector> collector =
        std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());
    return testRunner->run();
}

///////////////////////////
/// query_device_0_sub_device_1
///////////////////////////
bool query_device_0_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 / sub_device 1 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 1);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricQueryCollector> collector =
        std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());
    return testRunner->run();
}

///////////////////////////
/// query_device_0_sub_device_0_1
///////////////////////////
bool query_device_0_sub_device_0_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 / sub_device 0 : 1 ====-" << std::endl;

    ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
    ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

    if (machineConfig.devices[0].subDeviceCount < 2) {
        std::cout << "Error: Device has insufficient sub-devices" << std::endl;
        return false;
    }

    bool status = true;
    const char *metricGroupNames[] = {
        "TestOa",
        "ComputeBasic"};
    for (int32_t subDeviceId = 0; subDeviceId < 2; subDeviceId++) {
        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, subDeviceId);
        std::unique_ptr<CopyBufferToBuffer> workload =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<SingleMetricQueryCollector> collector =
            std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), metricGroupNames[subDeviceId]);

        std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(collector.get());
        testRunner->addWorkload(workload.get());
        status &= testRunner->run();
    }

    return status;
}

///////////////////////////
/// query_device_1_sub_device_1
///////////////////////////
bool query_device_1_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 1 / sub_device 1 ====-" << std::endl;

    ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
    ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

    if (machineConfig.deviceCount < 2) {
        std::cout << "Error: Device has insufficient devices" << std::endl;
        return false;
    }

    if (machineConfig.devices[0].subDeviceCount < 2) {
        std::cout << "Error: Device has insufficient sub-devices" << std::endl;
        return false;
    }

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(1, 1);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricQueryCollector> collector =
        std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());

    return testRunner->run();
}

///////////////////////////
/// query_device_1_sub_device_0
///////////////////////////
bool query_device_1_sub_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 1 ====-" << std::endl;

    ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
    ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

    if (machineConfig.deviceCount < 2) {
        std::cout << "Error: Device has insufficient devices" << std::endl;
        return false;
    }

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(1, 0);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricQueryCollector> collector =
        std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());

    return testRunner->run();
}

//////////////////////////////////
/// query_device_0_1_sub_device_0
//////////////////////////////////
bool query_device_0_1_sub_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 / 1 ====-" << std::endl;

    ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
    ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

    if (machineConfig.deviceCount < 2) {
        std::cout << "Error: Device has insufficient devices" << std::endl;
        return false;
    }

    bool status = true;
    const char *metricGroupNames[] = {
        "TestOa",
        "ComputeBasic"};
    for (int32_t deviceId = 0; deviceId < 2; deviceId++) {
        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, 0);
        std::unique_ptr<CopyBufferToBuffer> workload =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<SingleMetricQueryCollector> collector =
            std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), metricGroupNames[deviceId]);

        std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(collector.get());
        testRunner->addWorkload(workload.get());
        status &= testRunner->run();
    }

    return status;
}

bool stream_device_root(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, -1);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricStreamerCollector> collector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "ComputeBasic");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());

    return testRunner->run();
    ;
}

////////////////////////////////
/// stream_device_0_sub_device_0
////////////////////////////////
bool stream_device_0_sub_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricStreamerCollector> collector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());

    return testRunner->run();
}

///////////////////////////
/// stream_device_0_sub_device_1
///////////////////////////
bool stream_device_0_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 / sub_device 1 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 1);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricStreamerCollector> collector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());

    return testRunner->run();
}

///////////////////////////
/// stream_device_0_sub_device_0_1
///////////////////////////
bool stream_device_0_sub_device_0_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 / sub_device 0 : 1 ====-" << std::endl;

    ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
    ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

    if (machineConfig.devices[0].subDeviceCount < 2) {
        std::cout << "Error: Device has insufficient sub-devices" << std::endl;
        return false;
    }

    bool status = true;
    const char *metricGroupNames[] = {
        "TestOa",
        "ComputeBasic"};
    for (int32_t subDeviceId = 0; subDeviceId < 2; subDeviceId++) {
        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, subDeviceId);
        std::unique_ptr<CopyBufferToBuffer> workload =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<SingleMetricStreamerCollector> collector =
            std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroupNames[subDeviceId]);

        std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(collector.get());
        testRunner->addWorkload(workload.get());
        status &= testRunner->run();
    }

    return status;
}

/////////////////////////////////
/// stream_device_1_sub_device_0
/////////////////////////////////
bool stream_device_1_sub_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 1 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(1, 0);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricStreamerCollector> collector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "ComputeBasic");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());

    return testRunner->run();
}

///////////////////////////
/// stream_device_1_sub_device_1
///////////////////////////
bool stream_device_1_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 1 / sub_device 1 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(1, 1);
    std::unique_ptr<CopyBufferToBuffer> workload =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricStreamerCollector> collector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "ComputeBasic");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload.get());

    return testRunner->run();
}

///////////////////////////////////
/// stream_device_0_1_sub_device_0
///////////////////////////////////
bool stream_device_0_1_sub_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 / 1 ====-" << std::endl;

    ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
    ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

    if (machineConfig.deviceCount < 2) {
        std::cout << "Error: Device has insufficient devices" << std::endl;
        return false;
    }

    bool status = true;
    const char *metricGroupNames[] = {
        "TestOa",
        "ComputeBasic"};
    for (int32_t deviceId = 0; deviceId < 2; deviceId++) {
        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, 0);
        std::unique_ptr<CopyBufferToBuffer> workload =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<SingleMetricStreamerCollector> collector =
            std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroupNames[deviceId]);

        std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(collector.get());
        testRunner->addWorkload(workload.get());
        status &= testRunner->run();
    }

    return status;
}

/////////////////////////////////////////////
/// stream_ip_sampling_device_0_sub_device_0
/////////////////////////////////////////////
bool stream_ip_sampling_device_0_sub_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream Ip Sampling: device 0 / sub_device 0 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
    executionCtxt->setExecutionTimeInMilliseconds(200);
    std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload1 =
        std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
    std::unique_ptr<CopyBufferToBuffer> workload2 =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricStreamerCollector> collector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "EuStallSampling");
    collector->setMaxRequestRawReportCount(1000);

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(collector.get());
    testRunner->addWorkload(workload1.get());
    testRunner->addWorkload(workload2.get());

    return testRunner->run();
}

int main(int argc, char *argv[]) {

    printf("Zello metrics\n");
    fflush(stdout);

    std::map<std::string, std::function<bool(int argc, char *argv[])>> tests;

    tests["query_device_root"] = query_device_root;
    tests["query_device_0_sub_device_0"] = query_device_0_sub_device_0;
    tests["query_device_0_sub_device_1"] = query_device_0_sub_device_1;
    tests["query_device_0_sub_device_0_1"] = query_device_0_sub_device_0_1;
    tests["query_device_1_sub_device_0"] = query_device_1_sub_device_0;
    tests["query_device_1_sub_device_1"] = query_device_1_sub_device_1;
    tests["query_device_0_1_sub_device_0"] = query_device_0_1_sub_device_0;

    tests["stream_device_root"] = stream_device_root;
    tests["stream_device_0_sub_device_0"] = stream_device_0_sub_device_0;
    tests["stream_device_0_sub_device_1"] = stream_device_0_sub_device_1;
    tests["stream_device_0_sub_device_0_1"] = stream_device_0_sub_device_0_1;
    tests["stream_device_1_sub_device_0"] = stream_device_1_sub_device_0;
    tests["stream_device_1_sub_device_1"] = stream_device_1_sub_device_1;
    tests["stream_device_0_1_sub_device_0"] = stream_device_0_1_sub_device_0;
    tests["stream_ip_sampling_device_0_sub_device_0"] = stream_ip_sampling_device_0_sub_device_0;

    // Run test.
    if (argc > 1) {
        if (tests.find(argv[1]) != tests.end()) {
            if (tests[argv[1]](argc, argv) == true) {
                std::cout << argv[1] << " : PASS"
                          << "\n";
            } else {
                std::cout << argv[1] << " : FAIL"
                          << "\n";
            }
            return 0;
        }
    }

    // Print available tests.
    for (auto &test : tests) {
        std::cout << test.first.c_str() << std::endl;
    }

    return 0;
}
