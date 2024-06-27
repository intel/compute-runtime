/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"

namespace L0 {
namespace Sysman {

uint64_t SysmanKmdInterfaceXe::getPmuEngineConfig(zes_engine_group_t engineGroup, uint32_t engineInstance, uint32_t subDeviceId) {
    return UINT64_MAX;
}

} // namespace Sysman
} // namespace L0