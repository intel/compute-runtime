/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/hw_helper_tests.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "test_traits_common.h"

#include <numeric>

TEST(GfxCoreHelperSimpleTest, givenDebugVariableWhenAskingForCompressionThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    HardwareInfo localHwInfo = *defaultHwInfo;

    // debug variable not set
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(GfxCoreHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_FALSE(GfxCoreHelper::compressedImagesSupported(localHwInfo));

    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(GfxCoreHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_TRUE(GfxCoreHelper::compressedImagesSupported(localHwInfo));

    // debug variable set
    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
    DebugManager.flags.RenderCompressedImagesEnabled.set(1);
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(GfxCoreHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_TRUE(GfxCoreHelper::compressedImagesSupported(localHwInfo));

    DebugManager.flags.RenderCompressedBuffersEnabled.set(0);
    DebugManager.flags.RenderCompressedImagesEnabled.set(0);
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_FALSE(GfxCoreHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_FALSE(GfxCoreHelper::compressedImagesSupported(localHwInfo));
}

TEST_F(GfxCoreHelperTest, WhenGettingHelperThenValidHelperReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_NE(nullptr, &gfxCoreHelper);
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenAskingForTimestampPacketAlignmentThenReturnFourCachelines) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    constexpr auto expectedAlignment = MemoryConstants::cacheLineSize * 4;

    EXPECT_EQ(expectedAlignment, gfxCoreHelper.getTimestampPacketAllocatorAlignment());
}

HWTEST2_F(GfxCoreHelperTest, givenGfxCoreHelperWhenGettingISAPaddingThenCorrectValueIsReturned, IsAtMostXeHpgCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getPaddingForISAAllocation(), 512u);
}

HWTEST_F(GfxCoreHelperTest, givenForceExtendedKernelIsaSizeSetWhenGettingISAPaddingThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    auto defaultPadding = gfxCoreHelper.getPaddingForISAAllocation();
    for (int32_t valueToTest : {0, 1, 2, 10}) {
        DebugManager.flags.ForceExtendedKernelIsaSize.set(valueToTest);
        EXPECT_EQ(gfxCoreHelper.getPaddingForISAAllocation(), defaultPadding + MemoryConstants::pageSize * valueToTest);
    }
}

HWTEST2_F(GfxCoreHelperTest, WhenSettingRenderSurfaceStateForBufferThenL1CachePolicyIsSet, IsAtLeastXeHpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &gfxCoreHelper = this->pDevice->getGfxCoreHelper();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    RENDER_SURFACE_STATE state = FamilyType::cmdInitRenderSurfaceState;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);
    *surfaceState = state;
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    size_t offset = 0x1000;
    uint32_t pitch = 0x40;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;

    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, false,
                                                          false);

    EXPECT_NE(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, surfaceState->getL1CachePolicyL1CacheControl());

    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, false,
                                                          true);

    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB, surfaceState->getL1CachePolicyL1CacheControl());

    alignedFree(stateBuffer);
}

TEST_F(GfxCoreHelperTest, givenDebuggingInactiveWhenSipKernelTypeIsQueriedThenCsrTypeIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_NE(nullptr, &gfxCoreHelper);

    auto sipType = gfxCoreHelper.getSipKernelType(false);
    EXPECT_EQ(SipKernelType::Csr, sipType);
}

TEST_F(GfxCoreHelperTest, givenEngineTypeRcsWhenCsTraitsAreQueiredThenCorrectNameInTraitsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_NE(nullptr, &gfxCoreHelper);

    auto &csTraits = gfxCoreHelper.getCsTraits(aub_stream::ENGINE_RCS);
    EXPECT_STREQ("RCS", csTraits.name.c_str());
}

using isTglLpOrBelow = IsAtMostProduct<IGFX_TIGERLAKE_LP>;
HWTEST2_F(GfxCoreHelperTest, givenGfxCoreHelperWhenGettingThreadsPerEUConfigsThenNoConfigsAreReturned, isTglLpOrBelow) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    auto &configs = gfxCoreHelper.getThreadsPerEUConfigs();
    EXPECT_EQ(0U, configs.size());
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, givenGfxCoreHelperWhenGetGpuTimeStampInNSIsCalledThenCorrectValueIsReturned) {

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto timeStamp = 0x00ff'ffff'ffff;
    auto frequency = 123456.0;
    auto result = static_cast<uint64_t>(timeStamp * frequency);
    EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp, frequency));
}

TEST(DwordBuilderTest, WhenSettingNonMaskedBitsThenOnlySelectedBitAreSet) {
    uint32_t dword = 0;

    // expect non-masked bit 2
    uint32_t expectedDword = (1 << 2);
    dword = DwordBuilder::build(2, false, true, 0); // set 2nd bit
    EXPECT_EQ(expectedDword, dword);

    // expect non-masked bits 2 and 3
    expectedDword |= (1 << 3);
    dword = DwordBuilder::build(3, false, true, dword); // set 3rd bit with init value
    EXPECT_EQ(expectedDword, dword);
}

TEST(DwordBuilderTest, WhenSettingMaskedBitsThenOnlySelectedBitAreSet) {
    uint32_t dword = 0;

    // expect masked bit 2
    uint32_t expectedDword = (1 << 2);
    expectedDword |= (1 << (2 + 16));
    dword = DwordBuilder::build(2, true, true, 0); // set 2nd bit (masked)
    EXPECT_EQ(expectedDword, dword);

    // expect masked bits 2 and 3
    expectedDword |= (1 << 3);
    expectedDword |= (1 << (3 + 16));
    dword = DwordBuilder::build(3, true, true, dword); // set 3rd bit (masked) with init value
    EXPECT_EQ(expectedDword, dword);
}

