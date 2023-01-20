/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

namespace NEO {

TEST(OsInterfaceTest, GivenLinuxWhenCallingAre64kbPagesEnabledThenReturnFalse) {
    EXPECT_FALSE(OSInterface::are64kbPagesEnabled());
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenDeviceHandleQueriedThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    OSInterface osInterface;
    osInterface.setDriverModel(std::move(drm));
    EXPECT_EQ(0u, osInterface.getDriverModel()->getDeviceHandle());
}

TEST(OsInterfaceTest, GivenLinuxOsWhenCheckForNewResourceImplicitFlushSupportThenReturnTrue) {
    EXPECT_TRUE(OSInterface::newResourceImplicitFlush);
}

TEST(OsInterfaceTest, GivenLinuxOsWhenCheckForGpuIdleImplicitFlushSupportThenReturnFalse) {
    EXPECT_TRUE(OSInterface::gpuIdleImplicitFlush);
}

TEST(OsInterfaceTest, GivenLinuxOsInterfaceWhenCallingIsDebugAttachAvailableThenFalseIsReturned) {
    OSInterface osInterface;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    osInterface.setDriverModel(std::unique_ptr<DriverModel>(drm));
    EXPECT_FALSE(osInterface.isDebugAttachAvailable());
}

} // namespace NEO
