/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

/*
 *  This class is a RAII wrapper for GTEST's mechanism, which allows capturing stdout.
 *  We must ensure that GetCapturedStdout() is called after CaptureStdout().
 *  If it is not invoked, then next call to CaptureStdout() will cause abort of the test application.
 */
class StdoutCapturer {
  public:
    StdoutCapturer() {
        ::testing::internal::CaptureStdout();
    }

    ~StdoutCapturer() {
        if (!outputAcquired) {
            ::testing::internal::GetCapturedStdout();
        }
    }

    std::string acquireOutput() {
        outputAcquired = true;
        return ::testing::internal::GetCapturedStdout();
    }

  private:
    bool outputAcquired{false};
};

} // namespace NEO