/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler_with_aub_dump.h"

namespace NEO {

std::unique_ptr<WddmMemoryOperationsHandler> WddmMemoryOperationsHandler::create(Wddm *wddm, RootDeviceEnvironment *rootDeviceEnvironment, bool withAubDump) {
    if (withAubDump) {
        return std::make_unique<WddmMemoryOperationsHandlerWithAubDump<WddmMemoryOperationsHandler>>(wddm, *rootDeviceEnvironment);
    } else {
        return std::make_unique<WddmMemoryOperationsHandler>(wddm);
    }
}

} // namespace NEO
