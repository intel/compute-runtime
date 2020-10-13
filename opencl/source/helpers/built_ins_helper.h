/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/program/program_info.h"

namespace NEO {
class Device;

const SipKernel &initSipKernel(SipKernelType type, Device &device);
ProgramInfo createProgramInfoForSip(std::vector<char> &binary, size_t size, const Device &device);
} // namespace NEO
