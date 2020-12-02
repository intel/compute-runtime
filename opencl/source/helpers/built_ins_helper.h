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

void initSipKernel(SipKernelType type, Device &device);

} // namespace NEO