TEST(DwordBuilderTest, GivenDifferentBitValuesWhenSettingMaskedBitsThenOnlySelectedBitAreSet) {
    // expect only mask bit
    uint32_t expectedDword = 1 << (2 + 16);
    auto dword = DwordBuilder::build(2, true, false, 0);
    EXPECT_EQ(expectedDword, dword);

    // expect masked bits 3
    expectedDword = (1 << 3);
    expectedDword |= (1 << (3 + 16));
    dword = DwordBuilder::build(3, true, true, 0);
    EXPECT_EQ(expectedDword, dword);
}

using LriHelperTests = ::testing::Test;

HWTEST_F(LriHelperTests, givenAddressAndOffsetWhenHelperIsUsedThenProgramCmdStream) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);

    LriHelper<FamilyType>::program(&stream, address, data, false);
    auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(address, lri->getRegisterOffset());
    EXPECT_EQ(data, lri->getDataDword());
}

using PipeControlHelperTests = ::testing::Test;

HWTEST_F(PipeControlHelperTests, givenPostSyncWriteTimestampModeWhenHelperIsUsedThenProperFieldsAreProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654321;
    uint64_t immediateData = 0x1234;

    auto expectedPipeControl = FamilyType::cmdInitPipeControl;
    expectedPipeControl.setCommandStreamerStallEnable(true);
    expectedPipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    expectedPipeControl.setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    expectedPipeControl.setAddressHigh(static_cast<uint32_t>(address >> 32));
    HardwareInfo hardwareInfo = *defaultHwInfo;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        stream, PostSyncMode::Timestamp, address, immediateData, hardwareInfo, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hardwareInfo, false) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hardwareInfo);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), pipeControlLocationSize));
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_TRUE(memcmp(pipeControl, &expectedPipeControl, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST_F(PipeControlHelperTests, givenGfxCoreHelperwhenAskingForDcFlushThenReturnTrue) {
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo));
}

HWTEST_F(PipeControlHelperTests, givenDcFlushNotAllowedWhenProgrammingPipeControlThenDontSetDcFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);

    PipeControlArgs args;
    args.dcFlushEnable = true;

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    auto pipeControl = genCmdCast<PIPE_CONTROL *>(stream.getCpuBase());
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWTEST_F(PipeControlHelperTests, givenPostSyncWriteImmediateDataModeWhenHelperIsUsedThenProperFieldsAreProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654321;
    uint64_t immediateData = 0x1234;

    auto expectedPipeControl = FamilyType::cmdInitPipeControl;
    expectedPipeControl.setCommandStreamerStallEnable(true);
    expectedPipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    expectedPipeControl.setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    expectedPipeControl.setAddressHigh(static_cast<uint32_t>(address >> 32));
    expectedPipeControl.setImmediateData(immediateData);
    HardwareInfo hardwareInfo = *defaultHwInfo;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        stream, PostSyncMode::ImmediateData, address, immediateData, hardwareInfo, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hardwareInfo, false) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hardwareInfo);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), pipeControlLocationSize));
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_TRUE(memcmp(pipeControl, &expectedPipeControl, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST_F(PipeControlHelperTests, givenNotifyEnableArgumentIsTrueWhenHelperIsUsedThenNotifyEnableFlagIsTrue) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654321;
    uint64_t immediateData = 0x1234;

    auto expectedPipeControl = FamilyType::cmdInitPipeControl;
    expectedPipeControl.setCommandStreamerStallEnable(true);
    expectedPipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    expectedPipeControl.setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    expectedPipeControl.setAddressHigh(static_cast<uint32_t>(address >> 32));
    expectedPipeControl.setImmediateData(immediateData);
    expectedPipeControl.setNotifyEnable(true);
    HardwareInfo hardwareInfo = *defaultHwInfo;

    PipeControlArgs args;
    args.notifyEnable = true;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        stream, PostSyncMode::ImmediateData, address, immediateData, hardwareInfo, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(hardwareInfo, false) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hardwareInfo);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), pipeControlLocationSize));
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_TRUE(memcmp(pipeControl, &expectedPipeControl, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST_F(PipeControlHelperTests, WhenIsDcFlushAllowedIsCalledThenCorrectResultIsReturned) {
    auto &productHelper = *ProductHelper::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(false, *defaultHwInfo));
    EXPECT_EQ(productHelper.isDcFlushAllowed(), MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo));
}

HWTEST_F(PipeControlHelperTests, WhenPipeControlPostSyncTimestampUsedThenCorrectPostSyncUsed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654320;
    uint64_t immediateData = 0x0;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(
        stream, PostSyncMode::Timestamp, address, immediateData, args);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(stream.getCpuBase());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(address, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(immediateData, pipeControl->getImmediateData());
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pipeControl->getPostSyncOperation());
}

HWTEST_F(PipeControlHelperTests, WhenPipeControlPostSyncWriteImmediateDataUsedThenCorrectPostSyncUsed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654320;
    uint64_t immediateData = 0x1234;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(
        stream, PostSyncMode::ImmediateData, address, immediateData, args);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(stream.getCpuBase());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(address, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(immediateData, pipeControl->getImmediateData());
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
}

TEST(HwInfoTest, givenHwInfoWhenChosenEngineTypeQueriedThenDefaultIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    auto engineType = getChosenEngineType(hwInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engineType);
}

TEST(HwInfoTest, givenNodeOrdinalSetWhenChosenEngineTypeQueriedThenSetValueIsReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.NodeOrdinal.set(aub_stream::ENGINE_VECS);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    auto engineType = getChosenEngineType(hwInfo);
    EXPECT_EQ(aub_stream::ENGINE_VECS, engineType);
}

HWTEST_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenNoAllocationProvidedThenUseArgumentsasInput) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto gmmHelper = pDevice->getGmmHelper();

    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(sizeof(RENDER_SURFACE_STATE), gfxCoreHelper.getRenderSurfaceStateSize());

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;
    length.Length = static_cast<uint32_t>(size - 1);
    uint64_t addr = 0x2000;
    size_t offset = 0x1000;
    uint32_t pitch = 0x40;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, true, false);

    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(pitch, state->getSurfacePitch());
    addr += offset;
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());
    EXPECT_EQ(type, state->getSurfaceType());
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), state->getMemoryObjectControlState());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1003;
    length.Length = static_cast<uint32_t>(alignUp(size, 4) - 1);
    bool isReadOnly = false;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1000;
    addr = 0x2001;
    length.Length = static_cast<uint32_t>(size - 1);
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1005;
    length.Length = static_cast<uint32_t>(alignUp(size, 4) - 1);
    isReadOnly = true;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    alignedFree(stateBuffer);
}

