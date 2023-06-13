/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include <algorithm>

namespace NEO {

void ExecutionEnvironment::sortNeoDevicesWDDM() {
    std::sort(rootDeviceEnvironments.begin(), rootDeviceEnvironments.end(), comparePciIdBusNumber);
}

} // namespace NEO
