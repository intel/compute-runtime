/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm/wddm.h"

namespace NEO {
bool Wddm::configureDeviceAddressSpace() {
    return configureDeviceAddressSpaceImpl();
}

} // namespace NEO
