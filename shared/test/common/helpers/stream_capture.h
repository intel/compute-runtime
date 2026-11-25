/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"

#include <cstdio>
#include <string>

class StreamCapture {
  public:
    ~StreamCapture();

    void captureStdout() {
        captureStream(stdoutCapture);
    }

    void captureStderr() {
        captureStream(stderrCapture);
    }

    std::string getCapturedStdout() {
        return getCapturedStream(stdoutCapture);
    }

    std::string getCapturedStderr() {
        return getCapturedStream(stderrCapture);
    }

    static constexpr size_t pipeSize = 16 * MemoryConstants::kiloByte;

  private:
    struct Capture {
        FILE *stream = nullptr;
        int readFd = -1;
        int savedFd = -1;
    };

    void captureStream(Capture &capture);
    std::string getCapturedStream(Capture &capture);
    void releaseCapture(Capture &capture);
    void restoreStream(Capture &capture);

    Capture stdoutCapture{stdout};
    Capture stderrCapture{stderr};
};
