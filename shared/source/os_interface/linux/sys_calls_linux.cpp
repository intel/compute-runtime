/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

namespace NEO {

namespace SysCalls {

unsigned int getProcessId() {
    return getpid();
}

unsigned long getNumThreads() {
    struct stat taskStat;
    stat("/proc/self/task", &taskStat);
    return taskStat.st_nlink - 2;
}

int close(int fileDescriptor) {
    return ::close(fileDescriptor);
}
int open(const char *file, int flags) {
    return ::open(file, flags);
}
int ioctl(int fileDescriptor, unsigned long int request, void *arg) {
    return ::ioctl(fileDescriptor, request, arg);
}

void *dlopen(const char *filename, int flag) {
    return ::dlopen(filename, flag);
}

int access(const char *pathName, int mode) {
    return ::access(pathName, mode);
}

int readlink(const char *path, char *buf, size_t bufsize) {
    return static_cast<int>(::readlink(path, buf, bufsize));
}

int getDevicePath(int deviceFd, char *buf, size_t &bufSize) {
    struct stat st;
    if (::fstat(deviceFd, &st)) {
        return -1;
    }

    snprintf(buf, bufSize, "/sys/dev/char/%d:%d",
             major(st.st_rdev), minor(st.st_rdev));

    return 0;
}

int poll(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) {
    return ::poll(pollFd, numberOfFds, timeout);
}

int fstat(int fd, struct stat *buf) {
    return ::fstat(fd, buf);
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    return ::pread(fd, buf, count, offset);
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return ::pwrite(fd, buf, count, offset);
}

void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
    return ::mmap(addr, size, prot, flags, fd, off);
}

int munmap(void *addr, size_t size) {
    return ::munmap(addr, size);
}

ssize_t read(int fd, void *buf, size_t count) {
    return ::read(fd, buf, count);
}

int fcntl(int fd, int cmd) {
    return ::fcntl(fd, cmd);
}
int fcntl(int fd, int cmd, int arg) {
    return ::fcntl(fd, cmd, arg);
}

} // namespace SysCalls
} // namespace NEO
