/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "test.h"

TEST(osInterfaceTests, osInterfaceLocalMemoryEnabledByDefault) {
    EXPECT_TRUE(NEO::OSInterface::osEnableLocalMemory);
}
