/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/built_ins/built_ins.h"
#include "runtime/execution_environment/execution_environment.h"

namespace OCLRT {

const SipKernel &initSipKernel(SipKernelType type, Device &device);
Program *createProgramForSip(ExecutionEnvironment &executionEnvironment,
                             Context *context,
                             std::vector<char> &binary,
                             size_t size,
                             cl_int *errcodeRet);
} // namespace OCLRT
