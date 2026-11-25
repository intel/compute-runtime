/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/stream_capture.h"

#include <string>

namespace NEO {

class StdoutCapturer {
  public:
    StdoutCapturer() {
        capture.captureStdout();
    }

    std::string acquireOutput() {
        return capture.getCapturedStdout();
    }

  private:
    StreamCapture capture;
};

} // namespace NEO
