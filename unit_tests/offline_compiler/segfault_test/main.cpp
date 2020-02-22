/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "segfault_helper.h"

#include <string>

using namespace std;

extern void generateSegfaultWithSafetyGuard(SegfaultHelper *segfaultHelper);

int main(int argc, char **argv) {
    int retVal = 0;
    ::testing::InitGoogleTest(&argc, argv);

    retVal = RUN_ALL_TESTS();

    return retVal;
}

void captureAndCheckStdOut() {
    string callstack = ::testing::internal::GetCapturedStdout();

    EXPECT_THAT(callstack, ::testing::HasSubstr(string("Callstack")));
    EXPECT_THAT(callstack, ::testing::HasSubstr(string("cloc_segfault_test")));
    EXPECT_THAT(callstack, ::testing::HasSubstr(string("generateSegfaultWithSafetyGuard")));
}

TEST(SegFault, givenCallWithSafetyGuardWhenSegfaultHappensThenCallstackIsPrintedToStdOut) {
#if !defined(SKIP_SEGFAULT_TEST)
    ::testing::internal::CaptureStdout();
    SegfaultHelper segfault;
    segfault.segfaultHandlerCallback = captureAndCheckStdOut;

    generateSegfaultWithSafetyGuard(&segfault);
#endif
}
