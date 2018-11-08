/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "unit_tests/os_interface/linux/drm_mock.h"
#include "gtest/gtest.h"

namespace OCLRT {

TEST(OsInterfaceTest, GivenLinuxWhenare64kbPagesEnabledThenFalse) {
    EXPECT_FALSE(OSInterface::are64kbPagesEnabled());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenDeviceHandleQueriedthenZeroIsReturned) {
    OSInterface osInterface;
    EXPECT_EQ(0u, osInterface.getDeviceHandle());
}

TEST(OsContextTest, givenDrmWhenOsContextIsCreatedThenImplIsAvailable) {
    DrmMock drmMock;
    OSInterface osInterface;
    osInterface.get()->setDrm(&drmMock);

    auto osContext = std::make_unique<OsContext>(&osInterface, 0u);
    EXPECT_NE(nullptr, osContext->get());
}
} // namespace OCLRT
