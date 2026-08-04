/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"

namespace L0 {
namespace Sysman {
namespace ult {

// Minimal HwDeviceId stub that carries no OS-specific handle.
// createSysmanHwDeviceId() returns nullptr for the 'unknown' driver-model type,
// which exercises the initStatus=false / continue path in
// SysmanDriverImp::discoverAndInitializeDevices().
struct MockHwDeviceId : public NEO::HwDeviceId {
    MockHwDeviceId() : NEO::HwDeviceId(NEO::DriverModelType::unknown) {}
};

} // namespace ult
} // namespace Sysman
} // namespace L0
