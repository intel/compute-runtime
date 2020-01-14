/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/execution_environment/execution_environment.h"

namespace NEO {
class ClDevice;

const SipKernel &initSipKernel(SipKernelType type, ClDevice &device);
Program *createProgramForSip(ExecutionEnvironment &executionEnvironment,
                             Context *context,
                             std::vector<char> &binary,
                             size_t size,
                             cl_int *errcodeRet);
} // namespace NEO
