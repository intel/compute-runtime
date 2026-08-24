/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/mtd/sysman_mtd.h"

#include <string>

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockMtdOffset = 0x1000;
constexpr size_t mockMtdSize = 0x100;

// The DATA device of the device under test is enumerated as /dev/mtd1, as on hardware. The file
// descriptors are arbitrary tokens, kept away from the standard streams
inline const std::string mockMtdDevicePath = "/dev/mtd1";
constexpr int mockMtdDeviceFd = 3;
inline const std::string mockMtdSecondDevicePath = "/dev/mtd2";
constexpr int mockMtdSecondDeviceFd = 4;

// MEMERASE is _IOW('M', 2, struct erase_info_user). The value is spelled out, so that the ioctl the
// driver has to issue is pinned by the test
constexpr unsigned long memEraseCmd = 0x40084d02;

// Records the arguments of the mtd system calls
struct MockMtdSysCalls {
    std::string openedPath;
    uint32_t openCount = 0;
    uint32_t eraseStart = 0;
    uint32_t eraseLength = 0;
    off_t writeOffset = -1;
    const void *writeData = nullptr;
    size_t writeCount = 0;
};

inline MockMtdSysCalls mockMtdSysCalls = {};

inline bool isMockMtdFd(int fd) {
    return (fd == mockMtdDeviceFd) || (fd == mockMtdSecondDeviceFd);
}

inline int mockOpenSuccess(const char *pathname, int flags) {
    std::string path(pathname);
    mockMtdSysCalls.openedPath = path;
    mockMtdSysCalls.openCount++;
    if (path == mockMtdDevicePath) {
        return mockMtdDeviceFd;
    }
    if (path == mockMtdSecondDevicePath) {
        return mockMtdSecondDeviceFd;
    }
    return -1;
}

inline int mockCloseSuccess(int fd) {
    return 0;
}

inline int mockIoctlEraseSuccess(int fd, unsigned long request, void *arg) {
    if (isMockMtdFd(fd) && request == memEraseCmd) {
        auto pEraseInfo = static_cast<erase_info_t *>(arg);
        mockMtdSysCalls.eraseStart = pEraseInfo->start;
        mockMtdSysCalls.eraseLength = pEraseInfo->length;
        return 0;
    }
    return -1;
}

inline ssize_t mockWriteSuccess(int fd, const void *buf, size_t count) {
    if (isMockMtdFd(fd)) {
        mockMtdSysCalls.writeData = buf;
        mockMtdSysCalls.writeCount = count;
        return static_cast<ssize_t>(count);
    }
    return -1;
}

inline off_t mockLseekSuccess(int fd, off_t offset, int whence) {
    if (isMockMtdFd(fd) && whence == SEEK_SET) {
        mockMtdSysCalls.writeOffset = offset;
        return offset;
    }
    return -1;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
