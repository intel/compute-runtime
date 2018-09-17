/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"
#include <cstdint>
#include "runtime/execution_environment/execution_environment.h"

namespace OCLRT {
ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo);
} // namespace OCLRT
