/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"

namespace NEO {
std::unique_ptr<NEO::Debugger> DebuggerL0::create(NEO::Device *device) {
    auto &hwInfo = device->getHardwareInfo();
    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        return std::unique_ptr<DebuggerL0>(nullptr);
    }
    auto success = initDebuggingInOs(device->getRootDeviceEnvironment().osInterface.get());
    if (success) {
        auto debugger = debuggerL0Factory[device->getHardwareInfo().platform.eRenderCoreFamily](device);
        return std::unique_ptr<DebuggerL0>(debugger);
    }
    return std::unique_ptr<DebuggerL0>(nullptr);
}
} // namespace NEO