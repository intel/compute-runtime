/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <fcntl.h>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <cstdio>
#include <cstring>
#include <unistd.h>
#endif

class StreamCapture {
  public:
    ~StreamCapture() {
#ifdef _WIN32
        if (pipefdStdout[0] != -1)
            _close(pipefdStdout[0]);
        if (pipefdStderr[0] != -1)
            _close(pipefdStderr[0]);
#else
        if (pipefdStdout[0] != -1)
            close(pipefdStdout[0]);
        if (pipefdStderr[0] != -1)
            close(pipefdStderr[0]);
#endif
    }
    void captureStdout() {
        captureStream(stdout, pipefdStdout, saveStdout);
    }

    void captureStderr() {
        captureStream(stderr, pipefdStderr, saveStderr);
    }

    std::string getCapturedStdout() {
        return getCapturedStream(stdout, pipefdStdout, saveStdout);
    }

    std::string getCapturedStderr() {
        return getCapturedStream(stderr, pipefdStderr, saveStderr);
    }

  private:
    void captureStream(FILE *stream, int pipefd[2], int &savedFd) {
#ifdef _WIN32
        _pipe(pipefd, bufferSize, O_TEXT);
        fflush(stream);
        savedFd = _dup(_fileno(stream));
        _dup2(pipefd[1], _fileno(stream));
        _close(pipefd[1]);
        pipefd[1] = -1;
#else
        fflush(stream);
        if (pipe(pipefd) == -1) {
            return;
        }
        fcntl(pipefd[0], F_SETPIPE_SZ, bufferSize);
        savedFd = dup(fileno(stream));
        dup2(pipefd[1], fileno(stream));
        close(pipefd[1]);
        pipefd[1] = -1;
#endif
    }

    std::string getCapturedStream(FILE *stream, int pipefd[2], int &savedFd) {
#ifdef _WIN32
        fflush(stream);
        _dup2(savedFd, _fileno(stream));
        _close(savedFd);

        char buffer[bufferSize];
        int count = _read(pipefd[0], buffer, sizeof(buffer) - 1);
        _close(pipefd[0]);
        pipefd[0] = -1;
        if (count > 0) {
            buffer[count] = '\0';
            return std::string(buffer);
        }
        return "";
#else
        fflush(stream);
        dup2(savedFd, fileno(stream));
        close(savedFd);

        char buffer[bufferSize];
        ssize_t count = read(pipefd[0], buffer, bufferSize - 1);
        close(pipefd[0]);
        pipefd[0] = -1;
        if (count > 0) {
            buffer[count] = '\0';
            return std::string(buffer);
        }
        return "";
#endif
    }
    static constexpr size_t bufferSize = 16384;
    int pipefdStdout[2]{-1, -1};
    int pipefdStderr[2]{-1, -1};
    int saveStdout{-1};
    int saveStderr{-1};
};
