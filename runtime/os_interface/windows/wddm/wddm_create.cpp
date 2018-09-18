/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

namespace OCLRT {
Wddm *Wddm::createWddm() {
    return new Wddm();
}
} // namespace OCLRT
