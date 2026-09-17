/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/signal_utils.h"
#if defined(_WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#endif

#include "third_party/gtest/src/gtest-internal-inl.h"

#if defined(_WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace NEO {
extern const char *executionName;
extern const char *apiName;
} // namespace NEO

static int64_t iterationStartTimeMs = 0;

void markIterationStart() {
    iterationStartTimeMs = ::testing::internal::GetTimeInMillis();
}

void handleTestsTimeout(std::string_view testName, uint32_t elapsedTime) {
    printf("Tests timeout in %s %s, after %u seconds on: %s\n", NEO::apiName, NEO::executionName, elapsedTime, testName.data());

    auto *unitTest = ::testing::UnitTest::GetInstance();
    const ::testing::TestInfo *currentTest = unitTest->current_test_info();
    if (currentTest != nullptr) {
        auto elapsedMs = ::testing::internal::GetTimeInMillis() - currentTest->result()->start_timestamp();
        printf("Current test case took: %lld ms\n", static_cast<long long>(elapsedMs));
    }

    int totalTests = 0;
    int completedTests = 0;
    for (int suiteIdx = 0; suiteIdx < unitTest->total_test_suite_count(); ++suiteIdx) {
        const ::testing::TestSuite *suite = unitTest->GetTestSuite(suiteIdx);
        for (int testIdx = 0; testIdx < suite->total_test_count(); ++testIdx) {
            const ::testing::TestInfo *testInfo = suite->GetTestInfo(testIdx);
            if (!testInfo->should_run()) {
                continue;
            }
            ++totalTests;
            if (testInfo != currentTest && testInfo->result()->start_timestamp() >= iterationStartTimeMs) {
                ++completedTests;
            }
        }
    }
    printf("Completed test cases: %d / %d\n", completedTests, totalTests);

    auto xmlGenerator = unitTest->listeners().default_xml_generator();
    if (xmlGenerator) {
        xmlGenerator->OnTestIterationEnd(*unitTest, ::testing::GTEST_FLAG(repeat));
    }
    fflush(stdout);
    abort();
}
