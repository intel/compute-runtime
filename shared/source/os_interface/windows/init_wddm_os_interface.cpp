/*
 * Copyright (C) 2021 Intel Corporation
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

bool initWddmOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex,
                         RootDeviceEnvironment *rootDeviceEnv,
                         std::unique_ptr<OSInterface> &dstOsInterface, std::unique_ptr<MemoryOperationsHandler> &dstMemoryOpsHandler) {
    UNRECOVERABLE_IF(hwDeviceId->getDriverModelType() != DriverModelType::WDDM);
    auto hwDeviceIdWddm = std::unique_ptr<HwDeviceIdWddm>(reinterpret_cast<HwDeviceIdWddm *>(hwDeviceId.release()));
    NEO::Wddm *wddm = Wddm::createWddm(std::move(hwDeviceIdWddm), *rootDeviceEnv);
    if (!wddm->init()) {
        delete wddm;
        return false;
    }
    dstMemoryOpsHandler = std::make_unique<WddmMemoryOperationsHandler>(wddm);
    return true;
}
} // namespace NEO
