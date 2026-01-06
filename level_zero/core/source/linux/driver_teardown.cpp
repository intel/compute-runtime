/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/global_teardown.h"

namespace L0 {

void __attribute__((constructor)) driverHandleConstructor() {
    globalDriverSetup();
}
void __attribute__((destructor)) driverHandleDestructor() {
    globalDriverTeardown();
}
} // namespace L0
