/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/i915.h"

#include "test_files_setup.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <iostream>
#include <link.h>
#include <poll.h>
#include <string.h>
#include <string_view>
#include <sys/ioctl.h>
#include <system_error>

namespace NEO {
namespace SysCalls {
uint32_t closeFuncCalled = 0u;
uint32_t openFuncCalled = 0u;
uint32_t accessFuncCalled = 0u;
uint32_t pollFuncCalled = 0u;
int openFuncRetVal = 0;
int closeFuncArgPassed = 0;
int closeFuncRetVal = 0;
int dlOpenFlags = 0;
int latestExitCode = 0;
bool exitCalled = false;
bool dlOpenCalled = false;
bool getNumThreadsCalled = false;
bool makeFakeDevicePath = false;
bool allowFakeDevicePath = false;
constexpr unsigned long int invalidIoctl = static_cast<unsigned long int>(-1);
int setErrno = 0;
int fstatFuncRetVal = 0;
int flockRetVal = 0;
uint32_t preadFuncCalled = 0u;
uint32_t pwriteFuncCalled = 0u;
uint32_t readFuncCalled = 0u;
uint32_t writeFuncCalled = 0u;
bool isInvalidAILTest = false;
const char *drmVersion = "i915";
int passedFileDescriptorFlagsToSet = 0;
int getFileDescriptorFlagsCalled = 0;
int setFileDescriptorFlagsCalled = 0;
int unlinkCalled = 0;
int scandirCalled = 0;
int mkstempCalled = 0;
int renameCalled = 0;
int pathFileExistsCalled = 0;
int flockCalled = 0;
int opendirCalled = 0;
int readdirCalled = 0;
int closedirCalled = 0;
int pidfdopenCalled = 0;
int pidfdgetfdCalled = 0;
int prctlCalled = 0;
int fsyncCalled = 0;
int fsyncArgPassed = 0;
int fsyncRetVal = 0;
uint32_t mkfifoFuncCalled = 0;
bool failMkfifo = 0;
bool failFcntl = false;
bool failFcntl1 = false;
bool failAccess = false;

std::vector<void *> mmapVector(64);
std::vector<void *> mmapCapturedExtendedPointers(64);
bool mmapCaptureExtendedPointers = false;
bool mmapAllowExtendedPointers = false;
bool failMmap = false;
uint32_t mmapFuncCalled = 0u;
uint32_t munmapFuncCalled = 0u;
bool failMunmap = false;

int (*sysCallsOpen)(const char *pathname, int flags) = nullptr;
int (*sysCallsClose)(int fileDescriptor) = nullptr;
int (*sysCallsOpenWithMode)(const char *pathname, int flags, int mode) = nullptr;
int (*sysCallsDlinfo)(void *handle, int request, void *info) = nullptr;
ssize_t (*sysCallsPread)(int fd, void *buf, size_t count, off_t offset) = nullptr;
ssize_t (*sysCallsPwrite)(int fd, const void *buf, size_t count, off_t offset) = nullptr;
int (*sysCallsReadlink)(const char *path, char *buf, size_t bufsize) = nullptr;
int (*sysCallsIoctl)(int fileDescriptor, unsigned long int request, void *arg) = nullptr;
int (*sysCallsPoll)(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) = nullptr;
ssize_t (*sysCallsRead)(int fd, void *buf, size_t count) = nullptr;
ssize_t (*sysCallsWrite)(int fd, const void *buf, size_t count) = nullptr;
int (*sysCallsPipe)(int pipeFd[2]) = nullptr;
int (*sysCallsFstat)(int fd, struct stat *buf) = nullptr;
char *(*sysCallsRealpath)(const char *path, char *buf) = nullptr;
int (*sysCallsRename)(const char *currName, const char *dstName);
int (*sysCallsScandir)(const char *dirp,
                       struct dirent ***namelist,
                       int (*filter)(const struct dirent *),
                       int (*compar)(const struct dirent **,
                                     const struct dirent **)) = nullptr;
int (*sysCallsUnlink)(const std::string &pathname) = nullptr;
int (*sysCallsStat)(const std::string &filePath, struct stat *statbuf) = nullptr;
int (*sysCallsMkstemp)(char *fileName) = nullptr;
int (*sysCallsMkdir)(const std::string &dir) = nullptr;
DIR *(*sysCallsOpendir)(const char *name) = nullptr;
struct dirent *(*sysCallsReaddir)(DIR *dir) = nullptr;
int (*sysCallsClosedir)(DIR *dir) = nullptr;
int (*sysCallsGetDevicePath)(int deviceFd, char *buf, size_t &bufSize) = nullptr;
int (*sysCallsPidfdOpen)(pid_t pid, unsigned int flags) = nullptr;
int (*sysCallsPidfdGetfd)(int pidfd, int fd, unsigned int flags) = nullptr;
int (*sysCallsPrctl)(int option, unsigned long arg) = nullptr;
off_t lseekReturn = 4096u;
std::atomic<int> lseekCalledCount(0);
long sysconfReturn = 1ull << 30;
std::string dlOpenFilePathPassed;
bool captureDlOpenFilePath = false;
std::string mkfifoPathNamePassed;

int mkdir(const std::string &path) {
    if (sysCallsMkdir != nullptr) {
        return sysCallsMkdir(path);
    }

    return 0;
}

void exit(int code) {
    exitCalled = true;
    latestExitCode = code;
}

int close(int fileDescriptor) {
    closeFuncCalled++;
    closeFuncArgPassed = fileDescriptor;
    return closeFuncRetVal;
}

int fsync(int fd) {
    fsyncCalled++;
    fsyncArgPassed = fd;
    return fsyncRetVal;
}

int open(const char *file, int flags) {
    openFuncCalled++;
    if (sysCallsOpen != nullptr) {
        return sysCallsOpen(file, flags);
    }

    if (strcmp(file, "/dev/dri/by-path/pci-0000:invalid-render") == 0) {
        return 0;
    }
    if (strcmp(file, NEO_SHARED_TEST_FILES_DIR "/linux/by-path/pci-0000:00:02.0-render") == 0) {
        return fakeFileDescriptor;
    }
    std::string_view configFile = file;
    if (configFile.find("config.file") != configFile.npos) {
        return fakeFileDescriptor;
    }

    return openFuncRetVal;
}

int openWithMode(const char *file, int flags, int mode) {
    if (sysCallsOpenWithMode != nullptr) {
        return sysCallsOpenWithMode(file, flags, mode);
    }
    std::string_view configFile = file;
    if (configFile.find("config.file") != configFile.npos) {
        return fakeFileDescriptor;
    }

    return 0;
}

void *dlopen(const char *filename, int flag) {
    dlOpenFlags = flag;
    dlOpenCalled = true;
    if (captureDlOpenFilePath) {
        if (filename) {
            dlOpenFilePathPassed = filename;
        } else {
            dlOpenFilePathPassed = {};
        }
    }
    return ::dlopen(filename, flag);
}

int dlinfo(void *handle, int request, void *info) {
    if (sysCallsDlinfo != nullptr) {
        return sysCallsDlinfo(handle, request, info);
    }

    if (request == RTLD_DI_LINKMAP) {
        return ::dlinfo(handle, request, info);
    }
    return -1;
}

int ioctl(int fileDescriptor, unsigned long int request, void *arg) {

    if (sysCallsIoctl != nullptr) {
        return sysCallsIoctl(fileDescriptor, request, arg);
    }

    if (fileDescriptor == fakeFileDescriptor) {
        if (request == DRM_IOCTL_VERSION) {
            auto pVersion = static_cast<DrmVersion *>(arg);
            memcpy_s(pVersion->name, pVersion->nameLen, drmVersion, std::min(pVersion->nameLen, strlen(drmVersion) + 1));
        }
    }
    if (request == invalidIoctl) {
        errno = 0;
        if (setErrno != 0) {
            errno = setErrno;
            setErrno = 0;
        }
        return -1;
    }
    return 0;
}

unsigned int getProcessId() {
    return 0xABCEDF;
}

unsigned int getCurrentProcessId() {
    return 0xABCEDF;
}

unsigned long getNumThreads() {
    getNumThreadsCalled = true;
    return 1;
}

int access(const char *pathName, int mode) {
    accessFuncCalled++;
    if (failAccess) {
        return -1;
    }
    if (F_OK == mode) {
        if (mkfifoPathNamePassed == pathName) {
            return 0;
        }
    }
    if (allowFakeDevicePath || strcmp(pathName, "/sys/dev/char/226:128") == 0) {
        return 0;
    }
    return -1;
}

int readlink(const char *path, char *buf, size_t bufsize) {

    if (sysCallsReadlink != nullptr) {
        return sysCallsReadlink(path, buf, bufsize);
    }

    if (isInvalidAILTest) {
        return -1;
    }
    if (strcmp(path, "/proc/self/exe") == 0) {
        strcpy_s(buf, sizeof("/proc/self/exe/tests"), "/proc/self/exe/tests");

        return sizeof("/proc/self/exe/tests");
    }

    if (strcmp(path, "/sys/dev/char/226:128") != 0) {
        return -1;
    }

    constexpr size_t sizeofPath = sizeof("../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:02.0/drm/renderD128");

    strcpy_s(buf, sizeofPath, "../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:02.0/drm/renderD128");
    return sizeofPath;
}

int getDevicePath(int deviceFd, char *buf, size_t &bufSize) {
    if (sysCallsGetDevicePath != nullptr) {
        return sysCallsGetDevicePath(deviceFd, buf, bufSize);
    }

    if (deviceFd <= 0) {
        return -1;
    }
    constexpr size_t sizeofPath = sizeof("/sys/dev/char/226:128");

    makeFakeDevicePath ? strcpy_s(buf, sizeofPath, "/sys/dev/char/xyzwerq") : strcpy_s(buf, sizeofPath, "/sys/dev/char/226:128");
    bufSize = sizeofPath;

    return 0;
}

int poll(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) {
    pollFuncCalled++;
    if (sysCallsPoll != nullptr) {
        return sysCallsPoll(pollFd, numberOfFds, timeout);
    }
    return 0;
}

int fstat(int fd, struct stat *buf) {
    if (sysCallsFstat != nullptr) {
        return sysCallsFstat(fd, buf);
    }
    return fstatFuncRetVal;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    if (sysCallsPread != nullptr) {
        return sysCallsPread(fd, buf, count, offset);
    }
    preadFuncCalled++;
    return 0;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    if (sysCallsPwrite != nullptr) {
        return sysCallsPwrite(fd, buf, count, offset);
    }
    pwriteFuncCalled++;
    return 0;
}

void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) noexcept {
    mmapFuncCalled++;
    if (failMmap) {
        return reinterpret_cast<void *>(-1);
    }
    if (reinterpret_cast<uint64_t>(addr) > maxNBitValue(48)) {
        if (mmapCaptureExtendedPointers) {
            mmapCapturedExtendedPointers.push_back(addr);
        }
        if (!mmapAllowExtendedPointers) {
            addr = nullptr;
        }
    }
    if (addr) {
        return addr;
    }
    void *ptr = nullptr;
    if (size > 0) {
        ptr = alignedMalloc(size, MemoryConstants::pageSize64k);
        if (!ptr) {
            return reinterpret_cast<void *>(0x1000);
        }
        mmapVector.push_back(ptr);
    }
    return ptr;
}

int munmap(void *addr, size_t size) noexcept {
    munmapFuncCalled++;
    if (failMunmap) {
        return -1;
    }

    auto ptrIt = std::find(mmapVector.begin(), mmapVector.end(), addr);
    if (ptrIt != mmapVector.end()) {
        mmapVector.erase(ptrIt);
        alignedFree(addr);
    }
    return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (sysCallsRead != nullptr) {
        return sysCallsRead(fd, buf, count);
    }
    readFuncCalled++;
    return 0;
}

ssize_t write(int fd, const void *buf, size_t count) {
    if (sysCallsWrite != nullptr) {
        return sysCallsWrite(fd, buf, count);
    }
    writeFuncCalled++;
    return 0;
}

int pipe(int pipeFd[2]) {
    if (sysCallsPipe != nullptr) {
        return sysCallsPipe(pipeFd);
    }
    return 0;
}

int fcntl(int fd, int cmd) {
    if (failFcntl) {
        return -1;
    }

    if (cmd == F_GETFL) {
        getFileDescriptorFlagsCalled++;
        return O_RDWR;
    }
    return 0;
}

int fcntl(int fd, int cmd, int arg) {
    if (failFcntl1) {
        return -1;
    }

    if (cmd == F_SETFL) {
        setFileDescriptorFlagsCalled++;
        passedFileDescriptorFlagsToSet = arg;
    }

    return 0;
}

char *realpath(const char *path, char *buf) {
    if (sysCallsRealpath != nullptr) {
        return sysCallsRealpath(path, buf);
    }
    return nullptr;
}

int mkstemp(char *fileName) {
    mkstempCalled++;

    if (sysCallsMkstemp != nullptr) {
        return sysCallsMkstemp(fileName);
    }

    return 0;
}

int flock(int fd, int flag) {
    flockCalled++;

    if (fd >= 0 && flockRetVal == 0) {
        return 0;
    }

    return -1;
}
int rename(const char *currName, const char *dstName) {
    renameCalled++;

    if (sysCallsRename != nullptr) {
        return sysCallsRename(currName, dstName);
    }

    return 0;
}

int scandir(const char *dirp,
            struct dirent ***namelist,
            int (*filter)(const struct dirent *),
            int (*compar)(const struct dirent **,
                          const struct dirent **)) {
    scandirCalled++;

    if (sysCallsScandir != nullptr) {
        return sysCallsScandir(dirp, namelist, filter, compar);
    }

    return 0;
}

int unlink(const std::string &pathname) {
    unlinkCalled++;

    if (sysCallsUnlink != nullptr) {
        return sysCallsUnlink(pathname);
    }
    if (mkfifoPathNamePassed == pathname) {
        mkfifoPathNamePassed.clear();
        mkfifoPathNamePassed.shrink_to_fit();
    }

    return 0;
}

int stat(const std::string &filePath, struct stat *statbuf) {
    if (sysCallsStat != nullptr) {
        return sysCallsStat(filePath, statbuf);
    }

    return 0;
}

DIR *opendir(const char *name) {
    opendirCalled++;
    if (sysCallsOpendir != nullptr) {
        return sysCallsOpendir(name);
    }

    return nullptr;
}

struct dirent *readdir(DIR *dir) {
    readdirCalled++;
    if (sysCallsReaddir != nullptr) {
        return sysCallsReaddir(dir);
    }

