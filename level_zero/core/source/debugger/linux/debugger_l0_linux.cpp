/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {

void DebuggerL0::registerResourceClasses() {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        auto drm = device->getRootDeviceEnvironment().osInterface->get()->getDrm();
        drm->registerResourceClasses();
    }
}
} // namespace L0