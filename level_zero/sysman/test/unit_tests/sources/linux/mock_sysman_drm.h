/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"

namespace L0 {
namespace Sysman {
namespace ult {

constexpr int mockFd = -1;
class SysmanMockDrm : public NEO::Drm {
  public:
    SysmanMockDrm(NEO::RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<L0::Sysman::SysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {
        setupIoctlHelper(rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily);
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
