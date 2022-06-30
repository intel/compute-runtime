/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "gtest/gtest.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

extern std::string lastTest;
namespace NEO {
extern const char *executionName;
}

class CCustomEventListener : public ::testing::TestEventListener {
  public:
    CCustomEventListener(::testing::TestEventListener *listener) : hardwarePrefix("---") {
        _listener = listener;
    }

    CCustomEventListener(::testing::TestEventListener *listener, const char *hardwarePrefix) : _listener(listener), hardwarePrefix(hardwarePrefix) {
        std::transform(this->hardwarePrefix.begin(), this->hardwarePrefix.end(), this->hardwarePrefix.begin(),
                       [](unsigned char c) { return std::toupper(c); });
    }

    ~CCustomEventListener() override {
        delete _listener;
    }

  private:
    void OnTestProgramStart(const ::testing::UnitTest &unitTest) override {
    }

    void OnTestIterationStart(
        const ::testing::UnitTest &unitTest,
        int iteration) override {
        if (::testing::GTEST_FLAG(shuffle)) {
            std::cout << "Iteration: " << iteration + 1 << ". random_seed: " << unitTest.random_seed() << std::endl;
        } else {
            std::cout << "Iteration: " << iteration + 1 << std::endl;
            std::cout << "Iteration: " << iteration + 1 << ". random_seed: " << unitTest.random_seed() << std::endl;
        }
        this->currentSeed = unitTest.random_seed();
    }

    void OnTestIterationEnd(const ::testing::UnitTest &unitTest,
                            int iteration) override {
        this->currentSeed = -1;
    }

    void OnEnvironmentsSetUpStart(const ::testing::UnitTest &unitTest) override {
    }

    void OnEnvironmentsSetUpEnd(const ::testing::UnitTest &unitTest) override {
    }

    void OnTestCaseStart(const ::testing::TestCase &testCase) override {
    }

    void OnTestStart(const ::testing::TestInfo &testCase) override {
        std::stringstream ss;
        ss << testCase.test_case_name() << "." << testCase.name();
        lastTest = ss.str();
    }

    void OnTestPartResult(const ::testing::TestPartResult &testPartResult) override {
        if (testPartResult.failed()) {
            printf("\n");
        }
        _listener->OnTestPartResult(testPartResult);
    }

    void OnTestEnd(const ::testing::TestInfo &testCase) override {
        if (testCase.result()->Failed()) {
            std::stringstream ss;
            ss << testCase.test_case_name() << "." << testCase.name();
            testFailures.push_back(std::make_pair(ss.str(), currentSeed));
            std::cout << "[  FAILED  ][ " << hardwarePrefix << " ][ " << currentSeed << " ] " << testCase.test_case_name() << "." << testCase.name() << std::endl;
        }
    }

    void OnTestCaseEnd(const ::testing::TestCase &testCase) override {
    }

    void OnEnvironmentsTearDownStart(const ::testing::UnitTest &testCase) override {
    }

    void OnEnvironmentsTearDownEnd(const ::testing::UnitTest &testCase) override {
    }

    void OnTestProgramEnd(const ::testing::UnitTest &unitTest) override {
        int testsRun = unitTest.test_to_run_count();
        int testsPassed = unitTest.successful_test_count();
        int testsSkipped = unitTest.skipped_test_count();
        int testsFailed = unitTest.failed_test_count();
        int testsDisabled = unitTest.disabled_test_count();
        auto timeElapsed = static_cast<int>(unitTest.elapsed_time());
        std::string ultStatus = "PASSED";
        std::string paddingS = "";
        std::string paddingE = "";
        if (unitTest.Failed()) {
            ultStatus = "FAILED";
        }
        auto executionNameLen = strlen(NEO::executionName);

        if (hardwarePrefix != "---") {
            paddingS = std::string(hardwarePrefix.length() + executionNameLen, ' ');
            paddingE = std::string(hardwarePrefix.length() + executionNameLen, '=');

            fprintf(
                stdout,
                "\n"
                "%s==================\n"
                "==  %s %ss %s   ==\n"
                "%s==================\n",
                paddingE.c_str(), hardwarePrefix.c_str(), NEO::executionName, ultStatus.c_str(), paddingE.c_str());
        } else {
            paddingE = std::string(executionNameLen, '=');
            fprintf(
                stdout,
                "\n"
                "%s==================\n"
                "==   %ss %s   ==\n"
                "%s==================\n",
                paddingE.c_str(), NEO::executionName, ultStatus.c_str(), paddingE.c_str());
        }

        fprintf(
            stdout,
            "Tests run:      %d\n"
            "Tests passed:   %d\n"
            "Tests skipped:  %d\n"
            "Tests failed:   %d\n"
            "Tests disabled: %d\n"
            " Time elapsed:  %d ms\n"
            "%s==================\n",
            testsRun,
            testsPassed,
            testsSkipped,
            testsFailed,
            testsDisabled,
            timeElapsed,
            paddingE.c_str());

        for (auto failure : testFailures)
            fprintf(
                stdout,
                "[  FAILED  ][ %s ][ %u ] %s\n", hardwarePrefix.c_str(), failure.second, failure.first.c_str());
        if (unitTest.Failed())
            fprintf(
                stdout,
                "\n");

        fflush(stdout);
    }

    ::testing::TestEventListener *_listener;
    std::vector<std::pair<std::string, int>> testFailures;

    int currentSeed = -1;
    std::string hardwarePrefix;
};
