/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

const size_t SipKernel::maxDbgSurfaceSize = 0x1800000; // proper value should be taken from compiler when it's ready

SipKernel::~SipKernel() = default;

SipKernel::SipKernel(SipKernelType type, GraphicsAllocation *sipAlloc) : type(type), sipAllocation(sipAlloc) {
}

GraphicsAllocation *SipKernel::getSipAllocation() const {
    return sipAllocation;
}

SipKernelType SipKernel::getSipKernelType(GFXCORE_FAMILY family, bool debuggingActive) {
    auto &hwHelper = HwHelper::get(family);
    return hwHelper.getSipKernelType(debuggingActive);
}

GraphicsAllocation *SipKernel::getSipKernelAllocation(Device &device) {
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();
    auto sipType = SipKernel::getSipKernelType(device.getHardwareInfo().platform.eRenderCoreFamily, debuggingEnabled);
    return device.getBuiltIns()->getSipKernel(sipType, device).getSipAllocation();
}
} // namespace NEO