    return nullptr;
}

int closedir(DIR *dir) {
    closedirCalled++;
    if (sysCallsClosedir != nullptr) {
        return sysCallsClosedir(dir);
    }

    return 0;
}

off_t lseek(int fd, off_t offset, int whence) noexcept {
    lseekCalledCount++;
    return lseekReturn;
}
long sysconf(int name) {
    return sysconfReturn;
}
int mkfifo(const char *pathname, mode_t mode) {
    mkfifoFuncCalled++;
    if (failMkfifo) {
        errno = 0;
        if (setErrno != 0) {
            errno = setErrno;
            setErrno = 0;
        }
        return -1;
    }
    if (nullptr == pathname) {
        return -1;
    }
    mkfifoPathNamePassed = pathname;
    return 0;
}

int pidfdopen(pid_t pid, unsigned int flags) {
    pidfdopenCalled++;
    if (sysCallsPidfdOpen != nullptr) {
        return sysCallsPidfdOpen(pid, flags);
    }
    return 0;
}

int pidfdgetfd(int pid, int targetfd, unsigned int flags) {
    pidfdgetfdCalled++;
    if (sysCallsPidfdGetfd != nullptr) {
        return sysCallsPidfdGetfd(pid, targetfd, flags);
    }
    return 0;
}

int prctl(int option, unsigned long arg) {
    prctlCalled++;
    if (sysCallsPrctl != nullptr) {
        return sysCallsPrctl(option, arg);
    }
    return 0;
}

char **getEnviron() {
    return NEO::ULT::getCurrentEnviron();
}

} // namespace SysCalls

namespace ULT {

static char *defaultTestEnviron[] = {nullptr};
static char **mockEnviron = defaultTestEnviron;

MockEnvironBackup::MockEnvironBackup(char **newEnviron)
    : mockEnvironBackup(&mockEnviron, newEnviron) {
}

int MockEnvironBackup::defaultStatMock(const std::string &filePath, struct stat *statbuf) noexcept {
    statbuf->st_mode = S_IFDIR;
    return 0;
}

std::vector<char *> MockEnvironBackup::buildEnvironFromMap(
    const std::unordered_map<std::string, std::string> &envs,
    std::vector<std::string> &storage) {
    storage.clear();
    for (const auto &kv : envs) {
        storage.push_back(kv.first + "=" + kv.second);
    }
    std::vector<char *> result;
    for (auto &str : storage) {
        result.push_back(const_cast<char *>(str.c_str()));
    }
    result.push_back(nullptr);
    return result;
}

char **getCurrentEnviron() {
    return mockEnviron;
}

void setMockEnviron(char **mock) {
    mockEnviron = mock ? mock : defaultTestEnviron;
}

} // namespace ULT
} // namespace NEO
