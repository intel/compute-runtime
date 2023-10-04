/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/xe/ioctl_helper_xe_tests.h"

using namespace NEO;

TEST(IoctlHelperXeTest, whenGetFdFromVmExportIsCalledThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockXe drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXe>(drm);
    uint32_t vmId = 0, flags = 0;
    int32_t fd = 0;
    EXPECT_FALSE(xeIoctlHelper->getFdFromVmExport(vmId, flags, &fd));
}
