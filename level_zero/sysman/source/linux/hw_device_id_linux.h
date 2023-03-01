/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/hw_device_id.h"

#include <mutex>

namespace L0 {
namespace Sysman {
class SysmanHwDeviceIdDrm : public NEO::HwDeviceIdDrm {

  public:
    using NEO::HwDeviceIdDrm::HwDeviceIdDrm;
    SysmanHwDeviceIdDrm() = delete;
    int openFileDescriptor();
    int closeFileDescriptor();

  private:
    std::mutex fdMutex{};
    uint32_t fdRefCounter = 0;
};

} // namespace Sysman
} // namespace L0