/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"

#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/sysman/source/device/sysman_hw_device_id.h"

#include <fcntl.h>

namespace L0 {
namespace Sysman {

int SysmanHwDeviceIdDrm::openFileDescriptor() {

    std::unique_lock<std::mutex> lock(fdMutex);
    if (fileDescriptor == -1) {
        fileDescriptor = NEO::SysCalls::open(devNodePath.c_str(), O_RDWR);
    }
    ++fdRefCounter;
    return fileDescriptor;
}

int SysmanHwDeviceIdDrm::closeFileDescriptor() {

    int closeStatus = 0;
    std::unique_lock<std::mutex> lock(fdMutex);

    DEBUG_BREAK_IF(fdRefCounter == 0u);
    if (fdRefCounter > 0u) {
        --fdRefCounter;
        if (fdRefCounter == 0u && fileDescriptor >= 0) {
            closeStatus = NEO::SysCalls::close(fileDescriptor);
            fileDescriptor = -1;
        }
    }
    return closeStatus;
}

std::unique_ptr<NEO::HwDeviceId> createSysmanHwDeviceId(std::unique_ptr<NEO::HwDeviceId> &hwDeviceId) {

    if (hwDeviceId->getDriverModelType() != NEO::DriverModelType::drm) {
        return std::move(hwDeviceId);
    }

    const auto hwDeviceIdDrm = static_cast<NEO::HwDeviceIdDrm *>(hwDeviceId.get());
    return std::make_unique<SysmanHwDeviceIdDrm>(hwDeviceIdDrm->getFileDescriptor(), hwDeviceIdDrm->getPciPath(), hwDeviceIdDrm->getDeviceNode());
}

} // namespace Sysman
} // namespace L0