/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/stream_capture_os.h"

#include <fcntl.h>
#include <unistd.h>

namespace StreamCaptureOs {

bool createPipe(int pipefd[2]) {
    if (pipe2(pipefd, O_NONBLOCK) == -1) {
        return false;
    }
    fcntl(pipefd[0], F_SETPIPE_SZ, static_cast<int>(StreamCapture::pipeSize));
    return true;
}

int duplicateFd(int fd) {
    return dup(fd);
}

void redirectFd(int srcFd, int dstFd) {
    dup2(srcFd, dstFd);
}

void closeFd(int fd) {
    close(fd);
}

int readFromFd(int fd, char *buffer, unsigned int size) {
    return static_cast<int>(read(fd, buffer, size));
}

int openNullDevice() {
    return open("/dev/null", O_WRONLY);
}

int getFd(FILE *stream) {
    return fileno(stream);
}

} // namespace StreamCaptureOs
