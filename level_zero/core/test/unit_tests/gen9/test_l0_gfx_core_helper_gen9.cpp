/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using L0GfxCoreHelperTestGen9 = Test<DeviceFixture>;

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenCheckingL0HelperForFrontEndTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenCheckingL0HelperForCmdlistPrimaryBufferSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPrimaryBatchBufferCmdList());
}

GEN9TEST_F(L0GfxCoreHelperTestGen9, GivenGen9WhenGettingSupportedRTASFormatThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZE_RTAS_FORMAT_EXP_INVALID, l0GfxCoreHelper.getSupportedRTASFormat());
}

} // namespace ult
} // namespace L0
