/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/sys_calls_common.h"

#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace NEO {
namespace SysCalls {
int fsync(int fd);
int close(int fd);
int mkdir(const std::string &path);
int open(const char *file, int flags);
int openWithMode(const char *file, int flags, int mode);
void *dlopen(const char *filename, int flag);
int dlinfo(void *handle, int request, void *info);
int ioctl(int fileDescriptor, unsigned long int request, void *arg);
int getDevicePath(int deviceFd, char *buf, size_t &bufSize);
int access(const char *pathname, int mode);
int readlink(const char *path, char *buf, size_t bufsize);
int poll(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout);
int fstat(int fd, struct stat *buf);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) noexcept;
int munmap(void *addr, size_t size) noexcept;
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int fcntl(int fd, int cmd);
int fcntl(int fd, int cmd, int arg);
char *realpath(const char *path, char *buf);
int stat(const std::string &filePath, struct stat *statbuf);
int pipe(int pipefd[2]);
int flock(int fd, int flag);
int mkstemp(char *filePath);
int rename(const char *currName, const char *dstName);
int scandir(const char *dirp,
            struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **,
                          const struct dirent **));
int unlink(const std::string &pathname);
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
off_t lseek(int fd, off_t offset, int whence) noexcept;
long sysconf(int name);
int mkfifo(const char *pathname, mode_t mode);
int pidfdopen(pid_t pid, unsigned int flags);
int pidfdgetfd(int pidfd, int targetfd, unsigned int flags);
int prctl(int option, unsigned long arg);
char **getEnviron();

} // namespace SysCalls
} // namespace NEO
