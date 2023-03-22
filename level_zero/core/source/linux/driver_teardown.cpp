/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/global_teardown.h"

using namespace L0;

void __attribute__((destructor)) driverHandleDestructor() {
    std::string loaderLibraryName = "lib" + L0::loaderLibraryFilename + ".so.1";
    loaderDriverTeardown(loaderLibraryName);
    globalDriverTeardown();
}