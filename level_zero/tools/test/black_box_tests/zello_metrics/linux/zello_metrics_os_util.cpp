/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <memory>
#include <sys/wait.h>
#include <unistd.h>

namespace ZelloMetricsUtility {

bool osStreamMpCollectionWorkloadDifferentProcess() {
    bool status = true;
    auto streamMp = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
        LOG(LogLevel::INFO) << "Running Multi Process Stream Test : Device [" << deviceId << ", " << subDeviceId << " ] : Metric Group :" << metricGroupName.c_str() << "\n";
        constexpr uint32_t processCount = 2;
        bool status = true;

        auto collectorProcess = [](uint32_t deviceId, int32_t subDeviceId, std::string &metricGroupName) {
            createL0();
            if (!isDeviceAvailable(deviceId, subDeviceId)) {
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
            createL0();
            if (!isDeviceAvailable(deviceId, subDeviceId)) {
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
                LOG(LogLevel::DEBUG) << "[" << pids[j] << "]: "
                                     << "exitStatus : " << processExitStatus << " ; WIFEXITED : "
                                     << WIFEXITED(processExitStatus) << " ; WEXITSTATUS: " << WEXITSTATUS(processExitStatus) << "\n";
            }
        }

        return status;
    };

    auto testSettings = TestSettings::get();

    if (testSettings->deviceId.get() == -1) {
        status &= streamMp(0, 0, testSettings->metricGroupName.get());
    } else {
        // Run for specific device
        status &= streamMp(testSettings->deviceId.get(), testSettings->subDeviceId.get(), testSettings->metricGroupName.get());
    }

    return status;
}

bool osStreamMpCollectionWorkloadSameProcess() {
    std::string metricGroupName = TestSettings::get()->metricGroupName.get();
    LOG(LogLevel::INFO) << std::endl
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
            createL0();

            if (!isDeviceAvailable(0, i)) {
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
            LOG(LogLevel::DEBUG) << "[" << pids[j] << "]: "
                                 << "exitStatus : " << processExitStatus << " ; WIFEXITED : "
                                 << WIFEXITED(processExitStatus) << " ; WEXITSTATUS: " << WEXITSTATUS(processExitStatus) << "\n";
        }
    }

    return status;
}

int32_t osRunAllTests(int32_t runStatus) {
    auto &tests = ZelloMetricsTestList::get().getTests();
    for (auto const &[testName, testFn] : tests) {
        LOG(LogLevel::INFO) << "\n== Start " << testName << " == \n";
        // Run each test in a new process
        pid_t pid = fork();
        if (pid == 0) {
            int32_t status = 0;
            if (testFn() == true) {
                LOG(LogLevel::INFO) << testName << " : PASS\n";
            } else {
                LOG(LogLevel::ERROR) << testName << " : FAIL \n";
                status = 1;
            }
            exit(status);
        }

        int32_t testStatus = 0;
        // Wait for the process to complete
        waitpid(pid, &testStatus, 0);
        LOG(LogLevel::INFO) << "\n== End " << testName << " == \n";
        if (WIFEXITED(testStatus) != true) {
            runStatus = 1;
        } else {
            runStatus += WEXITSTATUS(testStatus);
        }
    }
    return std::min(1, runStatus);
}

} // namespace ZelloMetricsUtility
