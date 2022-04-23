/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <algorithm>
#include <atomic>
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
        std::cout << "Warning: Device has insufficient sub-devices" << std::endl;
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
        std::cout << "Warning: Device has insufficient devices" << std::endl;
        return false;
    }

    if (machineConfig.devices[0].subDeviceCount < 2) {
        std::cout << "Warning: Device has insufficient sub-devices" << std::endl;
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
        std::cout << "Warning: Device has insufficient devices" << std::endl;
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
        std::cout << "Warning: Device has insufficient devices" << std::endl;
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
        std::cout << "Warning: Device has insufficient sub-devices" << std::endl;
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
        std::cout << "Warning: Device has insufficient devices" << std::endl;
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

/////////////////////////////////////////////
/// stream_oa_ip_sampling
/////////////////////////////////////////////
bool stream_oa_ip_sampling(int argc, char *argv[]) {

    // This test collects EuStallSampling Metric and TestOa Metric on the same device.

    std::cout << std::endl
              << "-==== Metric stream Capture OA and Ip Sampling: device 0 / sub_device 0 ====-" << std::endl;

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
    executionCtxt->setExecutionTimeInMilliseconds(200);
    std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload1 =
        std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
    std::unique_ptr<CopyBufferToBuffer> workload2 =
        std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
    std::unique_ptr<SingleMetricStreamerCollector> ipSamplingCollector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "EuStallSampling");
    ipSamplingCollector->setMaxRequestRawReportCount(1000);
    std::unique_ptr<SingleMetricStreamerCollector> oaCollector =
        std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "TestOa");

    std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
    testRunner->addCollector(ipSamplingCollector.get());
    testRunner->addCollector(oaCollector.get());
    testRunner->addWorkload(workload1.get());
    testRunner->addWorkload(workload2.get());

    return testRunner->run();
}

///////////////////////////////////////////////////////////
/// stream_mt_ip_sampling_collection_workload_same_thread
///////////////////////////////////////////////////////////
bool stream_mt_ip_sampling_collection_workload_same_thread(int argc, char *argv[]) {

    // This test collects EuStallSampling Metric on sub-devices from different threads
    // Each thread runs the workload and collects for a single sub-device

    constexpr uint32_t threadCount = 2;
    std::cout << std::endl
              << "-==== Multi Thread Metric stream Ip Sampling: device 0 / sub_device 0 & 1 ====-" << std::endl;

    ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
    ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

    if (machineConfig.devices[0].subDeviceCount < threadCount) {
        std::cout << "Warning: Device has insufficient sub-devices" << std::endl;
        return false;
    }

    std::vector<std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt>> executionCtxt(threadCount);
    std::vector<std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost>> workload(threadCount);
    std::vector<std::unique_ptr<SingleMetricStreamerCollector>> collector(threadCount);
    std::vector<std::unique_ptr<SingleDeviceTestRunner>> testRunner(threadCount);

    for (uint32_t i = 0; i < threadCount; i++) {
        executionCtxt[i] = std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, i);
        executionCtxt[i]->setExecutionTimeInMilliseconds(200);
        workload[i] = std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt[i].get());
        collector[i] = std::make_unique<SingleMetricStreamerCollector>(executionCtxt[i].get(), "EuStallSampling");

        testRunner[i] = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt[i].get()));
        testRunner[i]->addCollector(collector[i].get());
        testRunner[i]->addWorkload(workload[i].get());
    }

    std::vector<std::thread> threads;
    std::atomic<bool> status = true;
    for (uint32_t i = 0; i < threadCount; i++) {
        threads.push_back(std::thread([&status, &testRunner, i]() {
            bool runStatus = testRunner[i]->run();
            status = status & static_cast<std::atomic<bool>>(runStatus);
            std::cout << "thread : " << i << " status:" << status << "\n";
        }));
    }

    std::for_each(threads.begin(), threads.end(), [](std::thread &th) {
        th.join();
    });

    return status;
}

///////////////////////////////////////////////////////////////
/// stream_mt_ip_sampling_collection_workload_different_threads
///////////////////////////////////////////////////////////////
bool stream_mt_ip_sampling_collection_workload_different_threads(int argc, char *argv[]) {

    // This test collects EuStallSampling Metric on one thread and runs the workload on another thread

    constexpr uint32_t threadCount = 2;
    std::cout << std::endl
              << "-==== Multi Thread Metric stream Ip Sampling: device 0 / sub_device 0 ====-" << std::endl;
    constexpr uint32_t collectorIndex = 0;
    constexpr uint32_t workloadIndex = 1;

    std::vector<std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt>> executionCtxt(threadCount);
    executionCtxt[collectorIndex] = std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
    executionCtxt[workloadIndex] = std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
    executionCtxt[workloadIndex]->setExecutionTimeInMilliseconds(200);
    std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload =
        std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt[workloadIndex].get());
    std::unique_ptr<SingleMetricStreamerCollector> collector =

        std::make_unique<SingleMetricStreamerCollector>(executionCtxt[collectorIndex].get(), "EuStallSampling");
    collector->setMaxRequestRawReportCount(1000);

    bool status = executionCtxt[collectorIndex]->activateMetricGroups();
    EXPECT(status == true);

    std::vector<std::thread> threads;
    bool collectorStatus = true, workloadStatus = true;

    std::thread collectorThread([&executionCtxt, &collector, &collectorStatus]() {
        collectorStatus &= collector->start();
        EXPECT(collectorStatus == true);
        // sleep while the workload executes
        ZelloMetricsUtility::sleep(200);
        collectorStatus &= collector->isDataAvailable();
        EXPECT(collectorStatus == true);
        collector->showResults();
        collectorStatus &= executionCtxt[collectorIndex]->deactivateMetricGroups();
    });

    std::thread workloadThread([&executionCtxt, &workload, &workloadStatus]() {
        workloadStatus &= workload->appendCommands();
        EXPECT(workloadStatus == true);
        workloadStatus &= executionCtxt[workloadIndex]->run();
        EXPECT(workloadStatus == true);
        workloadStatus &= workload->validate();
    });

    collectorThread.join();
    workloadThread.join();

    status &= workloadStatus & collectorStatus;
    return status;
}

