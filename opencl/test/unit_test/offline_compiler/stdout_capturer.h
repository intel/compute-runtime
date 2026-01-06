/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/stream_capture.h"

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
        capture.captureStdout();
    }

    ~StdoutCapturer() {
        if (!outputAcquired) {
            capture.getCapturedStdout();
        }
    }

    std::string acquireOutput() {
        outputAcquired = true;
        return capture.getCapturedStdout();
    }

  private:
    StreamCapture capture;
    bool outputAcquired{false};
};

} // namespace NEO