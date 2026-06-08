/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"

#include "shared/source/os_interface/linux/sys_calls.h"

#include <errno.h>

namespace L0 {
namespace Sysman {

int SysmanSysCallsWrapper::open(const char *pathname, int flags, int &errorNum) {
    errno = 0;
    int result = NEO::SysCalls::open(pathname, flags);
    errorNum = errno;
    return result;
}

ssize_t SysmanSysCallsWrapper::read(int fd, void *buf, size_t count, int &errorNum) {
    errno = 0;
    ssize_t result = NEO::SysCalls::read(fd, buf, count);
    errorNum = errno;
    return result;
}

ssize_t SysmanSysCallsWrapper::write(int fd, const void *buf, size_t count, int &errorNum) {
    errno = 0;
    ssize_t result = NEO::SysCalls::write(fd, buf, count);
    errorNum = errno;
    return result;
}

off_t SysmanSysCallsWrapper::lseek(int fd, off_t offset, int whence, int &errorNum) {
    errno = 0;
    off_t result = NEO::SysCalls::lseek(fd, offset, whence);
    errorNum = errno;
    return result;
}

int SysmanSysCallsWrapper::ioctl(int fd, unsigned long request, void *arg, int &errorNum) {
    errno = 0;
    int result = NEO::SysCalls::ioctl(fd, request, arg);
    errorNum = errno;
    return result;
}

int SysmanSysCallsWrapper::close(int fd, int &errorNum) {
    errno = 0;
    int result = NEO::SysCalls::close(fd);
    errorNum = errno;
    return result;
}

} // namespace Sysman
} // namespace L0
