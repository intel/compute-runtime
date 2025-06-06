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
        _pipe(pipefd, 4096, O_TEXT);
        fflush(stream);
        savedFd = _dup(_fileno(stream));
        _dup2(pipefd[1], _fileno(stream));
        _close(pipefd[1]);
#else
        fflush(stream);
        pipe(pipefd);
        savedFd = dup(fileno(stream));
        dup2(pipefd[1], fileno(stream));
        close(pipefd[1]);
#endif
    }

    std::string getCapturedStream(FILE *stream, int pipefd[2], int &savedFd) {
#ifdef _WIN32
        fflush(stream);
        _dup2(savedFd, _fileno(stream));
        _close(savedFd);

        char buffer[4096];
        int count = _read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (count > 0) {
            buffer[count] = '\0';
            return std::string(buffer);
        }
        return "";
#else
        fflush(stream);
        dup2(savedFd, fileno(stream));
        close(savedFd);

        constexpr size_t bufferSize = 4096;
        char buffer[bufferSize];
        ssize_t count = read(pipefd[0], buffer, bufferSize - 1);
        if (count > 0) {
            buffer[count] = '\0';
            return std::string(buffer);
        }
        return "";
#endif
    }

    int pipefdStdout[2];
    int pipefdStderr[2];
    int saveStdout;
    int saveStderr;
};
