/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {
std::unique_ptr<NEO::Debugger> DebuggerL0::create(NEO::Device *device) {
    initDebuggingInOs(device->getRootDeviceEnvironment().osInterface.get());
    auto debugger = debuggerL0Factory[device->getHardwareInfo().platform.eRenderCoreFamily](device);
    return std::unique_ptr<DebuggerL0>(debugger);
}

} // namespace L0