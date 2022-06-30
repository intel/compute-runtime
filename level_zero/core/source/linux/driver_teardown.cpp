/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle_imp.h"

using namespace L0;

void __attribute__((destructor)) driverHandleDestructor() {
    if (GlobalDriver != nullptr) {
        delete GlobalDriver;
        GlobalDriver = nullptr;
    }
}