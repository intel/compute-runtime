/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm/wddm.h"

namespace NEO {
Wddm *Wddm::createWddm(RootDeviceEnvironment &rootDeviceEnvironment) {
    return new Wddm(rootDeviceEnvironment);
}
} // namespace NEO
