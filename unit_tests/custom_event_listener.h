/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "gtest/gtest.h"
#include <vector>
#include <string>
#include <sstream>

std::string lastTest("");

class CCustomEventListener : public ::testing::TestEventListener {
  public:
    CCustomEventListener(::testing::TestEventListener *listener) {
        _listener = listener;
    }

  private:
    virtual void OnTestProgramStart(const ::testing::UnitTest &unit_test) {
    }

    virtual void OnTestIterationStart(
        const ::testing::UnitTest &unit_test,
        int iteration) {
        if (::testing::GTEST_FLAG(shuffle)) {
            std::cout << "Iteration: " << iteration + 1 << ". random_seed: " << unit_test.random_seed() << std::endl;
        } else {
            std::cout << "Iteration: " << iteration + 1 << std::endl;
        }
    }

    virtual void OnEnvironmentsSetUpStart(const ::testing::UnitTest &unit_test) {
    }

    virtual void OnEnvironmentsSetUpEnd(const ::testing::UnitTest &unit_test) {
    }

    virtual void OnTestCaseStart(const ::testing::TestCase &test_case) {
    }

    virtual void OnTestStart(const ::testing::TestInfo &test_case) {
        std::stringstream ss;
        ss << test_case.test_case_name() << "." << test_case.name();
        lastTest = ss.str();
    }

    virtual void OnTestPartResult(const ::testing::TestPartResult &test_part_result) {
        printf("\n");
        _listener->OnTestPartResult(test_part_result);
    }

    virtual void OnTestEnd(const ::testing::TestInfo &test_case) {
        if (test_case.result()->Passed())
            return;

        std::stringstream ss;
        ss << test_case.test_case_name() << "." << test_case.name();
        testFailures.push_back(ss.str());
        std::cout << "[  FAILED  ] " << test_case.test_case_name() << "." << test_case.name() << std::endl;
    }

    virtual void OnTestCaseEnd(const ::testing::TestCase &test_case) {
    }

    virtual void OnEnvironmentsTearDownStart(const ::testing::UnitTest &test_case) {
    }

    virtual void OnEnvironmentsTearDownEnd(const ::testing::UnitTest &test_case) {
    }

    virtual void OnTestIterationEnd(
        const ::testing::UnitTest &test_case,
        int iteration) {
    }

    virtual void OnTestProgramEnd(const ::testing::UnitTest &unit_test) {
        int testsRun = unit_test.test_to_run_count();
        int testsPassed = unit_test.successful_test_count();
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
            "Tests failed:   %d\n"
            "Tests disabled: %d\n"
            " Time elapsed:  %d ms\n"
            "=====================\n",
            testsRun,
            testsPassed,
            testsFailed,
            testsDisabled,
            timeElapsed);

        for (auto str : testFailures)
            fprintf(
                stdout,
                "[  FAILED  ] %s\n", str.c_str());
        if (unit_test.Failed())
            fprintf(
                stdout,
                "\n");

        fflush(stdout);
    }

    ::testing::TestEventListener *_listener;
    std::vector<std::string> testFailures;
};
