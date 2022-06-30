/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"

namespace NEO {

void ExecutionEnvironment::sortNeoDevices() {
    return ExecutionEnvironment::sortNeoDevicesDRM();
}

} // namespace NEO
