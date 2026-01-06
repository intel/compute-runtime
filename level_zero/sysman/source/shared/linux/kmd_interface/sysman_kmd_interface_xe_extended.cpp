/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"

namespace L0 {
namespace Sysman {

bool SysmanKmdInterfaceXe::isDeviceInFdoMode() {
    return false;
}

} // namespace Sysman
} // namespace L0
