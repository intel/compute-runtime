/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/linux/sysman_hw_device_id_linux.h"

#pragma once

namespace L0 {
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
} // namespace L0
