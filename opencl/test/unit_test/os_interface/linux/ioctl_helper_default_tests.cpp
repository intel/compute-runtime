/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "test.h"

using namespace NEO;

TEST(IoctlHelperTestsDefault, givenUnsupportedPlatformWhenCreateGemExtThenReturnErrorNumber) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto ioctlHelper = IoctlHelper::get(IGFX_UNKNOWN);
    uint32_t handle = 0;
    auto ret = ioctlHelper->createGemExt(drm.get(), &regionInfo[1], 1, 1024, handle);

    EXPECT_EQ(-1u, ret);
}

TEST(IoctlHelperTestsDefault, givenUnsupportedPlatformWhenTranslateIfRequiredReturnSameData) {
    auto *data = new uint8_t{};
    auto ioctlHelper = IoctlHelper::get(IGFX_UNKNOWN);
    auto ret = ioctlHelper->translateIfRequired(data, 1);
    EXPECT_EQ(ret.get(), data);
}
