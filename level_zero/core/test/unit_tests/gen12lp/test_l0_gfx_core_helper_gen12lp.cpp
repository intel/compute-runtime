/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForSingleThreadWhenGettingThreadsThenSingleCorrectThreadReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllSubslicesWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForAllEUsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenBitmaskWithAttentionBitsForHalfOfThreadsWhenGettingThreadsThenCorrectThreadsAreReturned, IGFX_GEN12LP_CORE);

using L0GfxCoreHelperTestGen12Lp = Test<DeviceFixture>;

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGetRegsetTypeForLargeGrfDetectionIsCalledThenInvalidRegsetTypeIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU, l0GfxCoreHelper.getRegsetTypeForLargeGrfDetection());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGetGrfRegisterCountIsCalledThen128IsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(128u, l0GfxCoreHelper.getGrfRegisterCount(nullptr));
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForCmdListHeapSharingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsCmdListHeapSharing());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenCheckingL0HelperForCmdlistPrimaryBufferSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPrimaryBatchBufferCmdList());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGettingSupportedRTASFormatThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZE_RTAS_FORMAT_EXP_INVALID, l0GfxCoreHelper.getSupportedRTASFormat());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGetIpSamplingMetricCountIsCalledThenProperValueIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0u, l0GfxCoreHelper.getIpSamplingMetricCount());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGetStallSamplingReportMetricsThenEmptyListIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::vector<std::pair<const char *, const char *>> expectedStallSamplingReportList = {};
    EXPECT_EQ(expectedStallSamplingReportList, l0GfxCoreHelper.getStallSamplingReportMetrics());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenStallIpDataMapUpdateIsCalledThenFalseIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;
    EXPECT_FALSE(l0GfxCoreHelper.stallIpDataMapUpdate(stallSumIpDataMap, nullptr));
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenStallIpDataMapDeleteIsCalledThenMapisUnchanged) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;
    size_t mapSizeBefore = stallSumIpDataMap.size();
    l0GfxCoreHelper.stallIpDataMapDelete(stallSumIpDataMap);
    EXPECT_EQ(mapSizeBefore, stallSumIpDataMap.size());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenStallSumIpDataToTypedValuesIsCalledThenNoChangeToDataValues) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    uint64_t ip = 0ull;
    void *sumIpData = nullptr;
    std::vector<zet_typed_value_t> ipDataValues;
    l0GfxCoreHelper.stallSumIpDataToTypedValues(ip, sumIpData, ipDataValues);
    EXPECT_EQ(0u, ipDataValues.size());
}

GEN12LPTEST_F(L0GfxCoreHelperTestGen12Lp, GivenGen12LpWhenGetIpSamplingIpMaskIsCalledThenZeroIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0u, l0GfxCoreHelper.getIpSamplingIpMask());
}

} // namespace ult
} // namespace L0