HWTEST_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenAllocationProvidedThenUseAllocationAsInput) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    length.Length = static_cast<uint32_t>(allocSize - 1);
    GraphicsAllocation allocation(0, AllocationType::UNKNOWN, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 0u);
    allocation.setDefaultGmm(new Gmm(pDevice->getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, true, false);
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(pitch, state->getSurfacePitch() - 1u);
    EXPECT_EQ(gpuAddr, state->getSurfaceBaseAddress());

    EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxDisabledThenSetCoherencyToGpuAndAuxModeToCompression) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 0u);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    allocation.getDefaultGmm()->isCompressionEnabled = true;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, state->getCoherencyType());
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(state, allocation.getDefaultGmm()));

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenGmmCompressionDisabledAndAllocationEnabledAnNonAuxDisabledThenSetCoherencyToIaAndAuxModeToNone) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 1);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(GfxCoreHelperTest, givenOverrideMocsIndexForScratchSpaceWhenSurfaceStateIsProgrammedForScratchSpaceThenOverrideMocsIndexWithCorrectValue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideMocsIndexForScratchSpace.set(1);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 1);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);

    auto mocsProgrammed = state->getMemoryObjectControlState() >> 1;
    EXPECT_EQ(1u, mocsProgrammed);

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxEnabledThenSetCoherencyToIaAndAuxModeToNone) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 1u);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    allocation.getDefaultGmm()->isCompressionEnabled = true;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, true, false);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(GfxCoreHelperTest, DISABLED_profilingCreationOfRenderSurfaceStateVsMemcpyOfCachelineAlignedBuffer) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    constexpr uint32_t maxLoop = 1000u;

    std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> timesCreate;
    timesCreate.reserve(maxLoop * 2);

    std::vector<std::chrono::time_point<std::chrono::high_resolution_clock>> timesMemCpy;
    timesMemCpy.reserve(maxLoop * 2);

    std::vector<int64_t> nanoDurationCreate;
    nanoDurationCreate.reserve(maxLoop);

    std::vector<int64_t> nanoDurationCpy;
    nanoDurationCpy.reserve(maxLoop);

    std::vector<void *> surfaceStates;
    surfaceStates.reserve(maxLoop);

    std::vector<void *> copyBuffers;
    copyBuffers.reserve(maxLoop);

    for (uint32_t i = 0; i < maxLoop; ++i) {
        void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
        ASSERT_NE(nullptr, stateBuffer);
        memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
        surfaceStates.push_back(stateBuffer);

        void *copyBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
        ASSERT_NE(nullptr, copyBuffer);
        copyBuffers.push_back(copyBuffer);
    }

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;

    for (uint32_t i = 0; i < maxLoop; ++i) {
        auto t1 = std::chrono::high_resolution_clock::now();
        gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, surfaceStates[i], size, addr, 0, pitch, nullptr, false, type, true, false);
        auto t2 = std::chrono::high_resolution_clock::now();
        timesCreate.push_back(t1);
        timesCreate.push_back(t2);
    }

    for (uint32_t i = 0; i < maxLoop; ++i) {
        auto t1 = std::chrono::high_resolution_clock::now();
        memcpy_s(copyBuffers[i], sizeof(RENDER_SURFACE_STATE), surfaceStates[i], sizeof(RENDER_SURFACE_STATE));
        auto t2 = std::chrono::high_resolution_clock::now();
        timesMemCpy.push_back(t1);
        timesMemCpy.push_back(t2);
    }

    for (uint32_t i = 0; i < maxLoop; ++i) {
        std::chrono::duration<double> delta = timesCreate[i * 2 + 1] - timesCreate[i * 2];
        std::chrono::nanoseconds duration = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
        nanoDurationCreate.push_back(duration.count());

        delta = timesMemCpy[i * 2 + 1] - timesMemCpy[i * 2];
        duration = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
        nanoDurationCpy.push_back(duration.count());
    }

    sort(nanoDurationCreate.begin(), nanoDurationCreate.end());
    sort(nanoDurationCpy.begin(), nanoDurationCpy.end());

    double averageCreate = std::accumulate(nanoDurationCreate.begin(), nanoDurationCreate.end(), 0.0) / nanoDurationCreate.size();
    double averageCpy = std::accumulate(nanoDurationCpy.begin(), nanoDurationCpy.end(), 0.0) / nanoDurationCpy.size();

    size_t middleCreate = nanoDurationCreate.size() / 2;
    size_t middleCpy = nanoDurationCpy.size() / 2;

    std::cout << "Creation average: " << averageCreate << " median: " << nanoDurationCreate[middleCreate];
    std::cout << " min: " << nanoDurationCreate[0] << " max: " << nanoDurationCreate[nanoDurationCreate.size() - 1] << std::endl;
    std::cout << "Copy average: " << averageCpy << " median: " << nanoDurationCpy[middleCpy];
    std::cout << " min: " << nanoDurationCpy[0] << " max: " << nanoDurationCpy[nanoDurationCpy.size() - 1] << std::endl;

    for (uint32_t i = 0; i < maxLoop; i++) {
        std::cout << "#" << (i + 1) << " Create: " << nanoDurationCreate[i] << " Copy: " << nanoDurationCpy[i] << std::endl;
    }

    for (uint32_t i = 0; i < maxLoop; ++i) {
        alignedFree(surfaceStates[i]);
        alignedFree(copyBuffers[i]);
    }
}

