/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"
#include "core/os_interface/windows/os_interface.h"
#include "test.h"

TEST(osInterfaceTests, osInterfaceLocalMemoryEnabledByDefault) {
    EXPECT_TRUE(NEO::OSInterface::osEnableLocalMemory);
}
