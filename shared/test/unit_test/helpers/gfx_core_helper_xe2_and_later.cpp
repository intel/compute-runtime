/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
}
using namespace NEO;

using GfxCoreHelperXe2AndLaterTests = ::testing::Test;
using GfxCoreHelperXe2AndLaterDeviceTests = GfxCoreHelperTest;

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenAtLeastXe2HpgWhenCallIsTimestampShiftRequiredThenFalseIsReturned, IsAtLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};

    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isTimestampShiftRequired());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenAtLeastXe2HpgWhenCallIsNonCoherentTimestampsModeEnabledThenReturnCorrectValue, IsAtLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};

    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    EXPECT_EQ(!productHelper.isDcFlushAllowed(), productHelper.isNonCoherentTimestampsModeEnabled());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    auto productHelper = &mockDevice->getProductHelper();

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_EQ(productHelper->isDcFlushAllowed(), productHelper->isResolveDependenciesByPipeControlsSupported());

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported());

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenAtLeastXe2HpgWhenEncodeAdditionalTimestampOffsetsThenOffsetsEncoded, IsWithinXe2HpgCoreAndXe3Core) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    constexpr static auto bufferSize = sizeof(MI_STORE_REGISTER_MEM) * 2;

    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    uint64_t fstAddress = 12;
    uint64_t sndAddress = 100;
    MemorySynchronizationCommands<FamilyType>::encodeAdditionalTimestampOffsets(stream, fstAddress, sndAddress, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList storeRegMemList = hwParser.getCommandsList<MI_STORE_REGISTER_MEM>();
    EXPECT_EQ(2u, storeRegMemList.size());
    auto storeRegMemIt = find<MI_STORE_REGISTER_MEM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(storeRegMemIt, hwParser.cmdList.end());

    auto storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*storeRegMemIt);
    EXPECT_EQ(storeRegMem->getRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetHigh);
    EXPECT_EQ(storeRegMem->getMemoryAddress(), fstAddress + sizeof(uint32_t));

    storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*(++storeRegMemIt));
    EXPECT_EQ(storeRegMem->getRegisterAddress(), RegisterOffsets::globalTimestampUn);
    EXPECT_EQ(storeRegMem->getMemoryAddress(), sndAddress + sizeof(uint32_t));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenAtLeastXe2HpgWhenIsExtendedUsmPoolSizeEnabledThenTrueIsReturned, IsAtLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isExtendedUsmPoolSizeEnabled());
}

class MockGmmResourceInfoWithDenyCompression : public MockGmmResourceInfo {
  public:
    MockGmmResourceInfoWithDenyCompression(GMM_RESCREATE_PARAMS *resourceCreateParams) : MockGmmResourceInfo(resourceCreateParams) {}

    bool isResourceDenyCompressionEnabled() override {
        return true;
    }
};