TEST(GfxCoreHelperCacheFlushTest, givenEnableCacheFlushFlagIsEnableWhenPlatformDoesNotSupportThenOverrideAndReturnSupportTrue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_TRUE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(GfxCoreHelperCacheFlushTest, givenEnableCacheFlushFlagIsDisableWhenPlatformSupportsThenOverrideAndReturnSupportFalse) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_FALSE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(GfxCoreHelperCacheFlushTest, givenEnableCacheFlushFlagIsReadPlatformSettingWhenPlatformDoesNotSupportThenReturnSupportFalse) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(-1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_FALSE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(GfxCoreHelperCacheFlushTest, givenEnableCacheFlushFlagIsReadPlatformSettingWhenPlatformSupportsThenReturnSupportTrue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(-1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_TRUE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, givenGfxCoreHelperWhenGettingGlobalTimeStampBitsThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getGlobalTimeStampBits(), 36U);
}

TEST_F(GfxCoreHelperTest, givenEnableLocalMemoryDebugVarAndOsEnableLocalMemoryWhenSetThenGetEnableLocalMemoryReturnsCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    VariableBackup<bool> orgOsEnableLocalMemory(&OSInterface::osEnableLocalMemory);
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    DebugManager.flags.EnableLocalMemory.set(0);
    EXPECT_FALSE(gfxCoreHelper.getEnableLocalMemory(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(1);
    EXPECT_TRUE(gfxCoreHelper.getEnableLocalMemory(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(-1);

    OSInterface::osEnableLocalMemory = false;
    EXPECT_FALSE(gfxCoreHelper.getEnableLocalMemory(hardwareInfo));

    OSInterface::osEnableLocalMemory = true;
    EXPECT_EQ(gfxCoreHelper.isLocalMemoryEnabled(hardwareInfo), gfxCoreHelper.getEnableLocalMemory(hardwareInfo));
}

TEST_F(GfxCoreHelperTest, givenAUBDumpForceAllToLocalMemoryDebugVarWhenSetThenGetEnableLocalMemoryReturnsCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);
    EXPECT_TRUE(gfxCoreHelper.getEnableLocalMemory(hardwareInfo));
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, givenVariousCachesRequestThenCorrectMocsIndexesAreReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmmHelper = this->pDevice->getGmmHelper();
    auto expectedMocsForL3off = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    auto expectedMocsForL3on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
    auto expectedMocsForL3andL1on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;

    auto mocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, false, true);
    EXPECT_EQ(expectedMocsForL3off, mocsIndex);

    mocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, true, false);
    EXPECT_EQ(expectedMocsForL3on, mocsIndex);

    mocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, true, true);
    if (mocsIndex != expectedMocsForL3andL1on) {
        EXPECT_EQ(expectedMocsForL3on, mocsIndex);
    } else {
        EXPECT_EQ(expectedMocsForL3andL1on, mocsIndex);
    }
}

HWTEST_F(GfxCoreHelperTest, whenQueryingMaxNumSamplersThenReturnSixteen) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getMaxNumSamplers());
}

HWTEST_F(GfxCoreHelperTest, givenDebugVariableSetWhenAskingForAuxTranslationModeThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    EXPECT_EQ(UnitTestHelper<FamilyType>::requiredAuxTranslationMode, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    if (GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo) == AuxTranslationMode::Blit) {
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        EXPECT_EQ(AuxTranslationMode::Builtin, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));
    }

    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::None));
    EXPECT_EQ(AuxTranslationMode::None, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    hwInfo.capabilityTable.blitterOperationsSupported = false;
    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Blit));
    EXPECT_EQ(AuxTranslationMode::Builtin, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    hwInfo.capabilityTable.blitterOperationsSupported = true;
    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Blit));
    EXPECT_EQ(AuxTranslationMode::Blit, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Builtin));
    EXPECT_EQ(AuxTranslationMode::Builtin, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));
}

HWTEST_F(GfxCoreHelperTest, givenDebugFlagWhenCheckingIfBufferIsSuitableForCompressionThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(0);
    EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(0));
    EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(KB));
    EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(KB + 1));

    DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(1);
    EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(0));
    EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(KB));
    EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(KB + 1));
}

HWTEST_F(GfxCoreHelperTest, WhenIsBankOverrideRequiredIsCalledThenFalseIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(gfxCoreHelper.isBankOverrideRequired(hardwareInfo, productHelper));
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, GivenBarrierEncodingWhenCallingGetBarriersCountFromHasBarrierThenNumberOfBarriersIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(0u, gfxCoreHelper.getBarriersCountFromHasBarriers(0u));
    EXPECT_EQ(1u, gfxCoreHelper.getBarriersCountFromHasBarriers(1u));
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, GivenVariousValuesWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto result = gfxCoreHelper.calculateAvailableThreadCount(hardwareInfo, 0);
    EXPECT_EQ(hardwareInfo.gtSystemInfo.ThreadCount, result);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, GivenModifiedGtSystemInfoWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto hwInfo = hardwareInfo;
    for (auto threadCount : {1u, 5u, 9u}) {
        hwInfo.gtSystemInfo.ThreadCount = threadCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, 0);
        EXPECT_EQ(threadCount, result);
    }
}

HWTEST_F(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenIsOffsetToSkipSetFFIDGPWARequiredCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(gfxCoreHelper.isOffsetToSkipSetFFIDGPWARequired(hardwareInfo, productHelper));
}

