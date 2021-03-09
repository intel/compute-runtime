/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/multi_device_kernel.h"
namespace NEO {

MultiDeviceKernel::~MultiDeviceKernel() {
    kernel->decRefInternal();
}
MultiDeviceKernel::MultiDeviceKernel(Kernel *pKernel) : kernel(pKernel) {
    pKernel->incRefInternal();
    pKernel->setMultiDeviceKernel(this);
};

} // namespace NEO
