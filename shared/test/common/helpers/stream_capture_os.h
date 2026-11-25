/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdio>

namespace StreamCaptureOs {
bool createPipe(int pipefd[2]);
int duplicateFd(int fd);
void redirectFd(int srcFd, int dstFd);
void closeFd(int fd);
int readFromFd(int fd, char *buffer, unsigned int size);
int openNullDevice();
int getFd(FILE *stream);
} // namespace StreamCaptureOs
