/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/mtd/sysman_mtd.h"

#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

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
    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(filePath.c_str(), O_RDWR, errorNum);
    if (fd < 0) {
        return LinuxSysmanImp::getResult(errorNum);
    }

    erase_info_t eraseInfo;
    eraseInfo.start = offset;
    eraseInfo.length = static_cast<uint32_t>(size);

    int result = SysmanSysCallsWrapper::ioctl(fd, MEMERASE, &eraseInfo, errorNum);
    int savedErrno = errorNum;
    SysmanSysCallsWrapper::close(fd, errorNum);

    if (result < 0) {
        return LinuxSysmanImp::getResult(savedErrno);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryTechnologyDeviceInterface::write(const std::string &filePath, uint32_t offset, const uint8_t *data, size_t size) {
    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(filePath.c_str(), O_RDWR, errorNum);
    if (fd < 0) {
        return LinuxSysmanImp::getResult(errorNum);
    }

    // Seek to the specified offset
    if (SysmanSysCallsWrapper::lseek(fd, offset, SEEK_SET, errorNum) != static_cast<off_t>(offset)) {
        int savedErrno = errorNum;
        SysmanSysCallsWrapper::close(fd, errorNum);
        return LinuxSysmanImp::getResult(savedErrno);
    }

    // Write the data
    ssize_t bytesWritten = SysmanSysCallsWrapper::write(fd, data, size, errorNum);
    int savedErrno = errorNum;
    SysmanSysCallsWrapper::close(fd, errorNum);

    if (bytesWritten != static_cast<ssize_t>(size)) {
        return LinuxSysmanImp::getResult(savedErrno);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryTechnologyDeviceInterface::getDeviceInfo(const std::string &filePath, std::map<std::string, std::map<uint32_t, uint32_t>> &mtdRegionDeviceInfoMap) {
    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(filePath.c_str(), O_RDONLY, errorNum);
    if (fd < 0) {
        return LinuxSysmanImp::getResult(errorNum);
    }

    // Iterate over the mtdRegionToOffsetMap to get region information
    for (const auto &regionEntry : mtdRegionToOffsetMap) {
        const std::string &regionName = regionEntry.first;
        uint32_t spiOffset = 0x40 + (regionEntry.second * sizeof(uint32_t));

        // Read the SPI descriptor region data
        MtdSysman::SpiDescRegionBar value;
        if (SysmanSysCallsWrapper::lseek(fd, spiOffset, SEEK_SET, errorNum) != static_cast<off_t>(spiOffset)) {
            continue; // Skip this region if seek fails
        }

        ssize_t bytesRead = SysmanSysCallsWrapper::read(fd, &value, sizeof(value), errorNum);
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

    SysmanSysCallsWrapper::close(fd, errorNum);
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
