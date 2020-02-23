/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include "CL/cl.h"

#include <cstdint>

namespace NEO {
ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo, uint32_t rootDeviceEnvironments);
} // namespace NEO
