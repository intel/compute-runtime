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

class StdoutCapture {
  public:
    void captureStdout() {
#ifdef _WIN32
        _pipe(pipefd, 4096, O_TEXT);
        fflush(stdout);
        saveStdout = _dup(_fileno(stdout));
        _dup2(pipefd[1], _fileno(stdout));
        _close(pipefd[1]);
#else
        fflush(stdout);
        pipe(pipefd);
        saveStdout = dup(fileno(stdout));
        dup2(pipefd[1], fileno(stdout));
        close(pipefd[1]);
#endif
    }

    std::string getCapturedStdout() {
#ifdef _WIN32
        fflush(stdout);
        _dup2(saveStdout, _fileno(stdout));
        _close(saveStdout);

        char buffer[4096];
        int count = _read(pipefd[0], buffer, sizeof(buffer) - 1);
        if (count > 0) {
            buffer[count] = '\0';
            return std::string(buffer);
        }
        return "";
#else
        fflush(stdout);
        dup2(saveStdout, fileno(stdout));
        close(saveStdout);

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

  private:
    int pipefd[2];
    int saveStdout;
};