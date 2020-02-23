/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "opencl/source/built_ins/built_ins.h"

namespace NEO {
class Device;

const SipKernel &initSipKernel(SipKernelType type, Device &device);
Program *createProgramForSip(ExecutionEnvironment &executionEnvironment,
                             Context *context,
                             std::vector<char> &binary,
                             size_t size,
                             cl_int *errcodeRet,
                             Device *device);
} // namespace NEO
