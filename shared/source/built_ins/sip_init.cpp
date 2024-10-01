/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"

namespace NEO {

bool SipKernel::initSipKernel(SipKernelType type, Device &device) {
    return SipKernel::initSipKernelImpl(type, device, nullptr);
}

const SipKernel &SipKernel::getSipKernel(Device &device, OsContext *context) {
    if (context && device.getExecutionEnvironment()->getDebuggingMode() == NEO::DebuggingMode::offline) {
        return SipKernel::getDebugSipKernel(device, context);
    } else {
        return SipKernel::getSipKernelImpl(device);
    }
}

} // namespace NEO
