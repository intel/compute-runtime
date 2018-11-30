/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/os_interface.h"
#include "test.h"

TEST(osInterfaceTests, osInterfaceLocalMemoryEnabledByDefault) {
    EXPECT_TRUE(OCLRT::OSInterface::osEnableLocalMemory);
}
