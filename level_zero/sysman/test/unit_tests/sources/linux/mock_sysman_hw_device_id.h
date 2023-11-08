/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"

namespace L0 {
namespace Sysman {
namespace ult {

class MockSysmanHwDeviceIdDrm : public L0::Sysman::SysmanHwDeviceIdDrm {
  public:
    using L0::Sysman::SysmanHwDeviceIdDrm::SysmanHwDeviceIdDrm;
    int returnOpenFileDescriptor = 1;
    int returnCloseFileDescriptor = 0;

    int openFileDescriptor() override {
        return returnOpenFileDescriptor;
    }

    int closeFileDescriptor() override {
        return returnCloseFileDescriptor;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
