/*
 * Copyright (c) 2018, Intel Corporation
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

#include "segfault_helper.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

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
