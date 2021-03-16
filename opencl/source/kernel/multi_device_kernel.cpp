/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/multi_device_kernel.h"
namespace NEO {

MultiDeviceKernel::~MultiDeviceKernel() {
    for (auto &pKernel : kernels) {
        if (pKernel) {
            pKernel->decRefInternal();
        }
    }
}
MultiDeviceKernel::MultiDeviceKernel(KernelVectorType kernelVector) : kernels(std::move(kernelVector)) {
    for (auto &pKernel : kernels) {
        if (pKernel) {
            if (!defaultKernel) {
                defaultKernel = kernels[(*pKernel->getDevices().begin())->getRootDeviceIndex()];
            }
            pKernel->incRefInternal();
            pKernel->setMultiDeviceKernel(this);
        }
    }
};

} // namespace NEO
