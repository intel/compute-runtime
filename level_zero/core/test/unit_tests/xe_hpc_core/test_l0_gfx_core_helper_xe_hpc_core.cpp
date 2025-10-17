/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using L0GfxCoreHelperTestXeHpc = Test<DeviceFixture>;

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenHpcPlatformsWhenAlwaysAllocateEventInLocalMemCalledThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.alwaysAllocateEventInLocalMem());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForCmdlistPrimaryBufferSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPrimaryBatchBufferCmdList());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForPlatformSupportsImmediateFlushTaskThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenGetRegsetTypeForLargeGrfDetectionIsCalledThenCrRegsetTypeIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, l0GfxCoreHelper.getRegsetTypeForLargeGrfDetection());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenGettingSupportedRTASFormatExpThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version1, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExp()));
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenGettingSupportedRTASFormatExtThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version1, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExt()));
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenGettingCmdlistUpdateCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(127u, l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenGettingRecordReplayGraphCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0u, l0GfxCoreHelper.getPlatformRecordReplayGraphCapabilities());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForDeletingIpSamplingEntryWithNullValuesThenMapRemainstheSameSize) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;
    stallSumIpDataMap.emplace(std::pair<uint64_t, void *>(0ull, nullptr));
    l0GfxCoreHelper.stallIpDataMapDelete(stallSumIpDataMap);
    EXPECT_NE(0u, stallSumIpDataMap.size());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForGetIpSamplingIpMaskThenCorrectValueIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0x1fffffffull, l0GfxCoreHelper.getIpSamplingIpMask());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenXeHpcWhenCheckingL0HelperForGetOaTimestampValidBitsThenCorrectValueIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(32u, l0GfxCoreHelper.getOaTimestampValidBits());
}

XE_HPC_CORETEST_F(L0GfxCoreHelperTestXeHpc, GivenL0GfxCoreHelperWhenCheckingMetricsAggregationSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.supportMetricsAggregation());
}

} // namespace ult
} // namespace L0