HWTEST2_F(GfxCoreHelperXe2AndLaterTests, givenXe2AndLaterWhenIsCompressionAppliedForImportedResourceWithDenyCompressionEnabledThenFalseIsReturned, IsAtLeastXe2HpgCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    GMM_RESCREATE_PARAMS gmmParams = {};
    gmmParams.Type = GMM_RESOURCE_TYPE::RESOURCE_BUFFER;
    gmmParams.Format = GMM_FORMAT_GENERIC_8BIT;
    gmmParams.BaseWidth64 = 1024;
    gmmParams.BaseHeight = 1;
    gmmParams.Depth = 1;
    gmmParams.Flags.Info.NotCompressed = 0; // Compression enabled in flags

    auto mockGmmResourceInfo = std::make_unique<MockGmmResourceInfoWithDenyCompression>(&gmmParams);
    MockGmm mockGmm(mockExecutionEnvironment.rootDeviceEnvironments[0]->getGmmHelper());
    mockGmm.gmmResourceInfo.reset(mockGmmResourceInfo.release());

    // Even though NotCompressed = 0 (compression enabled), the deny compression should override it
    EXPECT_FALSE(gfxCoreHelper.isCompressionAppliedForImportedResource(mockGmm));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenGfxCoreHelperWhenAskingForTimestampPacketAlignmentThenReturnCachelineSize, IsAtLeastXe2HpgCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr auto expectedAlignment = MemoryConstants::cacheLineSize;

    EXPECT_EQ(expectedAlignment, gfxCoreHelper.getTimestampPacketAllocatorAlignment());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenGfxCoreHelperWhenCallCopyThroughLockedPtrEnabledThenReturnValueDependsOnLocalMemory, IsAtLeastXe2HpgCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.featureTable.flags.ftrLocalMemory = true;
    EXPECT_TRUE(gfxCoreHelper.copyThroughLockedPtrEnabled(hwInfo, productHelper));

    hwInfo.featureTable.flags.ftrLocalMemory = false;
    EXPECT_FALSE(gfxCoreHelper.copyThroughLockedPtrEnabled(hwInfo, productHelper));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenDebugFlagSetWhenCallCopyThroughLockedPtrEnabledThenReturnCorrectValue, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    debugManager.flags.ExperimentalCopyThroughLock.set(0);
    EXPECT_FALSE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo, productHelper));

    debugManager.flags.ExperimentalCopyThroughLock.set(1);
    EXPECT_TRUE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo, productHelper));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, whenNonBcsEngineIsVerifiedThenReturnFalse, IsAtLeastXe2HpgCore) {
    EXPECT_FALSE(EngineHelpers::isBcs(static_cast<aub_stream::EngineType>(aub_stream::ENGINE_BCS8 + 1)));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenGfxCoreHelperWhenAskedIfFenceAllocationRequiredThenReturnCorrectValue, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore dbgRestore;

    const auto hwInfo = *defaultHwInfo;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(-1);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(-1);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(-1);
    EXPECT_EQ(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper), !hwInfo.capabilityTable.isIntegratedDevice);

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_FALSE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));

    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
    debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
    debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
    debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(1);
    EXPECT_TRUE(gfxCoreHelper.isFenceAllocationRequired(hwInfo, productHelper));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenDontProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);

    EXPECT_EQ(NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    EXPECT_EQ(sizeof(MI_MEM_FENCE), MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenProgramGlobalFenceAsMiMemFenceCommandInCommandStreamWhenAddingAdditionalSynchronizationThenMemoryFenceIsReleased, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    auto &hardwareInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    uint8_t buffer[128] = {};
    LinearStream commandStream(buffer, 128);

    MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, 0x0, NEO::FenceType::release, rootDeviceEnvironment);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream);
    EXPECT_EQ(1u, hwParser.cmdList.size());
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*hwParser.cmdList.begin());
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenIntegratedDeviceWhenGettingSizeForAcquireAdditionalSynchronizationThenZeroIsReturned, IsAtLeastXe2HpgCore) {
    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    EXPECT_EQ(0u, MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::acquire, rootDeviceEnvironment));
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenIntegratedDeviceWhenAddingAcquireAdditionalSynchronizationThenNoCommandIsProgrammed, IsAtLeastXe2HpgCore) {
    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    uint8_t buffer[128] = {};
    LinearStream commandStream(buffer, 128);

    MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, 0x1000, NEO::FenceType::acquire, rootDeviceEnvironment);

    EXPECT_EQ(0u, commandStream.getUsed());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenDiscreteDeviceWhenAddingAcquireAdditionalSynchronizationThenMemoryFenceIsProgrammed, IsAtLeastXe2HpgCore) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    uint8_t buffer[128] = {};
    LinearStream commandStream(buffer, 128);

    MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, 0x1000, NEO::FenceType::acquire, rootDeviceEnvironment);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandStream);
    EXPECT_EQ(1u, hwParser.cmdList.size());
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*hwParser.cmdList.begin());
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_ACQUIRE_FENCE, fenceCmd->getFenceType());
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenProductHelperWhenCallGetInternalHeapsPreallocatedThenReturnCorrectValue, IsAtLeastXe2HpgCore) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 2u);

    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(3);
    EXPECT_EQ(productHelper.getInternalHeapsPreallocated(), 3u);
}

HWTEST2_F(GfxCoreHelperXe2AndLaterDeviceTests, givenProductHelperWhenCallUseGemCreateExtInAllocateMemoryByKMDThenTrueIsReturned, IsAtLeastXe2HpgCore) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(productHelper.useGemCreateExtInAllocateMemoryByKMD());
}