HWTEST_F(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenIsForceDefaultRCSEngineWARequiredCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    EXPECT_FALSE(GfxCoreHelperHw<FamilyType>::isForceDefaultRCSEngineWARequired(hardwareInfo));
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenIsWorkaroundRequiredCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
}

HWTEST_F(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenMinimalSIMDSizeIsQueriedThen8IsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(8u, gfxCoreHelper.getMinimalSIMDSize());
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, WhenIsFusedEuDispatchEnabledIsCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isFusedEuDispatchEnabled(hardwareInfo, false));
}

HWTEST_F(PipeControlHelperTests, WhenGettingPipeControSizeForCacheFlushThenReturnCorrectValue) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    size_t actualSize = MemorySynchronizationCommands<FamilyType>::getSizeForFullCacheFlush();
    EXPECT_EQ(sizeof(PIPE_CONTROL), actualSize);
}

HWTEST_F(PipeControlHelperTests, WhenProgrammingCacheFlushThenExpectBasicFieldsSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);

    MemorySynchronizationCommands<FamilyType>::addFullCacheFlush(stream, *defaultHwInfo);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo), pipeControl->getDcFlushEnable());

    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());
    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getPipeControlFlushEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getTlbInvalidate());
}

using ProductHelperCommonTest = Test<DeviceFixture>;

HWTEST2_F(ProductHelperCommonTest, givenBlitterPreferenceWhenEnablingBlitterOperationsSupportThenHonorThePreference, IsAtLeastGen12lp) {
    HardwareInfo hardwareInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    productHelper.configureHardwareCustom(&hardwareInfo, nullptr);

    const auto expectedBlitterSupport = productHelper.obtainBlitterPreference(hardwareInfo);
    EXPECT_EQ(expectedBlitterSupport, hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWTEST2_F(ProductHelperCommonTest, givenHardwareInfoWhendisableRcsExposureIsCalledThenFtrRcsNodeIsFalse, IsAtLeastGen12lp) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    productHelper.disableRcsExposure(&hwInfo);
    EXPECT_FALSE(hwInfo.featureTable.flags.ftrRcsNode);
}

HWTEST2_F(ProductHelperCommonTest, givenHardwareInfoAndDebugVariableNodeOrdinalEqualsRcsWhenDisableRcsExposureIsCalledThenFtrRcsNodeIsTrue, IsAtLeastGen12lp) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    productHelper.disableRcsExposure(&hwInfo);
    EXPECT_TRUE(hwInfo.featureTable.flags.ftrRcsNode);
}

HWTEST2_F(ProductHelperCommonTest, givenHardwareInfoAndDebugVariableNodeOrdinalEqualsCccsWhenDisableRcsExposureIsCalledThenFtrRcsNodeIsTrue, IsAtLeastGen12lp) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    DebugManagerStateRestore restore;
    DebugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS));

    productHelper.disableRcsExposure(&hwInfo);
    EXPECT_TRUE(hwInfo.featureTable.flags.ftrRcsNode);
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenAskingForIsaSystemMemoryPlacementThenReturnFalseIfLocalMemorySupported) {
    DebugManagerStateRestore restorer;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    auto localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, gfxCoreHelper.useSystemMemoryPlacementForISA(hardwareInfo));

    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, gfxCoreHelper.useSystemMemoryPlacementForISA(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(true);
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, gfxCoreHelper.useSystemMemoryPlacementForISA(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(false);
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, gfxCoreHelper.useSystemMemoryPlacementForISA(hardwareInfo));
}

TEST_F(GfxCoreHelperTest, givenInvalidEngineTypeWhenGettingEngineGroupTypeThenThrow) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_ANY_THROW(gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::NUM_ENGINES, EngineUsage::Regular, hardwareInfo));
    EXPECT_ANY_THROW(gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_VECS, EngineUsage::Regular, hardwareInfo));
}

