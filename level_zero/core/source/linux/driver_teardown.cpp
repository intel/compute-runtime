/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/sysman/source/sysman_driver_handle_imp.h"

using namespace L0;

void __attribute__((destructor)) driverHandleDestructor() {
    if (GlobalDriver != nullptr) {
        delete GlobalDriver;
        GlobalDriver = nullptr;
    }
    if (Sysman::GlobalSysmanDriver != nullptr) {
        delete Sysman::GlobalSysmanDriver;
        Sysman::GlobalSysmanDriver = nullptr;
    }
}