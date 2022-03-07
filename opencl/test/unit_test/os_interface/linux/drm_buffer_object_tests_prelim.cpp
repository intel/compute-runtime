/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/os_interface/linux/drm_buffer_object_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_prelim.h"

using namespace NEO;

using DrmBufferObjectPrelimFixture = DrmBufferObjectFixture<DrmMockCustomPrelim>;
using DrmBufferObjectPrelimTest = Test<DrmBufferObjectPrelimFixture>;

TEST_F(DrmBufferObjectPrelimTest, GivenCompletionAddressWhenCallingExecThenReturnIsCorrect) {
    mock->ioctl_expected.total = 1;
    mock->ioctl_res = 0;

    constexpr uint64_t completionAddress = 0x1230;
    constexpr uint32_t completionValue = 33;
    constexpr uint64_t expectedCompletionValue = completionValue;

    drm_i915_gem_exec_object2 execObjectsStorage = {};
    auto ret = bo->exec(0, 0, 0, false, osContext.get(), 0, 1, nullptr, 0u, &execObjectsStorage, completionAddress, completionValue);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(completionAddress, mock->context.completionAddress);
    EXPECT_EQ(expectedCompletionValue, mock->context.completionValue);
}
