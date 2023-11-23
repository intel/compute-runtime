/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

namespace L0 {

void globalDriverTeardown() {
    if (globalDriver != nullptr) {
        delete globalDriver;
        globalDriver = nullptr;
    }
    if (Sysman::globalSysmanDriver != nullptr) {
        delete Sysman::globalSysmanDriver;
        Sysman::globalSysmanDriver = nullptr;
    }
}
} // namespace L0
