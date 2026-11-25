/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/custom_event_listener.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>

namespace NEO {
extern const char *executionName;
extern const char *apiName;
} // namespace NEO

CCustomEventListener::CCustomEventListener(::testing::TestEventListener *listener) : listener(listener), hardwarePrefix("---") {}

CCustomEventListener::~CCustomEventListener() {
    delete listener;
}

void CCustomEventListener::setHwPrefix(const char *hardwarePrefix) {
    this->hardwarePrefix = hardwarePrefix;
    std::transform(this->hardwarePrefix.begin(), this->hardwarePrefix.end(), this->hardwarePrefix.begin(),
                   [](unsigned char c) { return std::toupper(c); });
}

void CCustomEventListener::OnTestIterationStart(const ::testing::UnitTest &unitTest, int iteration) {
    this->currentSeed = unitTest.random_seed();
    std::cout << "Iteration: " << iteration + 1 << ". random_seed: " << this->currentSeed << std::endl;
}

void CCustomEventListener::OnTestIterationEnd(const ::testing::UnitTest &unitTest, int iteration) {
    this->currentSeed = -1;
}

void CCustomEventListener::OnTestPartResult(const ::testing::TestPartResult &testPartResult) {
    if (testPartResult.failed()) {
        printf("\n");
    }
    if (!testPartResult.skipped()) {
        listener->OnTestPartResult(testPartResult);
    }
}

void CCustomEventListener::OnTestEnd(const ::testing::TestInfo &testCase) {
    if (testCase.result()->Failed()) {
        const auto testName = std::string(testCase.test_suite_name()) + "." + testCase.name();
        testFailures.push_back(std::make_pair(testName, currentSeed));
        std::cout << "[  FAILED  ][ " << hardwarePrefix << " ][ " << currentSeed << " ] " << testName << std::endl;
    }
}

void CCustomEventListener::OnTestProgramEnd(const ::testing::UnitTest &unitTest) {
    int testsRun = unitTest.test_to_run_count();
    int testsPassed = unitTest.successful_test_count();
    int testsSkipped = unitTest.skipped_test_count();
    int testsFailed = unitTest.failed_test_count();
    int testsDisabled = unitTest.disabled_test_count();
    auto timeElapsed = static_cast<int>(unitTest.elapsed_time());
    const std::string ultStatus = unitTest.Failed() ? "FAILED" : "PASSED";
    const std::string prefix = hardwarePrefix != "---" ? hardwarePrefix + " " : " ";
    const auto targetNameLen = strlen(NEO::apiName) + 1 + strlen(NEO::executionName);
    const std::string messageBorder(prefix.length() - 1 + targetNameLen, '=');

    fprintf(
        stdout,
        "\n"
        "%s==================\n"
        "==  %s%s %ss %s   ==\n"
        "%s==================\n",
        messageBorder.c_str(), prefix.c_str(), NEO::apiName, NEO::executionName, ultStatus.c_str(), messageBorder.c_str());

    fprintf(
        stdout,
        "Tests run:      %d\n"
        "Tests passed:   %d\n"
        "Tests skipped:  %d\n",
        testsRun,
        testsPassed,
        testsSkipped);
    if (testsFailed > 0) {
        fprintf(
            stdout,
            "Tests failed:   %d\n",
            testsFailed);
    }

    fprintf(
        stdout,
        "Tests disabled: %d\n"
        " Time elapsed:  %d ms\n"
        "%s==================\n",
        testsDisabled,
        timeElapsed,
        messageBorder.c_str());

    for (const auto &failure : testFailures) {
        fprintf(
            stdout,
            "[  FAILED  ][ %s ][ %d ] %s\n", hardwarePrefix.c_str(), failure.second, failure.first.c_str());
    }
    if (unitTest.Failed()) {
        fprintf(
            stdout,
            "\n");
    }

    fflush(stdout);
}
