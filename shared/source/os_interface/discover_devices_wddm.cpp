/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

std::vector<std::unique_ptr<HwDeviceId>> OSInterface::discoverDevices(ExecutionEnvironment &executionEnvironment) {
    return Wddm::discoverDevices(executionEnvironment);
}

} // namespace NEO
