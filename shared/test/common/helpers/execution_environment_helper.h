/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include <cstdint>

namespace NEO {
struct HardwareInfo;
ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo, uint32_t rootDeviceEnvironments);
} // namespace NEO
