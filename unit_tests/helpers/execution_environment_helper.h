/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/execution_environment/execution_environment.h"

#include "CL/cl.h"

#include <cstdint>

namespace OCLRT {
ExecutionEnvironment *getExecutionEnvironmentImpl(HardwareInfo *&hwInfo);
} // namespace OCLRT
