/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/execution_environment/execution_environment.h"

namespace NEO {
class Device;

const SipKernel &initSipKernel(SipKernelType type, Device &device);
Program *createProgramForSip(ExecutionEnvironment &executionEnvironment,
                             Context *context,
                             std::vector<char> &binary,
                             size_t size,
                             int *errcodeRet,
                             Device *device);
} // namespace NEO
