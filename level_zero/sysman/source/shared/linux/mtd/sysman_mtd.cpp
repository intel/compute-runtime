/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/mtd/sysman_mtd.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <cstring>
#include <sys/ioctl.h>

namespace L0 {
namespace Sysman {

std::unique_ptr<MemoryTechnologyDeviceInterface> MemoryTechnologyDeviceInterface::create() {
    return std::make_unique<MemoryTechnologyDeviceInterface>();
}

ze_result_t MemoryTechnologyDeviceInterface::erase(const std::string &filePath, uint32_t offset, size_t size) {
    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(filePath.c_str(), O_RDWR, errorNum);
    if (fd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Could not open %s for erase, errno %d (%s)\n", filePath.c_str(), errorNum, strerror(errorNum));
        return LinuxSysmanImp::getResult(errorNum);
    }

    erase_info_t eraseInfo;
    eraseInfo.start = offset;
    eraseInfo.length = static_cast<uint32_t>(size);

    int result = SysmanSysCallsWrapper::ioctl(fd, MEMERASE, &eraseInfo, errorNum);
    int savedErrno = errorNum;
    SysmanSysCallsWrapper::close(fd, errorNum);

    if (result < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Could not erase %zu bytes of %s at offset 0x%x, errno %d (%s)\n", size, filePath.c_str(), offset, savedErrno, strerror(savedErrno));
        return LinuxSysmanImp::getResult(savedErrno);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MemoryTechnologyDeviceInterface::write(const std::string &filePath, uint32_t offset, const uint8_t *data, size_t size) {
    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(filePath.c_str(), O_RDWR, errorNum);
    if (fd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Could not open %s for write, errno %d (%s)\n", filePath.c_str(), errorNum, strerror(errorNum));
        return LinuxSysmanImp::getResult(errorNum);
    }

    if (SysmanSysCallsWrapper::lseek(fd, offset, SEEK_SET, errorNum) != static_cast<off_t>(offset)) {
        int savedErrno = errorNum;
        SysmanSysCallsWrapper::close(fd, errorNum);
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Could not seek %s to offset 0x%x, errno %d (%s)\n", filePath.c_str(), offset, savedErrno, strerror(savedErrno));
        return LinuxSysmanImp::getResult(savedErrno);
    }

    ssize_t bytesWritten = SysmanSysCallsWrapper::write(fd, data, size, errorNum);
    int savedErrno = errorNum;

    SysmanSysCallsWrapper::close(fd, errorNum);

    if (bytesWritten != static_cast<ssize_t>(size)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Wrote %zd of %zu bytes to %s at offset 0x%x, errno %d (%s)\n", bytesWritten, size, filePath.c_str(), offset, savedErrno, strerror(savedErrno));
        return LinuxSysmanImp::getResult(savedErrno);
    }

    SysmanSysCallsWrapper::sync();
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