///////////////////////////////////////////////////////////
/// stream_mp_ip_sampling_collection_workload_same_process
///////////////////////////////////////////////////////////
bool stream_mp_ip_sampling_collection_workload_same_process(int argc, char *argv[]) {

    // This test collects EuStallSampling Metric on sub-devices from different processes
    // Each process runs the workload and collects for a single sub-device

    std::cout << std::endl
              << "-==== Multi Process Metric stream Ip Sampling: device 0 / sub_device 0 & 1 ====-" << std::endl;

    constexpr uint32_t processCount = 2;
    bool status = true;

    pid_t pids[processCount];
    for (uint32_t i = 0; i < processCount; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            abort();
        } else if (pids[i] == 0) {
            ZelloMetricsUtility::createL0();
            ZelloMetricsUtility::TestMachineConfiguration machineConfig = {};
            ZelloMetricsUtility::getTestMachineConfiguration(machineConfig);

            if (machineConfig.devices[0].subDeviceCount <= i) {
                std::cout << "Warning: Device has insufficient sub-devices" << std::endl;
                exit(-1);
            }
            std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
                std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, i);
            executionCtxt->setExecutionTimeInMilliseconds(200);
            std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload =
                std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
            std::unique_ptr<SingleMetricStreamerCollector> collector =
                std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "EuStallSampling");

            std::unique_ptr<SingleDeviceTestRunner> testRunner =
                std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
            testRunner->addCollector(collector.get());
            testRunner->addWorkload(workload.get());

            if (testRunner->run()) {
                exit(0);
            } else {
                exit(-1);
            }
        }
    }

    int32_t processExitStatus = -1;
    uint32_t j = processCount;
    while (j-- > 0) {
        wait(&processExitStatus);
        std::cout << "[" << pids[j] << "]: "
                  << "exitStatus : " << processExitStatus << " ; WIFEXITED : "
                  << WIFEXITED(processExitStatus) << " ; WEXITSTATUS: " << WEXITSTATUS(processExitStatus) << "\n";
        status &= WIFEXITED(processExitStatus) && (WEXITSTATUS(processExitStatus) == 0);
    }

    return status;
}

///////////////////////////////////////////////////////////////
/// stream_mp_ip_sampling_collection_workload_different_process
///////////////////////////////////////////////////////////////
bool stream_mp_ip_sampling_collection_workload_different_process(int argc, char *argv[]) {

    // This test collects EuStallSampling Metric on one process and runs the workload on another process

    std::cout << std::endl
              << "-==== Multi Process Metric stream Ip Sampling: device 0 / sub_device 0 ====-" << std::endl;

    constexpr uint32_t processCount = 2;
    bool status = true;

    auto collectorProcess = []() {
        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
        std::unique_ptr<SingleMetricStreamerCollector> collector =
            std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), "EuStallSampling");

        collector->setMaxRequestRawReportCount(1000);
        std::unique_ptr<SingleDeviceTestRunner> testRunner =
            std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(collector.get());
        // Since Streamer collector, no commands in the command list
        testRunner->disableCommandsExecution();
        return testRunner->run();
    };

    auto workloadProcess = []() {
        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, 0);
        executionCtxt->setExecutionTimeInMilliseconds(200);
        std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload =
            std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
        std::unique_ptr<SingleDeviceTestRunner> testRunner =
            std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addWorkload(workload.get());
        return testRunner->run();
    };

    pid_t pids[processCount];
    for (uint32_t i = 0; i < processCount; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            abort();
        } else if (pids[i] == 0) {
            bool status = false;
            if (i == 0) {
                status = collectorProcess();
            } else {
                status = workloadProcess();
            }

            if (status == true) {
                exit(0);
            } else {
                exit(-1);
            }
        }
    }

    int32_t processExitStatus = -1;
    uint32_t j = processCount;
    while (j-- > 0) {
        wait(&processExitStatus);
        std::cout << "[" << pids[j] << "]: "
                  << "exitStatus : " << processExitStatus << " ; WIFEXITED : "
                  << WIFEXITED(processExitStatus) << " ; WEXITSTATUS: " << WEXITSTATUS(processExitStatus) << "\n";
        status &= WIFEXITED(processExitStatus) && (WEXITSTATUS(processExitStatus) == 0);
    }

    return status;
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

    tests["stream_oa_ip_sampling"] = stream_oa_ip_sampling;
    tests["stream_mt_ip_sampling_collection_workload_same_thread"] = stream_mt_ip_sampling_collection_workload_same_thread;
    tests["stream_mt_ip_sampling_collection_workload_different_threads"] = stream_mt_ip_sampling_collection_workload_different_threads;
    tests["stream_mp_ip_sampling_collection_workload_same_process"] = stream_mp_ip_sampling_collection_workload_same_process;
    tests["stream_mp_ip_sampling_collection_workload_different_process"] = stream_mp_ip_sampling_collection_workload_different_process;

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
