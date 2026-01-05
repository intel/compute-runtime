/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/mtd/sysman_mtd.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr uint32_t mockMtdRegionCount = 5;
constexpr uint32_t mockMtdOffset = 0x1000;
constexpr size_t mockMtdSize = 0x100;

// Path to FD mapping for different test scenarios
inline const std::map<std::string, int> mockMtdPathToFdMap = {
    {"/dev/mtd3", 3}, // mtd3 maps to fd 3
    {"/dev/mtd4", 4}, // mtd4 maps to fd 4
    {"/dev/mtd5", 5}, // mtd5 maps to fd 5
    {"/dev/mtd6", 6}, // mtd6 maps to fd 6
    {"/dev/mtd7", 7}  // mtd7 maps to fd 7
};

// Define actual ioctl command values for comparison
// MEMERASE is _IOW('M', 2, struct erase_info_user)
// _IOW('M', 2, struct erase_info_user) = (0x1 << 30) | ('M' << 8) | 2 | (sizeof(struct erase_info_user) << 16)
constexpr unsigned long memEraseCmd = 0x40084d02; // Calculated value for MEMERASE

// Inline mock functions for MTD operations
inline int mockOpenSuccess(const char *pathname, int flags) {
    std::string path(pathname);
    auto it = mockMtdPathToFdMap.find(path);
    if (it != mockMtdPathToFdMap.end()) {
        return it->second;
    }
    return -1;
}

inline int mockCloseSuccess(int fd) {
    return 0;
}

inline int mockIoctlEraseSuccess(int fd, unsigned long request, void *arg) {
    // Accept fd values 3-7 for MTD devices
    if ((fd >= 3 && fd <= 7) && request == memEraseCmd) {
        return 0;
    }
    return -1;
}

inline ssize_t mockWriteSuccess(int fd, const void *buf, size_t count) {
    // Accept fd values 3-7 for MTD devices
    if (fd >= 3 && fd <= 7) {
        return static_cast<ssize_t>(count);
    }
    return -1;
}

inline off_t mockLseekSuccess(int fd, off_t offset, int whence) {
    // Accept fd values 3-7 for MTD devices
    if ((fd >= 3 && fd <= 7) && whence == SEEK_SET) {
        return offset; // Return the requested offset to indicate success
    }
    return -1;
}

inline off_t mockLseekWriteSuccess(int fd, off_t offset, int whence) {
    // Accept fd values 3-7 for MTD devices
    if ((fd >= 3 && fd <= 7) && whence == SEEK_SET && offset == mockMtdOffset) {
        return offset; // Return the expected offset for write operations
    }
    return -1;
}

inline ssize_t mockReadSpiDescriptorSuccess(int fd, void *buf, size_t count) {
    // Accept fd values 3-7 for MTD devices and return appropriate region limits
    if ((fd >= 3 && fd <= 7) && count == sizeof(MtdSysman::SpiDescRegionBar)) {

        MtdSysman::SpiDescRegionBar *regionBar = reinterpret_cast<MtdSysman::SpiDescRegionBar *>(buf);
        regionBar->reserved0 = 0;
        regionBar->reserved1 = 0;

        // Set different base and limits based on fd
        // Note: base and limit are 15-bit fields representing bits [26:12] of the address
        if (fd == 3) {
            regionBar->base = 0x0 >> 12;         // DESCRIPTOR region: 0x0 (used for both new mtd3 and legacy mtd0)
            regionBar->limit = 0x00000fff >> 12; // 0x0
        } else if (fd == 4) {
            regionBar->base = 0x00083000 >> 12;  // GSC region: 0x83
            regionBar->limit = 0x004a4fff >> 12; // 0x4a4
        } else if (fd == 5) {
            regionBar->base = 0x004a5000 >> 12;  // OptionROM region: 0x4a5
            regionBar->limit = 0x00ffffff >> 12; // 0xfff
        } else if (fd == 6 || fd == 7) {
            regionBar->base = 0x07fff000 >> 12;  // DAM region: 0x7fff
            regionBar->limit = 0x00000fff >> 12; // 0x0
        }

        return count;
    }
    return -1;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
