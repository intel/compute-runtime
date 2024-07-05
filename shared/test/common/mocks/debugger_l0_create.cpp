/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

NEO::DebugerL0CreateFn mockDebuggerL0HwFactory[IGFX_MAX_CORE];
bool forceCreateNullptrDebugger = false;
size_t createDebuggerCallCount = 0;

namespace NEO {

std::unique_ptr<Debugger> DebuggerL0::create(NEO::Device *device) {
    createDebuggerCallCount++;
    if (forceCreateNullptrDebugger) {
        return nullptr;
    }
    initDebuggingInOs(device->getRootDeviceEnvironment().osInterface.get());
    auto debugger = mockDebuggerL0HwFactory[device->getHardwareInfo().platform.eRenderCoreFamily](device);
    return std::unique_ptr<DebuggerL0>(debugger);
}

} // namespace NEO