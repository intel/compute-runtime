/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "test.h"

TEST(osInterfaceTests, osInterfaceLocalMemoryEnabledByDefault) {
    EXPECT_EQ(is64bit, OCLRT::OSInterface::osEnableLocalMemory);
}
