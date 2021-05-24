/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

std::vector<std::unique_ptr<HwDeviceId>> OSInterface::discoverDevicesDrm(ExecutionEnvironment &executionEnvironment) {
    UNRECOVERABLE_IF(true);
    return {};
}

} // namespace NEO
