/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/stream_capture.h"

#include "shared/test/common/helpers/stream_capture_os.h"

#include "gtest/gtest.h"

#include <iostream>

using namespace StreamCaptureOs;

namespace {
std::string readAll(int fd) {
    std::string captured;
    char buffer[StreamCapture::pipeSize];
    int count = 0;
    while ((count = readFromFd(fd, buffer, sizeof(buffer))) > 0) {
        captured.append(buffer, count);
    }
    return captured;
}
} // namespace

StreamCapture::~StreamCapture() {
    releaseCapture(stdoutCapture);
    releaseCapture(stderrCapture);
}

void StreamCapture::captureStream(Capture &capture) {
    releaseCapture(capture);
    fflush(capture.stream);
    clearerr(capture.stream);
    int pipefd[2] = {-1, -1};
    if (!createPipe(pipefd)) {
        return;
    }
    capture.readFd = pipefd[0];
    capture.savedFd = duplicateFd(getFd(capture.stream));
    redirectFd(pipefd[1], getFd(capture.stream));
    closeFd(pipefd[1]);
}

std::string StreamCapture::getCapturedStream(Capture &capture) {
    if (capture.savedFd == -1) {
        return "";
    }
    restoreStream(capture);
    auto captured = readAll(capture.readFd);
    releaseCapture(capture);
    return captured;
}

void StreamCapture::releaseCapture(Capture &capture) {
    if (capture.savedFd != -1) {
        restoreStream(capture);
    }
    if (capture.readFd != -1) {
        closeFd(capture.readFd);
        capture.readFd = -1;
    }
}

void StreamCapture::restoreStream(Capture &capture) {
    fflush(capture.stream);
    const bool limitExceeded = ferror(capture.stream) != 0;
    if (limitExceeded) {
        // stdio keeps the bytes it could not write; flush them into the null device so they do not reach the restored stream
        auto nullFd = openNullDevice();
        redirectFd(nullFd, getFd(capture.stream));
        closeFd(nullFd);
        fflush(capture.stream);
        clearerr(capture.stream);
        if (capture.stream == stdout) {
            std::cout.clear();
        } else {
            std::cerr.clear();
            std::clog.clear();
        }
    }
    redirectFd(capture.savedFd, getFd(capture.stream));
    closeFd(capture.savedFd);
    capture.savedFd = -1;
    if (limitExceeded) {
        ADD_FAILURE() << "StreamCapture: captured output exceeded the " << pipeSize << " bytes pipe limit";
    }
}
