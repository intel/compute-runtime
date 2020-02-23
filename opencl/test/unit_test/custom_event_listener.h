/*
 * Copyright (C) 2017-2020 Intel Corporation
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

std::string lastTest("");

class CCustomEventListener : public ::testing::TestEventListener {
  public:
    CCustomEventListener(::testing::TestEventListener *listener) : hardwarePrefix("---") {
        _listener = listener;
    }

    CCustomEventListener(::testing::TestEventListener *listener, const char *hardwarePrefix) : _listener(listener), hardwarePrefix(hardwarePrefix) {
        std::transform(this->hardwarePrefix.begin(), this->hardwarePrefix.end(), this->hardwarePrefix.begin(),
                       [](unsigned char c) { return std::toupper(c); });
    }

  private:
    void OnTestProgramStart(const ::testing::UnitTest &unit_test) override {
    }

    void OnTestIterationStart(
        const ::testing::UnitTest &unit_test,
        int iteration) override {
        if (::testing::GTEST_FLAG(shuffle)) {
            std::cout << "Iteration: " << iteration + 1 << ". random_seed: " << unit_test.random_seed() << std::endl;
        } else {
            std::cout << "Iteration: " << iteration + 1 << std::endl;
            std::cout << "Iteration: " << iteration + 1 << ". random_seed: " << unit_test.random_seed() << std::endl;
        }
        this->currentSeed = unit_test.random_seed();
    }

    void OnTestIterationEnd(const ::testing::UnitTest &unit_test,
                            int iteration) override {
        this->currentSeed = -1;
    }

    void OnEnvironmentsSetUpStart(const ::testing::UnitTest &unit_test) override {
    }

    void OnEnvironmentsSetUpEnd(const ::testing::UnitTest &unit_test) override {
    }

    void OnTestCaseStart(const ::testing::TestCase &test_case) override {
    }

    void OnTestStart(const ::testing::TestInfo &test_case) override {
        std::stringstream ss;
        ss << test_case.test_case_name() << "." << test_case.name();
        lastTest = ss.str();
    }

    void OnTestPartResult(const ::testing::TestPartResult &test_part_result) override {
        if (test_part_result.failed()) {
            printf("\n");
        }
        _listener->OnTestPartResult(test_part_result);
    }

    void OnTestEnd(const ::testing::TestInfo &test_case) override {
        if (test_case.result()->Failed()) {
            std::stringstream ss;
            ss << test_case.test_case_name() << "." << test_case.name();
            testFailures.push_back(std::make_pair(ss.str(), currentSeed));
            std::cout << "[  FAILED  ][ " << hardwarePrefix << " ][ " << currentSeed << " ] " << test_case.test_case_name() << "." << test_case.name() << std::endl;
        }
    }

    void OnTestCaseEnd(const ::testing::TestCase &test_case) override {
    }

    void OnEnvironmentsTearDownStart(const ::testing::UnitTest &test_case) override {
    }

    void OnEnvironmentsTearDownEnd(const ::testing::UnitTest &test_case) override {
    }

    void OnTestProgramEnd(const ::testing::UnitTest &unit_test) override {
        int testsRun = unit_test.test_to_run_count();
        int testsPassed = unit_test.successful_test_count();
        int testsSkipped = unit_test.skipped_test_count();
        int testsFailed = unit_test.failed_test_count();
        int testsDisabled = unit_test.disabled_test_count();
        auto timeElapsed = static_cast<int>(unit_test.elapsed_time());

        if (unit_test.Failed()) {
            fprintf(
                stdout,
                "\n"
                "=====================\n"
                "==   ULTs FAILED   ==\n"
                "=====================\n");
        } else {
            fprintf(
                stdout,
                "\n"
                "=====================\n"
                "==   ULTs PASSED   ==\n"
                "=====================\n");
        }

        fprintf(
            stdout,
            "Tests run:      %d\n"
            "Tests passed:   %d\n"
            "Tests skipped:  %d\n"
            "Tests failed:   %d\n"
            "Tests disabled: %d\n"
            " Time elapsed:  %d ms\n"
            "=====================\n",
            testsRun,
            testsPassed,
            testsSkipped,
            testsFailed,
            testsDisabled,
            timeElapsed);

        for (auto failure : testFailures)
            fprintf(
                stdout,
                "[  FAILED  ][ %s ][ %u ] %s\n", hardwarePrefix.c_str(), failure.second, failure.first.c_str());
        if (unit_test.Failed())
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
