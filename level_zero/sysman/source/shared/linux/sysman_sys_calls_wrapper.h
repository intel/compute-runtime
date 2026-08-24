/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdio>
#include <sys/types.h>

namespace L0 {
namespace Sysman {

// Sysman wrapper class with static methods that capture errno for error handling
// These wrappers call NEO::SysCalls and store errno in the provided reference

class SysmanSysCallsWrapper {
  public:
    static int open(const char *pathname, int flags, int &errorNum);
    static ssize_t read(int fd, void *buf, size_t count, int &errorNum);
    static ssize_t write(int fd, const void *buf, size_t count, int &errorNum);
    static off_t lseek(int fd, off_t offset, int whence, int &errorNum);
    static int ioctl(int fd, unsigned long request, void *arg, int &errorNum);
    static void sync();
    static int close(int fd, int &errorNum);
    static int dup(int oldfd, int &errorNum);
    static int access(const char *pathname, int mode, int &errorNum);
    static FILE *fdopen(int fd, const char *mode, int &errorNum);
    static char *fgets(char *s, int size, FILE *stream, int &errorNum);
    static int fclose(FILE *stream, int &errorNum);
    static int setvbuf(FILE *stream, char *buf, int mode, size_t size);
};

} // namespace Sysman
} // namespace L0
