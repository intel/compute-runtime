/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using L0GfxCoreHelperTestXe2Hpg = Test<DeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenAskingForImageCompressionSupportThenReturnFalse, IGFX_XE2_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, whenAlwaysAllocateEventInLocalMemCalledThenReturnFalse_IsNotXeHpcCore, IGFX_XE2_HPG_CORE);

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, givenL0GfxCoreHelperWhenGetRegsetTypeForLargeGrfDetectionIsCalledThenSrRegsetTypeIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, l0GfxCoreHelper.getRegsetTypeForLargeGrfDetection());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, whenAlwaysAllocateEventInLocalMemCalledThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.alwaysAllocateEventInLocalMem());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, givenL0GfxCoreHelperWhenAskingForImageCompressionSupportThenReturnCorrectValue) {
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

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, givenL0GfxCoreHelperWhenAskingForUsmCompressionSupportThenReturnCorrectValue) {
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

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForPlatformSupportsImmediateFlushTaskThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenGettingSupportedRTASFormatExpThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version1, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExp()));
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenGettingSupportedRTASFormatExtThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version1, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExt()));
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenGettingCmdlistUpdateCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(127u, l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenGettingRecordReplayGraphCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(1u, l0GfxCoreHelper.getPlatformRecordReplayGraphCapabilities());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCallingThreadResumeRequiresUnlockThenReturnFalse) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.threadResumeRequiresUnlock());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe3pWhenCallingisThreadControlStoppedSupportedThenReturnTrue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.isThreadControlStoppedSupported());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForDeletingIpSamplingMapWithNullValuesThenMapRemainstheSameSize) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;
    stallSumIpDataMap.emplace(std::pair<uint64_t, void *>(0ull, nullptr));
    l0GfxCoreHelper.stallIpDataMapDeleteSumData(stallSumIpDataMap);
    EXPECT_NE(0u, stallSumIpDataMap.size());
}

#pragma pack(1)
typedef struct StallSumIpDataXeCore {
    uint64_t tdrCount;
    uint64_t otherCount;
    uint64_t controlCount;
    uint64_t pipeStallCount;
    uint64_t sendCount;
    uint64_t distAccCount;
    uint64_t sbidCount;
    uint64_t syncCount;
    uint64_t instFetchCount;
    uint64_t activeCount;
} StallSumIpDataXeCore_t;
#pragma pack()

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForDeletingIpSamplingEntryWithThenMapRemainstheSameSize) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;

    StallSumIpDataXeCore *stallSumData = new StallSumIpDataXeCore;
    stallSumIpDataMap.emplace(std::pair<uint64_t, void *>(0ull, stallSumData));
    std::map<uint64_t, void *>::iterator it = stallSumIpDataMap.begin();
    l0GfxCoreHelper.stallIpDataMapDeleteSumDataEntry(it);
    EXPECT_EQ(1u, stallSumIpDataMap.size());

    l0GfxCoreHelper.stallIpDataMapDeleteSumDataEntry(it); // if entry not found it is skipped
    EXPECT_EQ(1u, stallSumIpDataMap.size());
    stallSumIpDataMap.clear();
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenL0HelperCanAddIPsFromDataThenSuccess) {

    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::map<uint64_t, void *> stallSumIpDataMap;

    // Raw reports are 64Bytes, 8 x uint64_t
    std::array<uint64_t, 8> ipData = {
        0x0000000000000001,
        0x0000000000000002,
        0x0000000000000003,
        0x0000000000000004,
        0x0000000000000005,
        0x0000000000000006,
        0x0000000000000007,
        0x0000000000000008};
    uint8_t *data = reinterpret_cast<uint8_t *>(ipData.data());
    // Call for new IP
    l0GfxCoreHelper.stallIpDataMapUpdateFromData(data, stallSumIpDataMap);
    // Call for repeated IP
    l0GfxCoreHelper.stallIpDataMapUpdateFromData(data, stallSumIpDataMap);

    // Delete the sumData
    l0GfxCoreHelper.stallIpDataMapDeleteSumData(stallSumIpDataMap);
    stallSumIpDataMap.clear();
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenL0HelperCanAddIPsFromMapThenSuccess) {

    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();

    std::map<uint64_t, void *> stallSourceIpDataMap;
    StallSumIpDataXeCore_t *stallSumData = new StallSumIpDataXeCore_t;
    stallSourceIpDataMap.emplace(std::pair<uint64_t, void *>(0ull, stallSumData));

    std::map<uint64_t, void *> stallSumIpDataMap;
    // Call for new IP
    l0GfxCoreHelper.stallIpDataMapUpdateFromMap(stallSourceIpDataMap, stallSumIpDataMap);
    // Call for repeated IP
    l0GfxCoreHelper.stallIpDataMapUpdateFromMap(stallSourceIpDataMap, stallSumIpDataMap);

    // Delete the sumData
    l0GfxCoreHelper.stallIpDataMapDeleteSumData(stallSourceIpDataMap);
    stallSourceIpDataMap.clear();
    l0GfxCoreHelper.stallIpDataMapDeleteSumData(stallSumIpDataMap);
    stallSumIpDataMap.clear();
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForGetIpSamplingIpMaskThenCorrectValueIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0x1fffffffull, l0GfxCoreHelper.getIpSamplingIpMask());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenXe2HpgWhenCheckingL0HelperForGetOaTimestampValidBitsThenCorrectValueIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(56u, l0GfxCoreHelper.getOaTimestampValidBits());
}

XE2_HPG_CORETEST_F(L0GfxCoreHelperTestXe2Hpg, GivenL0GfxCoreHelperWhenCheckingMetricsAggregationSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.supportMetricsAggregation());
}

} // namespace ult
} // namespace L0
