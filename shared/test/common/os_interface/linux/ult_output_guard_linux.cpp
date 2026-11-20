/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_output_guard.h"

#include <cstdio>
#include <fcntl.h>
#include <poll.h>
#include <thread>
#include <unistd.h>

namespace UltOutputGuard {
namespace {
std::atomic<bool> active = false;
int savedFds[2] = {-1, -1};
int readFds[2] = {-1, -1};
std::thread teeThread;

int moveToHighFd(int fd) {
    constexpr int guardFdBase = 512;
    auto highFd = fcntl(fd, F_DUPFD_CLOEXEC, guardFdBase);
    if (highFd == -1) {
        return fd;
    }
    close(fd);
    return highFd;
}

bool hasInput(const pollfd &fd) {
    return (fd.revents & POLLIN) != 0;
}

bool hasInputNow(int fd) {
    pollfd pollFd = {fd, POLLIN, 0};
    return poll(&pollFd, 1, 0) > 0 && hasInput(pollFd);
}

ssize_t copyChunk(int stream) {
    char buffer[4096];
    auto bytes = read(readFds[stream], buffer, sizeof(buffer));
    if (bytes > 0) {
        [[maybe_unused]] auto written = write(savedFds[stream], buffer, static_cast<size_t>(bytes));
    }
    return bytes;
}

void teeLoop() {
    pollfd fds[2] = {{readFds[0], POLLIN, 0}, {readFds[1], POLLIN, 0}};
    int openStreams = 2;
    while (openStreams > 0) {
        if (poll(fds, 2, -1) <= 0) {
            continue;
        }
        markBusy();
        for (int i = 0; i < 2; i++) {
            if (fds[i].revents == 0) {
                continue;
            }
            auto bytes = hasInput(fds[i]) ? copyChunk(i) : 0;
            if (bytes > 0) {
                capturedBytes += static_cast<size_t>(bytes);
            } else {
                fds[i].fd = -1;
                openStreams--;
            }
        }
        markIdle();
    }
}
} // namespace

void start() {
    fflush(stdout);
    fflush(stderr);
    int pipeFds[2][2];
    for (int i = 0; i < 2; i++) {
        if (pipe(pipeFds[i]) == -1) {
            for (int j = 0; j < i; j++) {
                close(pipeFds[j][0]);
                close(pipeFds[j][1]);
            }
            return;
        }
    }
    for (int i = 0; i < 2; i++) {
        savedFds[i] = moveToHighFd(dup(i + 1));
        readFds[i] = moveToHighFd(pipeFds[i][0]);
        dup2(pipeFds[i][1], i + 1);
        close(pipeFds[i][1]);
    }
    active = true;
    teeThread = std::thread(teeLoop);
}

bool pipesHaveUnreadData() {
    if (!active) {
        return false;
    }
    pollfd fds[2] = {{readFds[0], POLLIN, 0}, {readFds[1], POLLIN, 0}};
    return poll(fds, 2, 0) > 0 && (hasInput(fds[0]) || hasInput(fds[1]));
}

void stop() {
    if (!active) {
        return;
    }
    drain();
    active = false;
    for (int i = 0; i < 2; i++) {
        dup2(savedFds[i], i + 1);
    }
    teeThread.join();
    for (int i = 0; i < 2; i++) {
        close(readFds[i]);
        close(savedFds[i]);
        readFds[i] = -1;
        savedFds[i] = -1;
    }
}

void onFatal() {
    if (!active) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        dup2(savedFds[i], i + 1);
        while (hasInputNow(readFds[i]) && copyChunk(i) > 0) {
        }
    }
}
} // namespace UltOutputGuard
