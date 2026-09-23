/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"

namespace NEO {

struct MockHardwareInfoSetup {
    MockHardwareInfoSetup()
        : device{0, hwInfo.platform.eProductFamily},
          hwInfoBackup(&hardwareInfoTable[hwInfo.platform.eProductFamily], static_cast<const HardwareInfo *>(&hwInfo)),
          setupBackup(&hardwareInfoSetup[hwInfo.platform.eProductFamily], +[](HardwareInfo *, bool) {}) {}

    HardwareInfo hwInfo = *defaultHwInfo;
    DeviceDescriptor device;
    VariableBackup<const HardwareInfo *> hwInfoBackup;
    VariableBackup<void (*)(HardwareInfo *, bool)> setupBackup;
};

} // namespace NEO
