/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/sys_calls_common.h"

#include <iostream>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace NEO {
namespace SysCalls {
int close(int fileDescriptor);
int open(const char *file, int flags);
void *dlopen(const char *filename, int flag);
int ioctl(int fileDescriptor, unsigned long int request, void *arg);
int getDevicePath(int deviceFd, char *buf, size_t &bufSize);
int access(const char *pathname, int mode);
int readlink(const char *path, char *buf, size_t bufsize);
int poll(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout);
int fstat(int fd, struct stat *buf);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off);
int munmap(void *addr, size_t size);
} // namespace SysCalls
} // namespace NEO