HWTEST2_F(ProductHelperCommonTest, givenDebugFlagSetWhenEnablingBlitterOperationsSupportThenHonorTheFlag, IsAtLeastGen12lp) {
    DebugManagerStateRestore restore{};
    HardwareInfo hardwareInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    productHelper.configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_TRUE(hardwareInfo.capabilityTable.blitterOperationsSupported);

    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    productHelper.configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_FALSE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, GivenVariousValuesWhenAlignSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        EXPECT_EQ(0u, gfxCoreHelper.alignSlmSize(0));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(1));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(1024));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(1025));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(2048));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(2049));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(4096));
        EXPECT_EQ(8192u, gfxCoreHelper.alignSlmSize(4097));
        EXPECT_EQ(8192u, gfxCoreHelper.alignSlmSize(8192));
        EXPECT_EQ(16384u, gfxCoreHelper.alignSlmSize(8193));
        EXPECT_EQ(16384u, gfxCoreHelper.alignSlmSize(12288));
        EXPECT_EQ(16384u, gfxCoreHelper.alignSlmSize(16384));
        EXPECT_EQ(32768u, gfxCoreHelper.alignSlmSize(16385));
        EXPECT_EQ(32768u, gfxCoreHelper.alignSlmSize(24576));
        EXPECT_EQ(32768u, gfxCoreHelper.alignSlmSize(32768));
        EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(32769));
        EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(49152));
        EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(65535));
        EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(65536));
    } else {
        EXPECT_EQ(0u, gfxCoreHelper.alignSlmSize(0));
        EXPECT_EQ(1024u, gfxCoreHelper.alignSlmSize(1));
        EXPECT_EQ(1024u, gfxCoreHelper.alignSlmSize(1024));
        EXPECT_EQ(2048u, gfxCoreHelper.alignSlmSize(1025));
        EXPECT_EQ(2048u, gfxCoreHelper.alignSlmSize(2048));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(2049));
        EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(4096));
        EXPECT_EQ(8192u, gfxCoreHelper.alignSlmSize(4097));
        EXPECT_EQ(8192u, gfxCoreHelper.alignSlmSize(8192));
        EXPECT_EQ(16384u, gfxCoreHelper.alignSlmSize(8193));
        EXPECT_EQ(16384u, gfxCoreHelper.alignSlmSize(16384));
        EXPECT_EQ(32768u, gfxCoreHelper.alignSlmSize(16385));
        EXPECT_EQ(32768u, gfxCoreHelper.alignSlmSize(32768));
        EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(32769));
        EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(65536));
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, GivenVariousValuesWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        EXPECT_EQ(0u, gfxCoreHelper.computeSlmValues(hwInfo, 0));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 1));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 1024));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 1025));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 2048));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 2049));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 4096));
        EXPECT_EQ(2u, gfxCoreHelper.computeSlmValues(hwInfo, 4097));
        EXPECT_EQ(2u, gfxCoreHelper.computeSlmValues(hwInfo, 8192));
        EXPECT_EQ(4u, gfxCoreHelper.computeSlmValues(hwInfo, 8193));
        EXPECT_EQ(4u, gfxCoreHelper.computeSlmValues(hwInfo, 12288));
        EXPECT_EQ(4u, gfxCoreHelper.computeSlmValues(hwInfo, 16384));
        EXPECT_EQ(8u, gfxCoreHelper.computeSlmValues(hwInfo, 16385));
        EXPECT_EQ(8u, gfxCoreHelper.computeSlmValues(hwInfo, 24576));
        EXPECT_EQ(8u, gfxCoreHelper.computeSlmValues(hwInfo, 32768));
        EXPECT_EQ(16u, gfxCoreHelper.computeSlmValues(hwInfo, 32769));
        EXPECT_EQ(16u, gfxCoreHelper.computeSlmValues(hwInfo, 49152));
        EXPECT_EQ(16u, gfxCoreHelper.computeSlmValues(hwInfo, 65535));
        EXPECT_EQ(16u, gfxCoreHelper.computeSlmValues(hwInfo, 65536));
    } else {
        EXPECT_EQ(0u, gfxCoreHelper.computeSlmValues(hwInfo, 0));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 1));
        EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 1024));
        EXPECT_EQ(2u, gfxCoreHelper.computeSlmValues(hwInfo, 1025));
        EXPECT_EQ(2u, gfxCoreHelper.computeSlmValues(hwInfo, 2048));
        EXPECT_EQ(3u, gfxCoreHelper.computeSlmValues(hwInfo, 2049));
        EXPECT_EQ(3u, gfxCoreHelper.computeSlmValues(hwInfo, 4096));
        EXPECT_EQ(4u, gfxCoreHelper.computeSlmValues(hwInfo, 4097));
        EXPECT_EQ(4u, gfxCoreHelper.computeSlmValues(hwInfo, 8192));
        EXPECT_EQ(5u, gfxCoreHelper.computeSlmValues(hwInfo, 8193));
        EXPECT_EQ(5u, gfxCoreHelper.computeSlmValues(hwInfo, 16384));
        EXPECT_EQ(6u, gfxCoreHelper.computeSlmValues(hwInfo, 16385));
        EXPECT_EQ(6u, gfxCoreHelper.computeSlmValues(hwInfo, 32768));
        EXPECT_EQ(7u, gfxCoreHelper.computeSlmValues(hwInfo, 32769));
        EXPECT_EQ(7u, gfxCoreHelper.computeSlmValues(hwInfo, 65536));
    }
}

HWTEST_F(GfxCoreHelperTest, GivenZeroSlmSizeWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    using SHARED_LOCAL_MEMORY_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto receivedSlmSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(gfxCoreHelper.computeSlmValues(hwInfo, 0));
    EXPECT_EQ(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K, receivedSlmSize);
}

HWTEST2_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCheckingSipWaThenFalseIsReturned, isTglLpOrBelow) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_FALSE(gfxCoreHelper.isSipWANeeded(*defaultHwInfo));
}
HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, givenGfxCoreHelperWhenGettingPlanarYuvHeightThenHelperReturnsCorrectValue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getPlanarYuvMaxHeight(), 16352u);
}

TEST_F(GfxCoreHelperTest, WhenGettingIsCpuImageTransferPreferredThenFalseIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isCpuImageTransferPreferred(*defaultHwInfo));
}

TEST_F(GfxCoreHelperTest, whenFtrGpGpuMidThreadLevelPreemptFeatureDisabledThenFalseIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    FeatureTable featureTable = {};
    featureTable.flags.ftrGpGpuMidThreadLevelPreempt = false;
    bool result = gfxCoreHelper.isAdditionalFeatureFlagRequired(&featureTable);
    EXPECT_FALSE(result);
}

HWTEST_F(GfxCoreHelperTest, whenGettingNumberOfCacheRegionsThenReturnZero) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(0u, gfxCoreHelper.getNumCacheRegions());
}

HWCMDTEST_F(IGFX_GEN8_CORE, GfxCoreHelperTest, whenCheckingForSmallKernelPreferenceThenFalseIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.preferSmallWorkgroupSizeForKernel(0u, this->pDevice->getHardwareInfo()));
    EXPECT_FALSE(gfxCoreHelper.preferSmallWorkgroupSizeForKernel(20000u, this->pDevice->getHardwareInfo()));
}

HWTEST_F(GfxCoreHelperTest, whenSetCompressedFlagThenProperFlagSet) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->resourceParams.Flags.Info.RenderCompressed = 0;

    gfxCoreHelper.applyRenderCompressionFlag(*gmm, 1);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.RenderCompressed);

    gfxCoreHelper.applyRenderCompressionFlag(*gmm, 0);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.RenderCompressed);
}

