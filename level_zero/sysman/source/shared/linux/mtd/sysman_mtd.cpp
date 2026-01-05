/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/mtd/sysman_mtd.h"

#include <cstring>
#include <sys/ioctl.h>

namespace L0 {
namespace Sysman {

static std::map<std::string, uint32_t> mtdRegionToOffsetMap = {
    {"DESCRIPTOR", 0},
    {"GSC", 2},
    {"PADDING", 9},
    {"OptionROM", 11},
    {"DAM", 12}};

std::unique_ptr<MemoryTechnologyDeviceInterface> MemoryTechnologyDeviceInterface::create() {
    return std::make_unique<MemoryTechnologyDeviceInterface>();
}

ze_result_t MemoryTechnologyDeviceInterface::erase(const std::string &filePath, uint32_t offset, size_t size) {
    int fd = NEO::SysCalls::open(filePath.c_str(), O_RDWR);
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    erase_info_t eraseInfo;
    eraseInfo.start = offset;
    eraseInfo.length = static_cast<uint32_t>(size);

    int result = NEO::SysCalls::ioctl(fd, MEMERASE, &eraseInfo);
    NEO::SysCalls::close(fd);

    if (result < 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryTechnologyDeviceInterface::write(const std::string &filePath, uint32_t offset, const uint8_t *data, size_t size) {
    int fd = NEO::SysCalls::open(filePath.c_str(), O_RDWR);
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Seek to the specified offset
    if (NEO::SysCalls::lseek(fd, offset, SEEK_SET) != static_cast<off_t>(offset)) {
        NEO::SysCalls::close(fd);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Write the data
    ssize_t bytesWritten = NEO::SysCalls::write(fd, data, size);
    NEO::SysCalls::close(fd);

    if (bytesWritten != static_cast<ssize_t>(size)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryTechnologyDeviceInterface::getDeviceInfo(const std::string &filePath, std::map<std::string, std::map<uint32_t, uint32_t>> &mtdRegionDeviceInfoMap) {
    int fd = NEO::SysCalls::open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Iterate over the mtdRegionToOffsetMap to get region information
    for (const auto &regionEntry : mtdRegionToOffsetMap) {
        const std::string &regionName = regionEntry.first;
        uint32_t spiOffset = 0x40 + (regionEntry.second * sizeof(uint32_t));

        // Read the SPI descriptor region data
        MtdSysman::SpiDescRegionBar value;
        if (NEO::SysCalls::lseek(fd, spiOffset, SEEK_SET) != static_cast<off_t>(spiOffset)) {
            continue; // Skip this region if seek fails
        }

        ssize_t bytesRead = NEO::SysCalls::read(fd, &value, sizeof(value));
        if (bytesRead != sizeof(value)) {
            continue; // Skip this region if read fails
        }

        // Calculate region begin and end
        uint32_t regionBegin = static_cast<uint32_t>(value.base) << 12;
        uint32_t regionEnd = (static_cast<uint32_t>(value.limit) << 12) + 0xFFF;

        // Verify that regionBegin is less than regionEnd
        if (regionBegin >= regionEnd) {
            continue; // Skip this region if the range is invalid
        }

        // Store the region information in the map
        mtdRegionDeviceInfoMap[regionName][0] = regionBegin; // Use 0 for regionBegin
        mtdRegionDeviceInfoMap[regionName][1] = regionEnd;   // Use 1 for regionEnd
    }

    NEO::SysCalls::close(fd);
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
