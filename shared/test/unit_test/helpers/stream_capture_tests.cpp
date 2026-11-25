/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/stream_capture.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <string>

constexpr char outputWithNullCharacter[] = "ab\0cd";

TEST(StreamCaptureTest, givenActiveCaptureWhenItIsDestroyedWithoutGettingOutputThenStdoutIsRestored) {
    StreamCapture outerCapture;
    outerCapture.captureStdout();
    {
        StreamCapture innerCapture;
        innerCapture.captureStdout();
        printf("inner");
    }
    printf("outer");
    EXPECT_EQ("outer", outerCapture.getCapturedStdout());
}

TEST(StreamCaptureTest, givenOutputWithNullCharacterWhenGettingCapturedStdoutThenWholeOutputIsReturned) {
    const std::string output(outputWithNullCharacter, sizeof(outputWithNullCharacter) - 1);
    StreamCapture capture;
    capture.captureStdout();
    fwrite(output.data(), 1, output.size(), stdout);
    EXPECT_EQ(output, capture.getCapturedStdout());
}

TEST(StreamCaptureTest, givenNoActiveCaptureWhenGettingCapturedOutputThenEmptyStringIsReturned) {
    StreamCapture capture;
    EXPECT_EQ("", capture.getCapturedStdout());
    EXPECT_EQ("", capture.getCapturedStderr());

    capture.captureStdout();
    printf("output");
    EXPECT_EQ("output", capture.getCapturedStdout());
    EXPECT_EQ("", capture.getCapturedStdout());
}

TEST(StreamCaptureTest, givenOutputAlreadyGottenWhenCapturingAgainThenNewOutputIsCaptured) {
    StreamCapture capture;
    capture.captureStdout();
    printf("first");
    EXPECT_EQ("first", capture.getCapturedStdout());

    capture.captureStdout();
    printf("second");
    EXPECT_EQ("second", capture.getCapturedStdout());
}

TEST(StreamCaptureTest, givenStdoutAndStderrCapturedWhenWritingToBothThenEachOutputIsCapturedSeparately) {
    StreamCapture capture;
    capture.captureStdout();
    capture.captureStderr();
    printf("to stdout");
    fprintf(stderr, "to stderr");
    EXPECT_EQ("to stderr", capture.getCapturedStderr());
    EXPECT_EQ("to stdout", capture.getCapturedStdout());
}