HWTEST_F(GfxCoreHelperTest, whenAdjustPreemptionSurfaceSizeIsCalledThenCsrSizeDoesntChange) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    size_t csrSize = 1024;
    size_t oldCsrSize = csrSize;
    gfxCoreHelper.adjustPreemptionSurfaceSize(csrSize);
    EXPECT_EQ(oldCsrSize, csrSize);
}

HWTEST_F(GfxCoreHelperTest, whenSetSipKernelDataIsCalledThenSipKernelDataDoesntChange) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    uint32_t *sipKernelBinary = nullptr;
    uint32_t *oldSipKernelBinary = sipKernelBinary;
    size_t kernelBinarySize = 1024;
    size_t oldKernelBinarySize = kernelBinarySize;
    gfxCoreHelper.setSipKernelData(sipKernelBinary, kernelBinarySize);
    EXPECT_EQ(oldKernelBinarySize, kernelBinarySize);
    EXPECT_EQ(oldSipKernelBinary, sipKernelBinary);
}

HWTEST_F(GfxCoreHelperTest, whenIsSipKernelAsHexadecimalArrayPreferredIsCalledThenReturnFalse) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred());
}

using isXeHpCoreOrBelow = IsAtMostProduct<IGFX_XE_HP_SDV>;
HWTEST2_F(GfxCoreHelperTest, givenXeHPAndBelowPlatformWhenCheckingIfUnTypedDataPortCacheFlushRequiredThenReturnFalse, isXeHpCoreOrBelow) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.unTypedDataPortCacheFlushRequired());
}

HWTEST2_F(GfxCoreHelperTest, givenXeHPAndBelowPlatformPlatformWhenCheckingIfEngineTypeRemappingIsRequiredThenReturnFalse, isXeHpCoreOrBelow) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isEngineTypeRemappingToHwSpecificRequired());
}

HWTEST2_F(GfxCoreHelperTest, givenAtMostGen12lpPlatformiWhenCheckingIfScratchSpaceSurfaceStateAccessibleThenFalseIsReturned, IsAtMostGen12lp) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isScratchSpaceSurfaceStateAccessible());
}

HWTEST2_F(GfxCoreHelperTest, givenAtLeastXeHpPlatformWhenCheckingIfScratchSpaceSurfaceStateAccessibleTheniTrueIsReturned, IsAtLeastXeHpCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isScratchSpaceSurfaceStateAccessible());
}

HWTEST_F(GfxCoreHelperTest, givenGetRenderSurfaceStateBaseAddressCalledThenCorrectValueIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE renderSurfaceState;
    uint64_t expectedBaseAddress = 0x1122334455667788;
    renderSurfaceState.setSurfaceBaseAddress(expectedBaseAddress);
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(expectedBaseAddress, gfxCoreHelper.getRenderSurfaceStateBaseAddress(&renderSurfaceState));
}

HWTEST_F(GfxCoreHelperTest, givenGetRenderSurfaceStatePitchCalledThenCorrectValueIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE renderSurfaceState;
    uint32_t expectedPitch = 0x400;
    renderSurfaceState.setSurfacePitch(expectedPitch);
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(expectedPitch, gfxCoreHelper.getRenderSurfaceStatePitch(&renderSurfaceState));
}

HWTEST_F(GfxCoreHelperTest, whenBlitterSupportIsDisabledThenDontExposeAnyBcsEngine) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto engineUsageTypes = gfxCoreHelper.getGpgpuEngineInstances(hwInfo);
    for (auto &engineUsageType : engineUsageTypes) {
        EXPECT_FALSE(EngineHelpers::isBcs(engineUsageType.first));
    }
}

HWTEST2_F(GfxCoreHelperTest, givenNotXeHpOrXeHpgCoreWhenDisableL3ForDebugCalledThenFalseIsReturned, IsNotXeHpOrXeHpgCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(gfxCoreHelper.disableL3CacheForDebug(*defaultHwInfo, productHelper));
}

HWTEST2_F(GfxCoreHelperTest, givenXeHpOrXeHpgCoreWhenDisableL3ForDebugCalledThenTrueIsReturned, IsXeHpOrXeHpgCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_TRUE(gfxCoreHelper.disableL3CacheForDebug(*defaultHwInfo, productHelper));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenGettingIfRevisionSpecificBinaryBuiltinIsRequiredThenFalseIsReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isRevisionSpecificBinaryBuiltinRequired());
}

HWTEST2_F(GfxCoreHelperTest, givenDG2GfxCoreHelperWhenGettingIsPlatformFlushTaskEnabledThenTrueIsReturned, IsDG2) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isPlatformFlushTaskEnabled(*defaultHwInfo));
}

HWTEST2_F(GfxCoreHelperTest, givenPvcGfxCoreHelperWhenGettingIsPlatformFlushTaskEnabledThenTrueIsReturned, IsPVC) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isPlatformFlushTaskEnabled(*defaultHwInfo));
}

HWTEST2_F(GfxCoreHelperTest, givenAtMostGen12lpGfxCoreHelperWhenGettingIsPlatformFlushTaskEnabledThenFalseIsReturned, IsAtMostGen12lp) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isPlatformFlushTaskEnabled(*defaultHwInfo));
}

struct CoherentWANotNeeded {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if (productFamily == IGFX_BROADWELL)
            return false;
        return !TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceGpuNonCoherent;
    }
};
HWTEST2_F(GfxCoreHelperTest, givenProductHelperWhenCheckingForceNonGpuCoherencyWAThenPassedValueReturned, CoherentWANotNeeded) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.forceNonGpuCoherencyWA(true));
    EXPECT_FALSE(gfxCoreHelper.forceNonGpuCoherencyWA(false));
}

struct ForceNonCoherentMode {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if (productFamily == IGFX_BROADWELL)
            return false;
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceGpuNonCoherent;
    }
};

