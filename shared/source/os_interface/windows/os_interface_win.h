/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"

#include <cstdint>
#include <memory>

namespace NEO {

bool initWddmOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex,
                         RootDeviceEnvironment *rootDeviceEnv,
                         std::unique_ptr<OSInterface> &dstOsInterface, std::unique_ptr<MemoryOperationsHandler> &dstMemoryOpsHandler);

} // namespace NEO
