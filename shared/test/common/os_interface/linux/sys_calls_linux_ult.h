/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <iostream>
#include <poll.h>
#include <sys/stat.h>
#include <vector>

namespace NEO {
namespace SysCalls {

extern int (*sysCallsOpen)(const char *pathname, int flags);
extern ssize_t (*sysCallsPread)(int fd, void *buf, size_t count, off_t offset);
extern ssize_t (*sysCallsPwrite)(int fd, const void *buf, size_t count, off_t offset);
extern int (*sysCallsReadlink)(const char *path, char *buf, size_t bufsize);
extern int (*sysCallsIoctl)(int fileDescriptor, unsigned long int request, void *arg);
extern int (*sysCallsPoll)(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout);
extern ssize_t (*sysCallsRead)(int fd, void *buf, size_t count);
extern int (*sysCallsFstat)(int fd, struct stat *buf);
extern char *(*sysCallsRealpath)(const char *path, char *buf);

extern const char *drmVersion;
constexpr int fakeFileDescriptor = 123;
extern int passedFileDescriptorFlagsToSet;
extern int getFileDescriptorFlagsCalled;
extern int setFileDescriptorFlagsCalled;
extern uint32_t closeFuncCalled;

extern std::vector<void *> mmapVector;
extern std::vector<void *> mmapCapturedExtendedPointers;
extern bool mmapCaptureExtendedPointers;
extern bool mmapAllowExtendedPointers;
extern uint32_t mmapFuncCalled;
extern uint32_t munmapFuncCalled;
} // namespace SysCalls
} // namespace NEO
