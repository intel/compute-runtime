/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/debugger/debugger_l0.h"
namespace L0 {
bool DebuggerL0::initDebuggingInOs(NEO::OSInterface *osInterface) {
    if (osInterface != nullptr) {
        auto drm = osInterface->get()->getDrm();
        if (drm->isVmBindAvailable() && drm->isPerContextVMRequired()) {
            drm->registerResourceClasses();
            return true;
        }
    }
    return false;
}

void DebuggerL0::registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        auto drm = device->getRootDeviceEnvironment().osInterface->get()->getDrm();
        auto handle = drm->registerResource(NEO::Drm::ResourceClass::Elf, debugData->vIsa, debugData->vIsaSize);
        static_cast<NEO::DrmAllocation *>(isaAllocation)->linkWithRegisteredHandle(handle);
    }
}
} // namespace L0