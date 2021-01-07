/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"

namespace NEO {
class Device;

void initSipKernel(SipKernelType type, Device &device);

} // namespace NEO
