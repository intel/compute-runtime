/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace zmu = ZelloMetricsUtility;

ZelloMetricsTestList &ZelloMetricsTestList::get() {
    static ZelloMetricsTestList testList;
    return testList;
}

bool ZelloMetricsTestList::add(std::string testName, std::function<bool()> testFunction) {
    auto &tests = getTests();
    tests[testName] = testFunction;
    return true;
}

std::map<std::string, std::function<bool()>> &ZelloMetricsTestList::getTests() {
    static std::map<std::string, std::function<bool()>> tests;
    return tests;
}

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
    for (auto workload : workloadList) {
        workload->validate();
    }

    EXPECT(status == true);
    for (auto collector : collectorList) {
        collector->showResults();
    }

    EXPECT(status == true);
    for (auto collector : collectorList) {
        auto status = collector->isDataAvailable();
        if (!status) {
            LOG(zmu::LogLevel::INFO) << "[W]Event was not generated !!" << std::endl;
        } else {
            LOG(zmu::LogLevel::DEBUG) << "Data Available : " << std::boolalpha << status << std::endl;
        }
    }

    for (auto collector : collectorList) {
        status &= collector->stop();
    }

    EXPECT(status == true);
    status &= executionCtxt->deactivateMetricGroups();

    return status;
}

int main(int argc, char *argv[]) {

    auto &tests = ZelloMetricsTestList::get().getTests();

    auto testSettings = zmu::TestSettings::get();
    testSettings->parseArguments(argc, argv);

    int32_t runStatus = 0;
    if (testSettings->testName == "all") {
        return zmu::osRunAllTests(runStatus);
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
