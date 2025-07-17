/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_XE_HPG_CORE);

using L0GfxCoreHelperTestXeHpg = Test<DeviceFixture>;

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenGetRegsetTypeForLargeGrfDetectionIsCalledThenCrRegsetTypeIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_CR_INTEL_GPU, l0GfxCoreHelper.getRegsetTypeForLargeGrfDetection());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgcWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForCmdlistPrimaryBufferSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPrimaryBatchBufferCmdList());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenCheckingL0HelperForPlatformSupportsImmediateFlushTaskThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenGettingSupportedRTASFormatThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version1, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormat()));
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenGettingCmdlistUpdateCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(62u, l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenGetIpSamplingMetricCountIsCalledThenProperValueIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0u, l0GfxCoreHelper.getIpSamplingMetricCount());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenGetStallSamplingReportMetricsThenEmptyListIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::vector<std::pair<const char *, const char *>> expectedStallSamplingReportList = {};
    EXPECT_EQ(expectedStallSamplingReportList, l0GfxCoreHelper.getStallSamplingReportMetrics());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenStallIpDataMapUpdateIsCalledThenFalseIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;
    EXPECT_FALSE(l0GfxCoreHelper.stallIpDataMapUpdate(stallSumIpDataMap, nullptr));
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenStallIpDataMapDeleteIsCalledThenMapisUnchanged) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;
    size_t mapSizeBefore = stallSumIpDataMap.size();
    l0GfxCoreHelper.stallIpDataMapDelete(stallSumIpDataMap);
    EXPECT_EQ(mapSizeBefore, stallSumIpDataMap.size());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenStallSumIpDataToTypedValuesIsCalledThenNoChangeToDataValues) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    uint64_t ip = 0ull;
    void *sumIpData = nullptr;
    std::vector<zet_typed_value_t> ipDataValues;
    l0GfxCoreHelper.stallSumIpDataToTypedValues(ip, sumIpData, ipDataValues);
    EXPECT_EQ(0u, ipDataValues.size());
}

XE_HPG_CORETEST_F(L0GfxCoreHelperTestXeHpg, GivenXeHpgWhenGetIpSamplingIpMaskIsCalledThenZeroIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0u, l0GfxCoreHelper.getIpSamplingIpMask());
}

} // namespace ult
} // namespace L0
