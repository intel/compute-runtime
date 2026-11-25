/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/stream_capture_os.h"

#include <fcntl.h>
#include <io.h>

namespace StreamCaptureOs {

bool createPipe(int pipefd[2]) {
    if (_pipe(pipefd, static_cast<unsigned int>(StreamCapture::pipeSize), O_TEXT) == -1) {
        return false;
    }
    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    SetNamedPipeHandleState(reinterpret_cast<HANDLE>(_get_osfhandle(pipefd[1])), &mode, nullptr, nullptr);
    return true;
}

int duplicateFd(int fd) {
    return _dup(fd);
}

void redirectFd(int srcFd, int dstFd) {
    _dup2(srcFd, dstFd);
}

void closeFd(int fd) {
    _close(fd);
}

int readFromFd(int fd, char *buffer, unsigned int size) {
    return _read(fd, buffer, size);
}

int openNullDevice() {
    return _open("NUL", O_WRONLY);
}

int getFd(FILE *stream) {
    return _fileno(stream);
}

} // namespace StreamCaptureOs
