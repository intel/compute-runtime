/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/preemption.h"
#include "core/helpers/hw_helper.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "unit_tests/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(OsInterfaceTest, GivenLinuxWhenare64kbPagesEnabledThenFalse) {
    EXPECT_FALSE(OSInterface::are64kbPagesEnabled());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenDeviceHandleQueriedthenZeroIsReturned) {
    OSInterface osInterface;
    EXPECT_EQ(0u, osInterface.getDeviceHandle());
}
} // namespace NEO
