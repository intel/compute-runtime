/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

bool SipKernel::initSipKernel(SipKernelType type, Device &device) {
    return SipKernel::initSipKernelImpl(type, device, nullptr);
}

void SipKernel::freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager) {
    for (auto &sipKernel : rootDeviceEnvironment->sipKernels) {
        if (sipKernel.get()) {
            memoryManager->freeGraphicsMemory(sipKernel->getSipAllocation());
            sipKernel.reset();
        }
    }
}

const SipKernel &SipKernel::getSipKernel(Device &device, OsContext *context) {
    if (context && device.getExecutionEnvironment()->getDebuggingMode() == NEO::DebuggingMode::offline) {
        return SipKernel::getDebugSipKernel(device, context);
    } else {
        return SipKernel::getSipKernelImpl(device);
    }
}

} // namespace NEO
