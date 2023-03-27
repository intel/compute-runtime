/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"

namespace NEO {

bool SipKernel::initSipKernel(SipKernelType type, Device &device) {
    return SipKernel::initSipKernelImpl(type, device, nullptr);
}

const SipKernel &SipKernel::getSipKernel(Device &device) {
    return SipKernel::getSipKernelImpl(device);
}

} // namespace NEO
