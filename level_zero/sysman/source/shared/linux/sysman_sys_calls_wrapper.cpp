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

// sync cannot fail and reports no status, hence no errno is captured
void SysmanSysCallsWrapper::sync() {
    NEO::SysCalls::sync();
}

int SysmanSysCallsWrapper::close(int fd, int &errorNum) {
    errno = 0;
    int result = NEO::SysCalls::close(fd);
    errorNum = errno;
    return result;
}

int SysmanSysCallsWrapper::dup(int oldfd, int &errorNum) {
    errno = 0;
    int result = NEO::SysCalls::dup(oldfd);
    errorNum = errno;
    return result;
}

int SysmanSysCallsWrapper::flock(int fd, int operation, int &errorNum) {
    errno = 0;
    int result = NEO::SysCalls::flock(fd, operation);
    errorNum = errno;
    return result;
}

int SysmanSysCallsWrapper::access(const char *pathname, int mode, int &errorNum) {
    errno = 0;
    int result = NEO::SysCalls::access(pathname, mode);
    errorNum = errno;
    return result;
}

FILE *SysmanSysCallsWrapper::fdopen(int fd, const char *mode, int &errorNum) {
    errno = 0;
    auto filep = NEO::SysCalls::fdopen(fd, mode);
    errorNum = errno;
    return filep;
}

char *SysmanSysCallsWrapper::fgets(char *s, int size, FILE *stream, int &errorNum) {
    errno = 0;
    auto ret = NEO::SysCalls::fgets(s, size, stream);
    errorNum = errno;
    return ret;
}

int SysmanSysCallsWrapper::fclose(FILE *stream, int &errorNum) {
    errno = 0;
    auto ret = NEO::SysCalls::fclose(stream);
    errorNum = errno;
    return ret;
}

int SysmanSysCallsWrapper::setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    return NEO::SysCalls::setvbuf(stream, buf, mode, size);
}

} // namespace Sysman
} // namespace L0
