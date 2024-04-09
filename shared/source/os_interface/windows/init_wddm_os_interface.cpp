/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/os_interface_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"

namespace NEO {

bool initWddmOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, RootDeviceEnvironment *rootDeviceEnv) {
    UNRECOVERABLE_IF(hwDeviceId->getDriverModelType() != DriverModelType::wddm);
    auto hwDeviceIdWddm = std::unique_ptr<HwDeviceIdWddm>(reinterpret_cast<HwDeviceIdWddm *>(hwDeviceId.release()));
    NEO::Wddm *wddm = Wddm::createWddm(std::move(hwDeviceIdWddm), *rootDeviceEnv);
    if (!wddm->init()) {
        return false;
    }

    const bool isCsrHwWithAub = obtainCsrTypeFromIntegerValue(debugManager.flags.SetCommandStreamReceiver.get(), CommandStreamReceiverType::hardware) == CommandStreamReceiverType::hardwareWithAub;
    rootDeviceEnv->memoryOperationsInterface = WddmMemoryOperationsHandler::create(wddm, rootDeviceEnv, isCsrHwWithAub);

    return true;
}
} // namespace NEO
