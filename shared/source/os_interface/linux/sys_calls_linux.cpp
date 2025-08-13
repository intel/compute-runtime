/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"

#include <cstdlib>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <stdio.h>
#include <string>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif

#ifndef SYS_pidfd_getfd
#define SYS_pidfd_getfd 438
#endif

namespace NEO {

namespace SysCalls {
char **getEnviron() {
    return environ;
}
void exit(int code) {
    std::exit(code);
}

unsigned int getProcessId() {
    static unsigned int pid = getpid();
    return pid;
}

unsigned int getCurrentProcessId() {
    return getpid();
}

unsigned long getNumThreads() {
    struct stat taskStat;
    if (stat("/proc/self/task", &taskStat) == 0) {
        return taskStat.st_nlink - 2;
    }
    return 0;
}

int mkdir(const std::string &path) {
    return ::mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

int close(int fd) {
    return ::close(fd);
}
int open(const char *file, int flags) {
    return ::open(file, flags);
}
int openWithMode(const char *file, int flags, int mode) {
    return ::open(file, flags, mode);
}

int fsync(int fd) {
    return ::fsync(fd);
}

int ioctl(int fileDescriptor, unsigned long int request, void *arg) {
    return ::ioctl(fileDescriptor, request, arg);
}

void *dlopen(const char *filename, int flag) {
    return ::dlopen(filename, flag);
}

int dlinfo(void *handle, int request, void *info) {
    return ::dlinfo(handle, request, info);
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

void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) noexcept {
    return ::mmap(addr, size, prot, flags, fd, off);
}

int munmap(void *addr, size_t size) noexcept {
    return ::munmap(addr, size);
}

ssize_t read(int fd, void *buf, size_t count) {
    return ::read(fd, buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return ::write(fd, buf, count);
}

int fcntl(int fd, int cmd) {
    return ::fcntl(fd, cmd);
}
int fcntl(int fd, int cmd, int arg) {
    return ::fcntl(fd, cmd, arg);
}

char *realpath(const char *path, char *buf) {
    return ::realpath(path, buf);
}

int stat(const std::string &filePath, struct stat *statbuf) {
    return ::stat(filePath.c_str(), statbuf);
}

int pipe(int pipeFd[2]) {
    return ::pipe(pipeFd);
}

int mkstemp(char *filePath) {
    return ::mkstemp(filePath);
}

int flock(int fd, int flag) {
    return ::flock(fd, flag);
}

int rename(const char *currName, const char *dstName) {
    return ::rename(currName, dstName);
}

int scandir(const char *dirp,
            struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **,
                          const struct dirent **)) {
    return ::scandir(dirp, namelist, filter, compar);
}

int unlink(const std::string &pathname) {
    return ::unlink(pathname.c_str());
}

DIR *opendir(const char *name) {
    return ::opendir(name);
}

struct dirent *readdir(DIR *dir) {
    return ::readdir(dir);
}

int closedir(DIR *dir) {
    return ::closedir(dir);
}

int pidfdopen(pid_t pid, unsigned int flags) {
    long retval = ::syscall(SYS_pidfd_open, pid, flags);
    return static_cast<int>(retval);
}

int pidfdgetfd(int pidfd, int targetfd, unsigned int flags) {
    long retval = ::syscall(SYS_pidfd_getfd, pidfd, targetfd, flags);
    return static_cast<int>(retval);
}

int prctl(int option, unsigned long arg) {
    return ::prctl(option, arg);
}

off_t lseek(int fd, off_t offset, int whence) noexcept {
    return ::lseek(fd, offset, whence);
}
long sysconf(int name) {
    return ::sysconf(name);
}
int mkfifo(const char *pathname, mode_t mode) {
    return ::mkfifo(pathname, mode);
}
} // namespace SysCalls
} // namespace NEO
