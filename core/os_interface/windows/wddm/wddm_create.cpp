/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/windows/wddm/wddm.h"

namespace NEO {
Wddm *Wddm::createWddm(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    return new Wddm(std::move(hwDeviceId), rootDeviceEnvironment);
}
} // namespace NEO
