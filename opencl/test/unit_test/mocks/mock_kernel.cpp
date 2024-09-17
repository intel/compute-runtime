/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/program/printf_handler.h"

namespace NEO {

const uint32_t MockDebugKernel::perThreadSystemThreadSurfaceSize = 0x100;

const KernelInfoContainer MockKernel::toKernelInfoContainer(const KernelInfo &kernelInfo, uint32_t rootDeviceIndex) {
    KernelInfoContainer kernelInfos;
    kernelInfos.resize(rootDeviceIndex + 1);
    kernelInfos[rootDeviceIndex] = &kernelInfo;
    return kernelInfos;
}

bool MockKernel::isPatched() const {
    return isPatchedOverride;
}

void MockKernel::makeResident(CommandStreamReceiver &commandStreamReceiver) {
    makeResidentCalls++;
    Kernel::makeResident(commandStreamReceiver);
}

void MockKernel::getResidency(std::vector<Surface *> &dst) {
    getResidencyCalls++;
    Kernel::getResidency(dst);
}

cl_int MockKernel::setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc, uint32_t allocId) {
    ++setArgSvmAllocCalls;
    return Kernel::setArgSvmAlloc(argIndex, svmPtr, svmAlloc, allocId);
}
} // namespace NEO
