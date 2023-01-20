/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <iostream>
#include <poll.h>
#include <sys/stat.h>

namespace NEO {
namespace SysCalls {

extern int (*sysCallsOpen)(const char *pathname, int flags);
extern ssize_t (*sysCallsPread)(int fd, void *buf, size_t count, off_t offset);
extern int (*sysCallsReadlink)(const char *path, char *buf, size_t bufsize);
extern int (*sysCallsIoctl)(int fileDescriptor, unsigned long int request, void *arg);
extern int (*sysCallsPoll)(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout);
extern ssize_t (*sysCallsRead)(int fd, void *buf, size_t count);
extern int (*sysCallsFstat)(int fd, struct stat *buf);

extern const char *drmVersion;
constexpr int fakeFileDescriptor = 123;
extern int passedFileDescriptorFlagsToSet;
extern int getFileDescriptorFlagsCalled;
extern int setFileDescriptorFlagsCalled;
} // namespace SysCalls
} // namespace NEO
