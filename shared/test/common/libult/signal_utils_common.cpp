/*
 * Copyright (C) 2025 Intel Corporation
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

void handleTestsTimeout(std::string_view testName, uint32_t elapsedTime) {
    printf("Tests timeout in %s %s, after %u seconds on: %s\n", NEO::apiName, NEO::executionName, elapsedTime, testName.data());
    auto xmlGenerator = ::testing::internal::GetUnitTestImpl()->listeners()->default_xml_generator();
    if (xmlGenerator) {
        xmlGenerator->OnTestIterationEnd(*::testing::UnitTest::GetInstance(), ::testing::GTEST_FLAG(repeat));
    }
    abort();
}
