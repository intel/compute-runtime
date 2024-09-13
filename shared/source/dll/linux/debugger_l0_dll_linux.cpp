/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
std::unique_ptr<NEO::Debugger> DebuggerL0::create(NEO::Device *device) {
    auto &hwInfo = device->getHardwareInfo();
    if (!hwInfo.capabilityTable.l0DebuggerSupported) {
        return nullptr;
    }
    auto osInterface = device->getRootDeviceEnvironment().osInterface.get();
    if (!osInterface || !osInterface->isDebugAttachAvailable()) {
        return nullptr;
    }

    auto success = initDebuggingInOs(device->getRootDeviceEnvironment().osInterface.get());
    if (success) {
        auto debugger = debuggerL0Factory[device->getHardwareInfo().platform.eRenderCoreFamily](device);
        return std::unique_ptr<DebuggerL0>(debugger);
    }
    return std::unique_ptr<DebuggerL0>(nullptr);
}

} // namespace NEO