HWTEST2_F(GfxCoreHelperTest, givenProductHelperWhenCheckingForceNonGpuCoherencyWAThenFalseIsReturned, ForceNonCoherentMode) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.forceNonGpuCoherencyWA(true));
    EXPECT_FALSE(gfxCoreHelper.forceNonGpuCoherencyWA(false));
}

HWTEST_F(GfxCoreHelperTest, GivenHwInfoWhenGetBatchBufferEndSizeCalledThenCorrectSizeReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getBatchBufferEndSize(), sizeof(typename FamilyType::MI_BATCH_BUFFER_END));
}

HWTEST_F(GfxCoreHelperTest, GivenHwInfoWhenGetBatchBufferEndReferenceCalledThenCorrectPtrReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getBatchBufferEndReference(), reinterpret_cast<const void *>(&FamilyType::cmdInitBatchBufferEnd));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenPassingCopyEngineTypeThenItsCopyOnly) {
    EXPECT_TRUE(EngineHelper::isCopyOnlyEngineType(EngineGroupType::Copy));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenPassingLinkedCopyEngineTypeThenItsCopyOnly) {
    EXPECT_TRUE(EngineHelper::isCopyOnlyEngineType(EngineGroupType::LinkedCopy));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenPassingComputeEngineTypeThenItsNotCopyOnly) {
    EXPECT_FALSE(EngineHelper::isCopyOnlyEngineType(EngineGroupType::Compute));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenAskingForRelaxedOrderingSupportThenReturnFalse) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isRelaxedOrderingSupported());
}

HWTEST2_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCallCopyThroughLockedPtrEnabledThenReturnFalse, IsNotXeHpgOrXeHpcCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo));
}

HWTEST2_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCallGetAmountOfAllocationsToFillThenReturnFalse, IsNotXeHpcCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenFlagSetAndCallCopyThroughLockedPtrEnabledThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    DebugManager.flags.ExperimentalCopyThroughLock.set(0);
    EXPECT_FALSE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo));

    DebugManager.flags.ExperimentalCopyThroughLock.set(1);
    EXPECT_TRUE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    DebugManager.flags.SetAmountOfReusableAllocations.set(0);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

using LogicalStateHelperTest = ::testing::Test;

HWTEST_F(LogicalStateHelperTest, whenCreatingLogicalStateHelperThenReturnNullptr) {
    EXPECT_EQ(nullptr, LogicalStateHelper::create<FamilyType>());
}

HWTEST2_F(GfxCoreHelperTest, GivenVariousValuesAndXeHpOrXeHpgCoreWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsXeHpOrXeHpgCore) {
    std::array<std::pair<uint32_t, uint32_t>, 3> grfTestInputs = {{{64, 8},
                                                                   {128, 8},
                                                                   {256, 4}}};

    const auto &hwInfo = *defaultHwInfo;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto expected = expectedThreadCountPerEu * hwInfo.gtSystemInfo.EUCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, grfCount);
        EXPECT_EQ(expected, result);
    }
}

HWTEST2_F(GfxCoreHelperTest, GivenModifiedGtSystemInfoAndXeHpOrXeHpgCoreWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsXeHpOrXeHpgCore) {
    std::array<std::tuple<uint32_t, uint32_t, uint32_t>, 3> testInputs = {{{1, 64, 1},
                                                                           {5, 128, 5},
                                                                           {8, 256, 4}}};
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto hwInfo = hardwareInfo;
    for (const auto &[threadCount, grfCount, expectedThreadCount] : testInputs) {
        hwInfo.gtSystemInfo.ThreadCount = threadCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, grfCount);
        EXPECT_EQ(expectedThreadCount, result);
    }
}

HWTEST2_F(GfxCoreHelperTest, givenAtMostGen12lpPlatformWhenGettingMinimalScratchSpaceSizeThen1024IsReturned, IsAtMostGen12lp) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(1024U, gfxCoreHelper.getMinimalScratchSpaceSize());
}

HWTEST2_F(GfxCoreHelperTest, givenAtLeastXeHpPlatformWhenGettingMinimalScratchSpaceSizeThen64IsReturned, IsAtLeastXeHpCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(64U, gfxCoreHelper.getMinimalScratchSpaceSize());
}

HWTEST_F(GfxCoreHelperTest, whenIsDynamicallyPopulatedisFalseThengetHighestEnabledSliceReturnsMaxSlicesSupported) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = false;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 4;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto maxSlice = gfxCoreHelper.getHighestEnabledSlice(hwInfo);
    EXPECT_EQ(maxSlice, hwInfo.gtSystemInfo.MaxSlicesSupported);
}

HWTEST_F(GfxCoreHelperTest, whenIsDynamicallyPopulatedisTrueThengetHighestEnabledSliceReturnsHighestEnabledSliceInfo) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 4;
    for (int i = 0; i < GT_MAX_SLICE; i++) {
        hwInfo.gtSystemInfo.SliceInfo[i].Enabled = false;
    }
    hwInfo.gtSystemInfo.SliceInfo[6].Enabled = true;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto maxSlice = gfxCoreHelper.getHighestEnabledSlice(hwInfo);
    EXPECT_EQ(maxSlice, 7u);
}

HWTEST_F(ProductHelperCommonTest, givenPatIndexAndAllocationTypeWhenCallOverridePatIndexThenSameIndexIsReturned) {
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    uint64_t patIndex = 1u;
    auto allocationType = NEO::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER;
    EXPECT_EQ(patIndex, gfxCoreHelper.overridePatIndex(allocationType, patIndex));
    allocationType = NEO::AllocationType::BUFFER;
    patIndex = 3u;
    EXPECT_EQ(patIndex, gfxCoreHelper.overridePatIndex(allocationType, patIndex));
}
