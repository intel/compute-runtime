/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

namespace L0 {
namespace ult {

using L0GfxCoreHelperTestXe3p = Test<DeviceFixture>;

HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenAskingForImageCompressionSupportThenReturnFalse, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, givenL0GfxCoreHelperWhenGettingImplicitSyncDispatchModeForCooperativeKernelsThenReturnFalse, xe3pCoreEnumValue);
HWTEST_EXCLUDE_PRODUCT(L0GfxCoreHelperTest, whenAlwaysAllocateEventInLocalMemCalledThenReturnFalse_IsNotXeHpcCore, xe3pCoreEnumValue);

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, givenL0GfxCoreHelperWhenAskingForImageCompressionSupportThenReturnCorrectValue) {
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

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, whenAlwaysAllocateEventInLocalMemCalledThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.alwaysAllocateEventInLocalMem());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, givenL0GfxCoreHelperWhenAskingForUsmCompressionSupportThenReturnCorrectValue) {
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

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenCheckingL0HelperForStateComputeModeTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateComputeModeTracking());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenCheckingL0HelperForFrontEndTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsFrontEndTracking());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenCheckingL0HelperForPipelineSelectTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsPipelineSelectTracking());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pAndPlatforSupportsImagesWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnFalse) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    HardwareInfo *hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto variableBackupHwInfo = std::make_unique<VariableBackup<HardwareInfo>>(hwInfo);
    hwInfo->capabilityTable.supportsImages = true;
    EXPECT_FALSE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pAndPlatforDoesNotSupportImagesWhenCheckingL0HelperForStateBaseAddressTrackingSupportThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    HardwareInfo *hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto variableBackupHwInfo = std::make_unique<VariableBackup<HardwareInfo>>(hwInfo);
    hwInfo->capabilityTable.supportsImages = false;
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pCoreAndPlatforSupportsImagesWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    HardwareInfo *hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto variableBackupHwInfo = std::make_unique<VariableBackup<HardwareInfo>>(hwInfo);
    hwInfo->capabilityTable.supportsImages = true;
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pHeaplessEnabledAndPlatforDoesNotSupportImagesWhenGettingPlatformDefaultHeapAddressModelThenReturnGlobalStateless) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.Enable64BitAddressing.set(1);
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    HardwareInfo *hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto variableBackupHwInfo = std::make_unique<VariableBackup<HardwareInfo>>(hwInfo);
    hwInfo->capabilityTable.supportsImages = false;
    EXPECT_EQ(NEO::HeapAddressModel::globalStateless, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pNotHeaplessEnabledAndPlatforDoesNotSupportImagesWhenGettingPlatformDefaultHeapAddressModelThenReturnPrivateHeaps) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.Enable64BitAddressing.set(0);
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    HardwareInfo *hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto variableBackupHwInfo = std::make_unique<VariableBackup<HardwareInfo>>(hwInfo);
    hwInfo->capabilityTable.supportsImages = false;
    EXPECT_EQ(NEO::HeapAddressModel::privateHeaps, l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenCheckingL0HelperForPlatformSupportsImmediateFlushTaskThenReturnTrue) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.platformSupportsImmediateComputeFlushTask());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pCoreWhenGettingSupportedRTASFormatExpThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version2, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExp()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pCoreWhenGettingSupportedRTASFormatExtThenExpectedFormatIsReturned) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(RTASDeviceFormatInternal::version2, static_cast<RTASDeviceFormatInternal>(l0GfxCoreHelper.getSupportedRTASFormatExt()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenGettingCmdlistUpdateCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(127u, l0GfxCoreHelper.getPlatformCmdListUpdateCapabilities());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenGettingRecordReplayGraphCapabilityThenReturnCorrectValue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(1u, l0GfxCoreHelper.getPlatformRecordReplayGraphCapabilities());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenCallingThreadResumeRequiresUnlockThenReturnTrue) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_TRUE(l0GfxCoreHelper.threadResumeRequiresUnlock());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenCallingisThreadControlStoppedSupportedThenReturnFalse) {
    const auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_FALSE(l0GfxCoreHelper.isThreadControlStoppedSupported());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, givenL0GfxCoreHelperWhenGettingImplicitSyncDispatchModeForCooperativeKernelsThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_TRUE(l0GfxCoreHelper.implicitSynchronizedDispatchForCooperativeKernelsAllowed());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXe3pWhenGetRegsetTypeForLargeGrfDetectionIsCalledThenSrRegsetTypeIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(ZET_DEBUG_REGSET_TYPE_SR_INTEL_GPU, l0GfxCoreHelper.getRegsetTypeForLargeGrfDetection());
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXep3WhenGetGrfRegisterCountIsCalledThenCorrectMaskIsRetuned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    std::vector<uint32_t> val{0, 0, 0, 0, 0, 0, 0, 0};
    val[4] = 0xFFFFFFFF;
    constexpr uint32_t expectedMask = 0x3FF;
    EXPECT_EQ(expectedMask, l0GfxCoreHelper.getGrfRegisterCount(val.data()));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, givenL3FlushInPostSyncWhenGettingMaxKernelAndMaxPacketThenExpectKernelOneAndPacketOne) {

    REQUIRE_DC_FLUSH_OR_SKIP(neoDevice);

    DebugManagerStateRestore restorer;
    debugManager.flags.UsePipeControlMultiKernelEventSync.set(1);
    debugManager.flags.CompactL3FlushEventPacket.set(0);
    debugManager.flags.EnableL3FlushAfterPostSync.set(1);
    debugManager.flags.Enable64BitAddressing.set(1);

    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();

    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    uint32_t expectedPacket = 1;
    HardwareInfo hwInfo = *NEO::defaultHwInfo;

    EXPECT_EQ(1u, l0GfxCoreHelper.getEventMaxKernelCount(hwInfo));
    EXPECT_EQ(expectedPacket, l0GfxCoreHelper.getEventBaseMaxPacketCount(*executionEnvironment.rootDeviceEnvironments[0]));

    debugManager.flags.EnableL3FlushAfterPostSync.set(0);

    expectedPacket = 2;
    EXPECT_EQ(1u, l0GfxCoreHelper.getEventMaxKernelCount(hwInfo));
    EXPECT_EQ(expectedPacket, l0GfxCoreHelper.getEventBaseMaxPacketCount(*executionEnvironment.rootDeviceEnvironments[0]));
}

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenXep3WhenWhenCheckingL0HelperForGetIpSamplingIpMaskThenThenCorrectValueIsReturned) {
    auto &l0GfxCoreHelper = getHelper<L0GfxCoreHelper>();
    EXPECT_EQ(0x1fffffffffffffffull, l0GfxCoreHelper.getIpSamplingIpMask());
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

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenL0GfxCoreHelperWhenL0HelperCanAddIPsFromDataThenSuccess) {

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

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenL0GfxCoreHelperWhenL0HelperCanAddIPsFromMapThenSuccess) {

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

XE3P_CORETEST_F(L0GfxCoreHelperTestXe3p, GivenL0GfxCoreHelperWhenCheckingMetricsAggregationSupportThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0].get();
    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();

    EXPECT_FALSE(l0GfxCoreHelper.supportMetricsAggregation());
}

} // namespace ult
} // namespace L0
