/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "hw_cmds_xe3_core.h"

namespace L0 {
namespace ult {

using L0GfxCoreHelperTestXe3 = Test<DeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenAskingForImageCompressionSupportThenReturnFalse, IGFX_XE3_CORE);

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, givenL0GfxCoreHelperWhenAskingForImageCompressionSupportThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    HardwareInfo hwInfo = *NEO::defaultHwInfo;

    hwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(l0GfxCoreHelper.imageCompressionSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(l0GfxCoreHelper.imageCompressionSupported(hwInfo));

    NEO::debugManager.flags.RenderCompressedImagesEnabled.set(1);
    EXPECT_TRUE(l0GfxCoreHelper.imageCompressionSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedImages = true;
    NEO::debugManager.flags.RenderCompressedImagesEnabled.set(0);
    EXPECT_FALSE(l0GfxCoreHelper.imageCompressionSupported(hwInfo));
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, givenL0GfxCoreHelperWhenAskingForUsmCompressionSupportThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_TRUE(l0GfxCoreHelper.forceDefaultUsmCompressionSupport());

    HardwareInfo hwInfo = *NEO::defaultHwInfo;

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    EXPECT_TRUE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    EXPECT_FALSE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));

    NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    EXPECT_TRUE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    NEO::debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    EXPECT_FALSE(l0GfxCoreHelper.usmCompressionSupported(hwInfo));
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3CoreWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3CoreWhenCheckingL0HelperForCmdlistPrimaryBufferSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPrimaryBatchBufferCmdList());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenCheckingL0HelperForPlatformSupportsImmediateFlushTaskThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3CoreWhenGettingSupportedRTASFormatExpThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version2, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExp()));
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3CoreWhenGettingSupportedRTASFormatExtThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version2, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExt()));
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenGettingCmdlistUpdateCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(127u, l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenGettingRecordReplayGraphCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(1u, l0GfxCoreHelper.getPlatformRecordReplayGraphCapabilities());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenGetRegsetTypeForLargeGrfDetectionIsCalledThenSrRegsetTypeIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, l0GfxCoreHelper.getRegsetTypeForLargeGrfDetection());
}

XE3_CORETEST_F(L0GfxCoreHelperTestXe3, GivenXe3WhenGetGrfRegisterCountIsCalledThenCorrectMaskIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::vector<uint32_t> val{0, 0, 0, 0, 0, 0, 0, 0};
    val[4] = 0xFFFFFFFF;
    constexpr uint32_t expectedMask = 0x1FF;
    EXPECT_EQ(expectedMask, l0GfxCoreHelper.getGrfRegisterCount(val.data()));
}

} // namespace ult
} // namespace L0
