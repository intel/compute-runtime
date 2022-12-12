/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

int ProductHelper::configureHwInfoDrm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironemnt) {
    UNRECOVERABLE_IF(true);
    return {};
}

} // namespace NEO
