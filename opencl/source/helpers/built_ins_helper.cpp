/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/built_ins_helper.h"

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/device_binary_formats.h"

namespace NEO {
void initSipKernel(SipKernelType type, Device &device) {
    device.getBuiltIns()->getSipKernel(type, device);
}
} // namespace NEO
