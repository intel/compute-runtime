/*
 * Copyright (C) 2022-2023 Intel Corporation
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

namespace zmu = ZelloMetricsUtility;

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
        LOG(zmu::LogLevel::DEBUG) << "Data Available : " << std::boolalpha << status << std::endl;
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

//////////////
/// query_test
//////////////
bool queryTest() {
    // This test verifies Query mode for a metric group for all devices OR specific device

    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto queryRun = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Running Query Test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
        std::unique_ptr<CopyBufferToBuffer> workload =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<SingleMetricQueryCollector> collector =
            std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), metricGroupName.c_str());

        std::unique_ptr<SingleDeviceTestRunner> testRunner =
            std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(collector.get());
        testRunner->addWorkload(workload.get());

        return testRunner->run();
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= queryRun(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= queryRun(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= queryRun(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

///////////////
/// stream_test
///////////////
bool streamTest() {

    // This test verifies Streamer mode for a metric group for all devices OR specific device

    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto testSettings = zmu::TestSettings::get();

    auto streamRun = [&testSettings](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Running Stream Test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
        executionCtxt->setExecutionTimeInMilliseconds(200);

        std::unique_ptr<Power> power = std::make_unique<Power>(executionCtxt->getDeviceHandle(0));
        std::unique_ptr<Frequency> frequency = std::make_unique<Frequency>(executionCtxt->getDeviceHandle(0));

        if (testSettings->showSystemInfo.get()) {
            executionCtxt->addSystemParameterCapture(power.get());
            executionCtxt->addSystemParameterCapture(frequency.get());
        }
        std::unique_ptr<CopyBufferToBuffer> workload1 =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload2 =
            std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
        std::unique_ptr<SingleMetricStreamerCollector> collector =
            std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroupName.c_str());
        auto testSettings = zmu::TestSettings::get();
        collector->setNotifyReportCount(testSettings->eventNReportCount.get());

        std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(collector.get());
        testRunner->addWorkload(workload1.get());
        testRunner->addWorkload(workload2.get());

        bool runStatus = testRunner->run();

        if (testSettings->showSystemInfo.get()) {
            power->showAll(2.0);
            frequency->showAll(2.0);
        }

        return runStatus;
    };

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= streamRun(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= streamRun(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= streamRun(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

////////////////////////////////////
/// stream_multi_metric_domain_test
////////////////////////////////////
bool streamMultiMetricDomainTest() {

    // This test validates that metric groups of different domains can be captured concurrently

    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto streamMultiMetricDomainRun = [](uint32_t deviceId, int32_t subDeviceId) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Running Multi Metric Domain Test : Device [" << deviceId << ", " << subDeviceId << " ]\n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
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

        std::unique_ptr<SingleDeviceTestRunner> testRunner =
            std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        testRunner->addCollector(ipSamplingCollector.get());
        testRunner->addCollector(oaCollector.get());
        testRunner->addWorkload(workload1.get());
        testRunner->addWorkload(workload2.get());

        return testRunner->run();
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= streamMultiMetricDomainRun(deviceId, subDeviceId);
            }
            // Run for root device
            status &= streamMultiMetricDomainRun(deviceId, -1);
        }
    } else {
        // Run for specific device
        status &= streamMultiMetricDomainRun(testSettings->deviceId.get(), testSettings->subDeviceId.get());
    }

    return status;
}

//////////////////////////////////////////////
/// stream_mt_collection_workload_same_thread
//////////////////////////////////////////////
bool streamMtCollectionWorkloadSameThread() {

    // This test collects Metrics on sub-devices from different threads
    // Each thread runs the workload and collects for a single sub-device

    // This test collects EuStallSampling Metric on sub-devices from different threads
    // Each thread runs the workload and collects for a single sub-device

    constexpr uint32_t threadCount = 2;
    std::string metricGroupName = zmu::TestSettings::get()->metricGroupName.get();
    LOG(zmu::LogLevel::INFO) << std::endl
                             << "Multi Thread Metric stream : device 0 / sub_device 0 & 1; MetricGroup : "
                             << metricGroupName.c_str() << std::endl;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    if (machineConfig.devices[0].subDeviceCount < threadCount) {
        LOG(zmu::LogLevel::WARNING) << "Warning: Device has insufficient sub-devices" << std::endl;
        return false;
    }

    std::vector<std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt>> executionCtxt(threadCount);
    std::vector<std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost>> workload(threadCount);
    std::vector<std::unique_ptr<SingleMetricStreamerCollector>> collector(threadCount);
    std::vector<std::unique_ptr<SingleDeviceTestRunner>> testRunner(threadCount);

    for (uint32_t i = 0; i < threadCount; i++) {
        if (!zmu::isDeviceAvailable(0, i)) {
            return false;
        }
        executionCtxt[i] = std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, i);
        executionCtxt[i]->setExecutionTimeInMilliseconds(200);
        workload[i] = std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt[i].get());
        collector[i] = std::make_unique<SingleMetricStreamerCollector>(executionCtxt[i].get(), metricGroupName.c_str());

        testRunner[i] = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt[i].get()));
        testRunner[i]->addCollector(collector[i].get());
        testRunner[i]->addWorkload(workload[i].get());
    }

    std::vector<std::thread> threads;
    std::atomic<bool> status = true;
    for (uint32_t i = 0; i < threadCount; i++) {
        threads.push_back(std::thread([&status, &testRunner, i]() {
            bool runStatus = testRunner[i]->run();
            status = status && static_cast<std::atomic<bool>>(runStatus);
            if (status == false) {
                LOG(zmu::LogLevel::DEBUG) << "thread : " << i << " status:" << status << "\n";
            }
        }));
    }

    std::for_each(threads.begin(), threads.end(), [](std::thread &th) {
        th.join();
    });

    return status;
}

////////////////////////////////////////////////////
/// stream_mt_collection_workload_different_threads
////////////////////////////////////////////////////
bool streamMtCollectionWorkloadDifferentThreads() {

    // This test collects Metrics on one device from different threads
    // One thread collects metrics and other runs the workload

    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto streamMt = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        constexpr uint32_t threadCount = 2;
        constexpr uint32_t collectorIndex = 0;
        constexpr uint32_t workloadIndex = 1;

        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Running Multi thread Stream Test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";

        std::vector<std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt>> executionCtxt(threadCount);
        executionCtxt[collectorIndex] = std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
        executionCtxt[workloadIndex] = std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
        executionCtxt[workloadIndex]->setExecutionTimeInMilliseconds(200);
        std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload =
            std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt[workloadIndex].get());
        std::unique_ptr<SingleMetricStreamerCollector> collector =
            std::make_unique<SingleMetricStreamerCollector>(executionCtxt[collectorIndex].get(), metricGroupName.c_str());
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
            if (zmu::TestSettings::get()->verboseLevel.get() >= zmu::LogLevel::DEBUG) {
                collector->showResults();
            }
            collectorStatus &= collector->stop();
            collectorStatus &= executionCtxt[collectorIndex]->deactivateMetricGroups();
        });

        std::thread workloadThread([&executionCtxt, &workload, &workloadStatus]() {
            workloadStatus &= workload->appendCommands();
            EXPECT(workloadStatus == true);
            workloadStatus &= executionCtxt[workloadIndex]->run();
            EXPECT(workloadStatus == true);
            workloadStatus &= workload->validate();
        });

        workloadThread.join();
        collectorThread.join();

        status = status && workloadStatus && collectorStatus;
        return status;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= streamMt(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= streamMt(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= streamMt(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

//////////////////////////////////////////////
/// stream_mp_collection_workload_same_process
//////////////////////////////////////////////
bool streamMpCollectionWorkloadSameProcess() {

    // This test collects Metric on devices from different processes
    // Each process runs the workload and collects for a single device

    std::string metricGroupName = zmu::TestSettings::get()->metricGroupName.get();
    LOG(zmu::LogLevel::INFO) << std::endl
                             << "Multi Process Metric stream: device 0 / sub_device 0 & 1 ; MetricGroup : "
                             << metricGroupName.c_str() << std::endl;

    constexpr uint32_t processCount = 2;
    bool status = true;

    pid_t pids[processCount];
    for (uint32_t i = 0; i < processCount; ++i) {
        pids[i] = fork();
        if (pids[i] < 0) {
            abort();
        } else if (pids[i] == 0) {
            zmu::createL0();

            if (!zmu::isDeviceAvailable(0, i)) {
                exit(-1);
            }

            std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
                std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, i);
            executionCtxt->setExecutionTimeInMilliseconds(200);
            std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload =
                std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
            std::unique_ptr<SingleMetricStreamerCollector> collector =
                std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroupName.c_str());

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
        status &= WIFEXITED(processExitStatus) && (WEXITSTATUS(processExitStatus) == 0);

        if (status == false) {
            LOG(zmu::LogLevel::DEBUG) << "[" << pids[j] << "]: "
                                      << "exitStatus : " << processExitStatus << " ; WIFEXITED : "
                                      << WIFEXITED(processExitStatus) << " ; WEXITSTATUS: " << WEXITSTATUS(processExitStatus) << "\n";
        }
    }

    return status;
}

////////////////////////////////////////////////////
/// stream_mp_collection_workload_different_process
////////////////////////////////////////////////////
bool streamMpCollectionWorkloadDifferentProcess() {

    // This test collects EuStallSampling Metric on one process and runs the workload on another process

    // This test collects Metrics on one device from different processes
    // One process collects metrics and other runs the workload

    bool status = true;
    auto streamMp = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        LOG(zmu::LogLevel::INFO) << "Running Multi Process Stream Test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";
        constexpr uint32_t processCount = 2;
        bool status = true;

        auto collectorProcess = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
            zmu::createL0();
            if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
                return false;
            }
            std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
                std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
            std::unique_ptr<SingleMetricStreamerCollector> collector =
                std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroupName.c_str());

            collector->setMaxRequestRawReportCount(1000);
            std::unique_ptr<SingleDeviceTestRunner> testRunner =
                std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
            testRunner->addCollector(collector.get());
            // Since Streamer collector, no commands in the command list
            testRunner->disableCommandsExecution();
            return testRunner->run();
        };

        auto workloadProcess = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
            zmu::createL0();
            if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
                return false;
            }
            std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
                std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
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
                    status = collectorProcess(deviceId, subDeviceId, metricGroupName);
                } else {
                    status = workloadProcess(deviceId, subDeviceId, metricGroupName);
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
            status &= WIFEXITED(processExitStatus) && (WEXITSTATUS(processExitStatus) == 0);

            if (status == false) {
                LOG(zmu::LogLevel::DEBUG) << "[" << pids[j] << "]: "
                                          << "exitStatus : " << processExitStatus << " ; WIFEXITED : "
                                          << WIFEXITED(processExitStatus) << " ; WEXITSTATUS: " << WEXITSTATUS(processExitStatus) << "\n";
            }
        }

        return status;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        status &= streamMp(0, 0, testSettings->metricGroupName.get());
    } else {
        // Run for specific device
        status &= streamMp(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

bool streamPowerFrequencyTest() {

    // This test verifies Power and Frequency usage in Streamer mode
    // Requires ZES_ENABLE_SYSMAN=1

    bool status = true;
    const uint32_t deviceId = 0;
    const int32_t subDeviceId = -1;
    const std::string metricGroupName("ComputeBasic");

    LOG(zmu::LogLevel::DEBUG) << "Running Stream Power Frequency Test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";

    auto streamRun = [&deviceId, &subDeviceId, &metricGroupName](bool workloadEnable, bool streamerEnable,
                                                                 std::pair<double, double> &powerMinMax,
                                                                 std::pair<double, double> &freqMinMax) {
        if (!zmu::isDeviceAvailable(0, -1)) {
            return false;
        }

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
            std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
        executionCtxt->setExecutionTimeInMilliseconds(300);

        std::unique_ptr<Power> power = std::make_unique<Power>(executionCtxt->getDeviceHandle(0));
        std::unique_ptr<Frequency> frequency = std::make_unique<Frequency>(executionCtxt->getDeviceHandle(0));
        executionCtxt->addSystemParameterCapture(power.get());
        executionCtxt->addSystemParameterCapture(frequency.get());

        std::unique_ptr<SingleMetricStreamerCollector> collector =
            std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroupName.c_str());
        std::unique_ptr<CopyBufferToBuffer> workload1 =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<AppendMemoryCopyFromHeapToDeviceAndBackToHost> workload2 =
            std::make_unique<AppendMemoryCopyFromHeapToDeviceAndBackToHost>(executionCtxt.get());
        auto testSettings = zmu::TestSettings::get();
        collector->setNotifyReportCount(testSettings->eventNReportCount.get());

        std::unique_ptr<SingleDeviceTestRunner> testRunner = std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));
        if (streamerEnable == true) {
            testRunner->addCollector(collector.get());
        }
        if (workloadEnable == true) {
            testRunner->addWorkload(workload1.get());
            testRunner->addWorkload(workload2.get());
        }
        bool runStatus = testRunner->run();

        power->getMinMax(powerMinMax.first, powerMinMax.second);
        frequency->getMinMax(freqMinMax.first, freqMinMax.second);
        return runStatus;
    };

    // Get power and frequency with workload and metrics

    uint32_t sampleCount = 100;

    std::pair<double, double> powerMinMax = {};
    std::pair<double, double> freqMinMax = {};

    double maxPowerWithWorkloadAndMetrics = std::numeric_limits<double>::min();
    double maxFrequencyWithWorkloadAndMetrics = std::numeric_limits<double>::min();

    double avgPowerWithWorkloadAndMetrics = 0.0;

    for (uint32_t index = 0; index < sampleCount; index++) {
        status &= streamRun(true, true, powerMinMax, freqMinMax);

        avgPowerWithWorkloadAndMetrics += powerMinMax.second;

        maxPowerWithWorkloadAndMetrics = std::max(maxPowerWithWorkloadAndMetrics, powerMinMax.second);
        maxFrequencyWithWorkloadAndMetrics = std::max(maxFrequencyWithWorkloadAndMetrics, freqMinMax.second);
    }

    avgPowerWithWorkloadAndMetrics /= sampleCount;

    double maxPowerWithWorkload = std::numeric_limits<double>::min();
    double maxFrequencyWithWorkload = std::numeric_limits<double>::min();

    double avgPowerWithWorkload = 0.0;

    for (uint32_t index = 0; index < sampleCount; index++) {
        status &= streamRun(true, false, powerMinMax, freqMinMax);
        maxPowerWithWorkload = std::max(maxPowerWithWorkload, powerMinMax.second);
        maxFrequencyWithWorkload = std::max(maxFrequencyWithWorkload, freqMinMax.second);
        avgPowerWithWorkload += powerMinMax.second;
    }

    avgPowerWithWorkload /= sampleCount;

    LOG(zmu::LogLevel::INFO) << "Workload + Streamer [ Max Power Used : " << maxPowerWithWorkloadAndMetrics << "W | Frequency : " << maxFrequencyWithWorkloadAndMetrics << "Hz \n";
    LOG(zmu::LogLevel::INFO) << "Workload            [ Max Power Used: " << maxPowerWithWorkload << "W | Frequency : " << maxFrequencyWithWorkload << "Hz \n";
    LOG(zmu::LogLevel::INFO) << "Metrics Overhead    [ Power Used: " << (maxPowerWithWorkloadAndMetrics - maxPowerWithWorkload) << "W | Frequency : "
                             << (maxFrequencyWithWorkloadAndMetrics - maxFrequencyWithWorkload) << "Hz \n";
    LOG(zmu::LogLevel::INFO) << "Workload + Streamer [ Avg Power Used : " << avgPowerWithWorkloadAndMetrics << "\n";
    LOG(zmu::LogLevel::INFO) << "Workload            [ Avg Power Used : " << avgPowerWithWorkload << "\n";
    LOG(zmu::LogLevel::INFO) << "Metrics Overhead    [ Avg Power Used : " << avgPowerWithWorkloadAndMetrics - avgPowerWithWorkload << "\n";

    return status;
}

bool displayAllMetricGroups() {

    uint32_t metricGroupCount = 0;
    std::vector<zet_metric_group_handle_t> metricGroups = {};

    std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt =
        std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(0, -1);

    auto deviceHandle = executionCtxt->getDeviceHandle(0);
    VALIDATECALL(zetMetricGroupGet(deviceHandle, &metricGroupCount, nullptr));
    metricGroups.resize(metricGroupCount);
    VALIDATECALL(zetMetricGroupGet(deviceHandle, &metricGroupCount, metricGroups.data()));

    for (uint32_t i = 0; i < metricGroupCount; ++i) {

        const zet_metric_group_handle_t metricGroupHandle = metricGroups[i];
        zet_metric_group_properties_t metricGroupProperties = {};
        VALIDATECALL(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties));

        LOG(zmu::LogLevel::INFO) << "METRIC GROUP[" << i << "]: "
                                 << "desc: " << metricGroupProperties.description << "\n";
        LOG(zmu::LogLevel::INFO) << "\t -> name: " << metricGroupProperties.name << " | "
                                 << "samplingType: " << metricGroupProperties.samplingType << " | "
                                 << "domain: " << metricGroupProperties.domain << " | "
                                 << "metricCount: " << metricGroupProperties.metricCount << std::endl;

        uint32_t metricCount = 0;
        std::vector<zet_metric_handle_t> metrics = {};
        VALIDATECALL(zetMetricGet(metricGroupHandle, &metricCount, nullptr));
        metrics.resize(metricCount);
        VALIDATECALL(zetMetricGet(metricGroupHandle, &metricCount, metrics.data()));

        for (uint32_t j = 0; j < metricCount; ++j) {
            const zet_metric_handle_t metric = metrics[j];
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
    }

    return true;
}

//////////////////////////////////////////
/// queryImmediateCommandListTest
//////////////////////////////////////////
bool queryImmediateCommandListTest() {
    // This test verifies Query mode for a metric group for all devices OR specific device
    // using an immediate command list

    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto queryRun = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Running query_immediate_command_list_test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";

        std::unique_ptr<SingleDeviceImmediateCommandListCtxt> executionCtxt =
            std::make_unique<SingleDeviceImmediateCommandListCtxt>(deviceId, subDeviceId);
        std::unique_ptr<CopyBufferToBuffer> workload =
            std::make_unique<CopyBufferToBuffer>(executionCtxt.get());
        std::unique_ptr<SingleMetricQueryCollector> collector =
            std::make_unique<SingleMetricQueryCollector>(executionCtxt.get(), metricGroupName.c_str());
        std::unique_ptr<SingleDeviceTestRunner> testRunner =
            std::make_unique<SingleDeviceTestRunner>(static_cast<ExecutionContext *>(executionCtxt.get()));

        const bool sleepBetweenCommands = false;

        {
            bool status = true;
            status &= executionCtxt->activateMetricGroups();
            EXPECT(status == true);
            status &= collector->start();
            EXPECT(status == true);
            status &= collector->prefixCommands();
            EXPECT(status == true);

            uint32_t appendCount = 4096;
            while (appendCount--) {
                status &= workload->appendCommands();
                EXPECT(status == true);
                if (sleepBetweenCommands) {
                    zmu::sleep(1);
                }
            }
            status &= collector->suffixCommands();
            EXPECT(status == true);
            status &= collector->isDataAvailable();
            LOG(zmu::LogLevel::DEBUG) << "Data Available : " << std::boolalpha << status << std::endl;
            EXPECT(status == true);
            status &= workload->validate();
            collector->showResults();
            status &= collector->stop();
            EXPECT(status == true);
            status &= executionCtxt->deactivateMetricGroups();
            return status;
        }
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        for (uint32_t deviceId = 0; deviceId < machineConfig.deviceCount; deviceId++) {
            // Run for all subdevices
            for (uint32_t subDeviceId = 0; subDeviceId < machineConfig.devices[deviceId].subDeviceCount; subDeviceId++) {
                status &= queryRun(deviceId, subDeviceId, testSettings->metricGroupName.get());
            }
            // Run for root device
            status &= queryRun(deviceId, -1, testSettings->metricGroupName.get());
        }
    } else {
        // Run for specific device
        status &= queryRun(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

////////////////////////////////////////////////////
/// collectIndefinitely
////////////////////////////////////////////////////
bool collectIndefinitely() {

    // This test collects Metrics on one device indefinitely

    bool status = true;

    zmu::TestMachineConfiguration machineConfig = {};
    zmu::getTestMachineConfiguration(machineConfig);

    auto collectStart = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        if (!zmu::isDeviceAvailable(deviceId, subDeviceId)) {
            return false;
        }

        LOG(zmu::LogLevel::INFO) << "Running collectIndefinitely Test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";

        std::unique_ptr<SingleDeviceSingleQueueExecutionCtxt> executionCtxt;
        executionCtxt = std::make_unique<SingleDeviceSingleQueueExecutionCtxt>(deviceId, subDeviceId);
        std::unique_ptr<SingleMetricStreamerCollector> collector =
            std::make_unique<SingleMetricStreamerCollector>(executionCtxt.get(), metricGroupName.c_str());
        bool status = executionCtxt->activateMetricGroups();
        EXPECT(status == true);
        bool collectorStatus = true;
        collectorStatus &= collector->start();

        const uint32_t runTimeInMinutes = 1;

        // Running for approximately 1 minutes
        for (uint32_t i = 0; i < (10 * 60 * runTimeInMinutes); i++) {
            EXPECT(collectorStatus == true);
            ZelloMetricsUtility::sleep(100);
            collectorStatus &= collector->isDataAvailable();
            if (collectorStatus == true) {
                LOG(zmu::LogLevel::INFO) << "Data Available : " << (collectorStatus == true) << "\n";
                collector->showResults();
            }
        }

        collectorStatus &= collector->stop();
        collectorStatus &= executionCtxt->deactivateMetricGroups();

        status = status && collectorStatus;
        return status;
    };

    auto testSettings = zmu::TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        testSettings->deviceId.set(0);
    }
    // Run for specific device
    status &= collectStart(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());

    return status;
}

int main(int argc, char *argv[]) {

    std::map<std::string, std::function<bool()>> tests;

    tests["query_test"] = queryTest;
    tests["stream_test"] = streamTest;
    tests["stream_multi_metric_domain_test"] = streamMultiMetricDomainTest;
    tests["stream_mt_collection_workload_same_thread"] = streamMtCollectionWorkloadSameThread;
    tests["stream_mt_collection_workload_different_threads"] = streamMtCollectionWorkloadDifferentThreads;
    tests["stream_mp_collection_workload_same_process"] = streamMpCollectionWorkloadSameProcess;
    tests["stream_mp_collection_workload_different_process"] = streamMpCollectionWorkloadDifferentProcess;
    tests["streamPowerFrequencyTest"] = streamPowerFrequencyTest;
    tests["displayAllMetricGroups"] = displayAllMetricGroups;
    tests["queryImmediateCommandListTest"] = queryImmediateCommandListTest;
    tests["collectIndefinitely"] = collectIndefinitely;

    auto testSettings = zmu::TestSettings::get();
    testSettings->parseArguments(argc, argv);

    int32_t runStatus = 0;
    if (testSettings->testName == "all") {
        // Run all tests
        for (auto const &[testName, testFn] : tests) {
            LOG(zmu::LogLevel::INFO) << "\n== Start " << testName << " == \n";
            // Run each test in a new process
            pid_t pid = fork();
            if (pid == 0) {
                int32_t status = 0;
                if (testFn() == true) {
                    LOG(zmu::LogLevel::INFO) << testName << " : PASS\n";
                } else {
                    LOG(zmu::LogLevel::ERROR) << testName << " : FAIL \n";
                    status = 1;
                }
                exit(status);
            }

            int32_t testStatus = 0;
            // Wait for the process to complete
            waitpid(pid, &testStatus, 0);
            LOG(zmu::LogLevel::INFO) << "\n== End " << testName << " == \n";
            if (WIFEXITED(testStatus) != true) {
                runStatus = 1;
            } else {
                runStatus += WEXITSTATUS(testStatus);
            }
        }
        return std::min(1, runStatus);
    }

    // Run test.
    if (tests.find(testSettings->testName) != tests.end()) {
        if (tests[testSettings->testName]() == true) {
            LOG(zmu::LogLevel::INFO) << testSettings->testName << " : PASS\n";
            runStatus = 0;
        } else {
            LOG(zmu::LogLevel::ERROR) << testSettings->testName << " : FAIL \n";
            runStatus = 1;
        }
        return runStatus;
    } else {
        // Unsupported test was passed as argument
        LOG(zmu::LogLevel::WARNING) << "Warning:Test " << testSettings->testName << " not available ..\n";
    }

    // Print available tests.
    LOG(zmu::LogLevel::INFO) << "\n==Supported Test List ==\n\n";
    for (auto &test : tests) {
        LOG(zmu::LogLevel::INFO) << test.first.c_str() << std::endl;
    }
    return 0;
}
