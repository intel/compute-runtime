/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/ult_output_guard.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

class UltOutputGuardListener : public ::testing::EmptyTestEventListener {
  public:
    UltOutputGuardListener(bool enabled, bool allowOutput) : enabled(enabled && readEnvFlag("NEO_ULT_OUTPUT_GUARD", true)),
                                                             allowOutput(allowOutput || readEnvFlag("NEO_ULT_ALLOW_OUTPUT", false)) {}

  private:
    static bool readEnvFlag(const char *name, bool defaultValue) {
        auto value = getenv(name);
        return value != nullptr ? strcmp(value, "0") != 0 : defaultValue;
    }

    void OnTestProgramStart(const ::testing::UnitTest &unitTest) override {
        if (this->enabled) {
            UltOutputGuard::start();
        }
    }

    void OnTestIterationStart(const ::testing::UnitTest &unitTest, int iteration) override {
        UltOutputGuard::drain();
        UltOutputGuard::capturedBytes = 0;
        this->bytesPerTest.clear();
        this->currentTest = nullptr;
    }

    void OnTestStart(const ::testing::TestInfo &testInfo) override {
        this->collectBytes();
        this->currentTest = &testInfo;
    }

    void OnTestEnd(const ::testing::TestInfo &testInfo) override {
        this->collectBytes();
        this->currentTest = nullptr;
    }

    void OnTestIterationEnd(const ::testing::UnitTest &unitTest, int iteration) override {
        this->collectBytes();
        if (this->bytesPerTest.empty() || unitTest.failed_test_count() > 0) {
            return;
        }
        std::ostringstream report;
        report << "tests printed outside of StreamCapture in iteration " << iteration + 1 << " (random_seed: " << unitTest.random_seed() << "):\n";
        for (const auto &[testName, bytes] : this->bytesPerTest) {
            report << "  " << testName << " (" << bytes << " bytes)\n";
        }
        if (this->allowOutput) {
            fprintf(stdout, "WARNING: %scheck suppressed by NEO_ULT_ALLOW_OUTPUT / --allow_output\n", report.str().c_str());
            fflush(stdout);
        } else {
            ADD_FAILURE() << report.str() << "set NEO_ULT_ALLOW_OUTPUT=1 or pass --allow_output to allow output (e.g. for debug logging)";
        }
    }

    void OnTestProgramEnd(const ::testing::UnitTest &unitTest) override {
        UltOutputGuard::stop();
    }

    void collectBytes() {
        UltOutputGuard::drain();
        auto bytes = UltOutputGuard::capturedBytes.exchange(0);
        if (bytes == 0) {
            return;
        }
        auto testName = this->currentTest ? std::string(this->currentTest->test_suite_name()) + "." + this->currentTest->name() : std::string("<outside of tests>");
        this->bytesPerTest[testName] += bytes;
    }

    std::map<std::string, size_t> bytesPerTest;
    const ::testing::TestInfo *currentTest = nullptr;
    bool enabled;
    bool allowOutput;
};
