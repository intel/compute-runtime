/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gfx_core_helper_tests.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "test_traits_common.h"

#include <numeric>

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

TEST(GfxCoreHelperTestCreate, WhenGfxCoreHelperIsCalledWithUnknownGfxCoreThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, GfxCoreHelper::create(IGFX_UNKNOWN_CORE));
}

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
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    debugManager.flags.RenderCompressedImagesEnabled.set(1);
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(GfxCoreHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_TRUE(GfxCoreHelper::compressedImagesSupported(localHwInfo));

    debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    debugManager.flags.RenderCompressedImagesEnabled.set(0);
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
        debugManager.flags.ForceExtendedKernelIsaSize.set(valueToTest);
        EXPECT_EQ(gfxCoreHelper.getPaddingForISAAllocation(), defaultPadding + MemoryConstants::pageSize * valueToTest);
    }
}

HWTEST2_F(GfxCoreHelperTest, WhenSettingRenderSurfaceStateForBufferThenL1CachePolicyIsSet, IsAtLeastXeCore) {
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

    EXPECT_NE(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB, surfaceState->getL1CacheControlCachePolicy());

    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, false,
                                                          true);

    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB, surfaceState->getL1CacheControlCachePolicy());

    alignedFree(stateBuffer);
}

TEST_F(GfxCoreHelperTest, givenDebuggingInactiveWhenSipKernelTypeIsQueriedThenCsrTypeIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_NE(nullptr, &gfxCoreHelper);

    auto sipType = gfxCoreHelper.getSipKernelType(false);
    EXPECT_EQ(SipKernelType::csr, sipType);
}

TEST_F(GfxCoreHelperTest, givenEngineTypeRcsWhenCsTraitsAreQueiredThenCorrectNameInTraitsIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_NE(nullptr, &gfxCoreHelper);

    auto &csTraits = gfxCoreHelper.getCsTraits(aub_stream::ENGINE_RCS);
    EXPECT_STREQ("RCS", csTraits.name.c_str());
}

TEST_F(GfxCoreHelperTest, whenGetGpuTimeStampInNSIsCalledThenTimestampIsMaskedBasedOnResolution) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    auto timeStamp0 = 0x00ff'ffff'ffff;
    auto timeStamp1 = 0xfe00'00ff'ffff'ffff;
    auto timeStamp2 = 0xff00'00ff'ffff'ffff;
    {
        auto resolution = 135.0;
        auto result = static_cast<uint64_t>(timeStamp0 * resolution);
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp0, resolution));
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp1, resolution));
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp2, resolution));
    }
    {
        auto resolution = 128.0;
        auto result = static_cast<uint64_t>(timeStamp0 * resolution);
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp0, resolution));
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp1, resolution));
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp2, resolution));
    }
    {
        auto resolution = 127.0;
        auto result = static_cast<uint64_t>(timeStamp0 * resolution);
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp0, resolution));
        EXPECT_EQ(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp1, resolution));
        EXPECT_NE(result, gfxCoreHelper.getGpuTimeStampInNS(timeStamp2, resolution));
    }
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
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);

    LriHelper<FamilyType>::program(&stream, address, data, false, false);
    auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(address, lri->getRegisterOffset());
    EXPECT_EQ(data, lri->getDataDword());
}

HWTEST_F(LriHelperTests, givenAddressAndOffsetAndRemapAndNotBlitterWhenHelperIsUsedThenProgramCmdStream) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);

    LriHelper<FamilyType>::program(&stream, address, data, true, false);
    auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(address, lri->getRegisterOffset());
    EXPECT_EQ(data, lri->getDataDword());
}

HWTEST_F(LriHelperTests, givenAddressAndOffsetAndNotRemapAndBlitterWhenHelperIsUsedThenProgramCmdStream) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);

    LriHelper<FamilyType>::program(&stream, address, data, false, true);
    auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(address, lri->getRegisterOffset());
    EXPECT_EQ(data, lri->getDataDword());
}

HWTEST_F(LriHelperTests, givenAddressAndOffsetAndRemapAndBlitterWhenHelperIsUsedThenProgramCmdStream) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);

    LriHelper<FamilyType>::program(&stream, address, data, true, true);
    auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(address + RegisterOffsets::bcs0Base, lri->getRegisterOffset());
    EXPECT_EQ(data, lri->getDataDword());
}

using PipeControlHelperTests = ::testing::Test;

HWTEST_F(PipeControlHelperTests, givenPostSyncWriteTimestampModeWhenHelperIsUsedThenProperFieldsAreProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654321;
    uint64_t immediateData = 0x1234;

    auto expectedPipeControl = FamilyType::cmdInitPipeControl;
    expectedPipeControl.setCommandStreamerStallEnable(true);
    expectedPipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    expectedPipeControl.setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    expectedPipeControl.setAddressHigh(static_cast<uint32_t>(address >> 32));

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        stream, PostSyncMode::timestamp, address, immediateData, rootDeviceEnvironment, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::timestamp) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
    void *cpuPipeControlBuffer = ptrOffset(stream.getCpuBase(), pipeControlLocationSize);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(cpuPipeControlBuffer);
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(cpuPipeControlBuffer, args.postSyncCmd);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_EQ(pipeControl->getCommandStreamerStallEnable(), expectedPipeControl.getCommandStreamerStallEnable());
    EXPECT_EQ(pipeControl->getPostSyncOperation(), expectedPipeControl.getPostSyncOperation());
    EXPECT_EQ(pipeControl->getAddress(), expectedPipeControl.getAddress());
    EXPECT_EQ(pipeControl->getAddressHigh(), expectedPipeControl.getAddressHigh());
    EXPECT_EQ(pipeControl->getImmediateData(), expectedPipeControl.getImmediateData());
    EXPECT_EQ(pipeControl->getNotifyEnable(), expectedPipeControl.getNotifyEnable());
}

HWTEST_F(PipeControlHelperTests, givenGfxCoreHelperwhenAskingForDcFlushThenReturnTrue) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
}

HWTEST_F(PipeControlHelperTests, givenDcFlushNotAllowedWhenProgrammingPipeControlThenDontSetDcFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);

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
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654321;
    uint64_t immediateData = 0x1234;

    auto expectedPipeControl = FamilyType::cmdInitPipeControl;
    expectedPipeControl.setCommandStreamerStallEnable(true);
    expectedPipeControl.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA);
    expectedPipeControl.setAddress(static_cast<uint32_t>(address & 0x0000FFFFFFFFULL));
    expectedPipeControl.setAddressHigh(static_cast<uint32_t>(address >> 32));
    expectedPipeControl.setImmediateData(immediateData);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    PipeControlArgs args{};
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        stream, PostSyncMode::immediateData, address, immediateData, rootDeviceEnvironment, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
    void *cpuPipeControlBuffer = ptrOffset(stream.getCpuBase(), pipeControlLocationSize);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(cpuPipeControlBuffer);
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(cpuPipeControlBuffer, args.postSyncCmd);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_EQ(pipeControl->getCommandStreamerStallEnable(), expectedPipeControl.getCommandStreamerStallEnable());
    EXPECT_EQ(pipeControl->getPostSyncOperation(), expectedPipeControl.getPostSyncOperation());
    EXPECT_EQ(pipeControl->getAddress(), expectedPipeControl.getAddress());
    EXPECT_EQ(pipeControl->getAddressHigh(), expectedPipeControl.getAddressHigh());
    EXPECT_EQ(pipeControl->getImmediateData(), expectedPipeControl.getImmediateData());
    EXPECT_EQ(pipeControl->getNotifyEnable(), expectedPipeControl.getNotifyEnable());
}

HWTEST_F(PipeControlHelperTests, givenNotifyEnableArgumentIsTrueWhenHelperIsUsedThenNotifyEnableFlagIsTrue) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);

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
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    PipeControlArgs args;
    args.notifyEnable = true;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        stream, PostSyncMode::immediateData, address, immediateData, rootDeviceEnvironment, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), pipeControlLocationSize));
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_EQ(pipeControl->getCommandStreamerStallEnable(), expectedPipeControl.getCommandStreamerStallEnable());
    EXPECT_EQ(pipeControl->getPostSyncOperation(), expectedPipeControl.getPostSyncOperation());
    EXPECT_EQ(pipeControl->getAddress(), expectedPipeControl.getAddress());
    EXPECT_EQ(pipeControl->getAddressHigh(), expectedPipeControl.getAddressHigh());
    EXPECT_EQ(pipeControl->getImmediateData(), expectedPipeControl.getImmediateData());
    EXPECT_EQ(pipeControl->getNotifyEnable(), expectedPipeControl.getNotifyEnable());
}

HWTEST_F(PipeControlHelperTests, WhenIsDcFlushAllowedIsCalledThenCorrectResultIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(false, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
    EXPECT_EQ(productHelper.isDcFlushAllowed(), MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]));

    DebugManagerStateRestore restorer;

    debugManager.flags.AllowDcFlush.set(0);
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
    EXPECT_EQ(productHelper.isDcFlushAllowed(), MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]));

    debugManager.flags.AllowDcFlush.set(1);
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
    EXPECT_EQ(productHelper.isDcFlushAllowed(), MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
}

HWTEST_F(PipeControlHelperTests, WhenPipeControlPostSyncTimestampUsedThenCorrectPostSyncUsed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654320;
    uint64_t immediateData = 0x0;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(
        stream, PostSyncMode::timestamp, address, immediateData, args);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(stream.getCpuBase());
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_EQ(address, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(immediateData, pipeControl->getImmediateData());
    EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, pipeControl->getPostSyncOperation());
}

HWTEST_F(PipeControlHelperTests, WhenPipeControlPostSyncWriteImmediateDataUsedThenCorrectPostSyncUsed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    uint64_t address = 0x1234567887654320;
    uint64_t immediateData = 0x1234;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(
        stream, PostSyncMode::immediateData, address, immediateData, args);
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
    debugManager.flags.NodeOrdinal.set(aub_stream::ENGINE_VECS);
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
    SurfaceStateBufferLength length;
    length.length = static_cast<uint32_t>(size - 1);
    uint64_t addr = 0x2000;
    size_t offset = 0x1000;
    uint32_t pitch = 0x40;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, true, false);

    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);
    EXPECT_EQ(length.surfaceState.depth + 1u, state->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, state->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, state->getHeight());
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(pitch, EncodeSurfaceState<FamilyType>::getPitchForScratchInBytes(state, productHelper));
    addr += offset;
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());
    EXPECT_EQ(type, state->getSurfaceType());
    EXPECT_EQ(gmmHelper->getL3EnabledMOCS(), state->getMemoryObjectControlState());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1003;
    length.length = static_cast<uint32_t>(alignUp(size, 4) - 1);
    bool isReadOnly = false;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getUncachedMOCS(), state->getMemoryObjectControlState());
    EXPECT_EQ(length.surfaceState.depth + 1u, state->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, state->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, state->getHeight());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1000;
    addr = 0x2001;
    length.length = static_cast<uint32_t>(size - 1);
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getUncachedMOCS(), state->getMemoryObjectControlState());
    EXPECT_EQ(length.surfaceState.depth + 1u, state->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, state->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1005;
    length.length = static_cast<uint32_t>(alignUp(size, 4) - 1);
    isReadOnly = true;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getL3EnabledMOCS(), state->getMemoryObjectControlState());
    EXPECT_EQ(length.surfaceState.depth + 1u, state->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, state->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    alignedFree(stateBuffer);
}

HWTEST2_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenAllocationProvidedThenUseAllocationAsInput, MatchAny) {
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
    SurfaceStateBufferLength length;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    length.length = static_cast<uint32_t>(allocSize - 1);
    GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::memoryNull, 0u);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation.setDefaultGmm(new Gmm(pDevice->getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, true, false);
    EXPECT_EQ(length.surfaceState.depth + 1u, state->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, state->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, state->getHeight());
    EXPECT_EQ(pitch, state->getSurfacePitch() - 1u);
    EXPECT_EQ(gpuAddr, state->getSurfaceBaseAddress());

    if constexpr (IsAtMostXeCore::isMatched<productFamily>()) {
        EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    }
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST2_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxDisabledThenSetCoherencyToGpuAndAuxModeToCompression, MatchAny) {
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
    GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::memoryNull, 0u);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    allocation.getDefaultGmm()->setCompressionEnabled(true);
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);
    if constexpr (IsAtMostXeCore::isMatched<productFamily>()) {
        EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, state->getCoherencyType());
    }
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(state, allocation.getDefaultGmm()));

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST2_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenGmmCompressionDisabledAndAllocationEnabledAnNonAuxDisabledThenSetCoherencyToIaAndAuxModeToNone, MatchAny) {
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
    GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::memoryNull, 1);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);
    if constexpr (IsAtMostXeCore::isMatched<productFamily>()) {
        EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    }
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(GfxCoreHelperTest, givenOverrideMocsIndexForScratchSpaceWhenSurfaceStateIsProgrammedForScratchSpaceThenOverrideMocsIndexWithCorrectValue) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideMocsIndexForScratchSpace.set(1);

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
    GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::memoryNull, 1);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);

    auto mocsProgrammed = state->getMemoryObjectControlState() >> 1;
    EXPECT_EQ(1u, mocsProgrammed);

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST2_F(GfxCoreHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxEnabledThenSetCoherencyToIaAndAuxModeToNone, MatchAny) {
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
    GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::buffer, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::memoryNull, 1u);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmHelper(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    allocation.getDefaultGmm()->setCompressionEnabled(true);
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    gfxCoreHelper.setRenderSurfaceStateForScratchResource(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, true, false);
    if constexpr (IsAtMostXeCore::isMatched<productFamily>()) {
        EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    }
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
    debugManager.flags.EnableCacheFlushAfterWalker.set(1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_TRUE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(GfxCoreHelperCacheFlushTest, givenEnableCacheFlushFlagIsDisableWhenPlatformSupportsThenOverrideAndReturnSupportFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(0);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_FALSE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(GfxCoreHelperCacheFlushTest, givenEnableCacheFlushFlagIsReadPlatformSettingWhenPlatformDoesNotSupportThenReturnSupportFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(-1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_FALSE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(GfxCoreHelperCacheFlushTest, givenEnableCacheFlushFlagIsReadPlatformSettingWhenPlatformSupportsThenReturnSupportTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableCacheFlushAfterWalker.set(-1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_TRUE(GfxCoreHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, givenGfxCoreHelperWhenGettingGlobalTimeStampBitsThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getGlobalTimeStampBits(), 36U);
}

TEST_F(GfxCoreHelperTest, givenAUBDumpForceAllToLocalMemoryDebugVarWhenSetThenGetEnableLocalMemoryReturnsCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    debugManager.flags.AUBDumpForceAllToLocalMemory.set(true);
    EXPECT_TRUE(gfxCoreHelper.getEnableLocalMemory(hardwareInfo));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, givenVariousCachesRequestThenCorrectMocsIndexesAreReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmmHelper = this->pDevice->getGmmHelper();
    auto expectedMocsForL3off = gmmHelper->getUncachedMOCS() >> 1;
    auto expectedMocsForL3on = gmmHelper->getL3EnabledMOCS() >> 1;
    auto expectedMocsForL3andL1on = gmmHelper->getL1EnabledMOCS() >> 1;

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

HWTEST_F(GfxCoreHelperTest, givenDebugVariableSetWhenAskingForAuxTranslationModeThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    EXPECT_EQ(UnitTestHelper<FamilyType>::requiredAuxTranslationMode, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    if (GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo) == AuxTranslationMode::blit) {
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        EXPECT_EQ(AuxTranslationMode::builtin, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));
    }

    debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::none));
    EXPECT_EQ(AuxTranslationMode::none, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    hwInfo.capabilityTable.blitterOperationsSupported = false;
    debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::blit));
    EXPECT_EQ(AuxTranslationMode::builtin, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    hwInfo.capabilityTable.blitterOperationsSupported = true;
    debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::blit));
    EXPECT_EQ(AuxTranslationMode::blit, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    debugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::builtin));
    EXPECT_EQ(AuxTranslationMode::builtin, GfxCoreHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));
}

HWTEST_F(GfxCoreHelperTest, givenDebugFlagWhenCheckingIfBufferIsSuitableForCompressionThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    debugManager.flags.OverrideBufferSuitableForRenderCompression.set(0);
    EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(0));
    EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(MemoryConstants::kiloByte));
    EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(MemoryConstants::kiloByte + 1));

    debugManager.flags.OverrideBufferSuitableForRenderCompression.set(1);
    EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(0));
    EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(MemoryConstants::kiloByte));
    EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(MemoryConstants::kiloByte + 1));
}

HWTEST_F(GfxCoreHelperTest, WhenIsBankOverrideRequiredIsCalledThenFalseIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(gfxCoreHelper.isBankOverrideRequired(hardwareInfo, productHelper));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, GivenBarrierEncodingWhenCallingGetBarriersCountFromHasBarrierThenNumberOfBarriersIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(0u, gfxCoreHelper.getBarriersCountFromHasBarriers(0u));
    EXPECT_EQ(1u, gfxCoreHelper.getBarriersCountFromHasBarriers(1u));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, GivenVariousValuesWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto result = gfxCoreHelper.calculateAvailableThreadCount(hardwareInfo, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_EQ(hardwareInfo.gtSystemInfo.ThreadCount, result);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, GivenModifiedGtSystemInfoWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto hwInfo = hardwareInfo;
    for (auto threadCount : {1u, 5u, 9u}) {
        hwInfo.gtSystemInfo.ThreadCount = threadCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
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

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenIsWorkaroundRequiredCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
}

HWTEST_F(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenMinimalSIMDSizeIsQueriedThen8IsReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(8u, gfxCoreHelper.getMinimalSIMDSize());
}

HWTEST_F(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenGettingPreferredVectorWidthsThenCorrectValuesAreReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getPreferredVectorWidthChar(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(8u, gfxCoreHelper.getPreferredVectorWidthShort(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(4u, gfxCoreHelper.getPreferredVectorWidthInt(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(1u, gfxCoreHelper.getPreferredVectorWidthLong(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(1u, gfxCoreHelper.getPreferredVectorWidthFloat(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(8u, gfxCoreHelper.getPreferredVectorWidthHalf(gfxCoreHelper.getMinimalSIMDSize()));
}

HWTEST_F(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenGettingNativeVectorWidthsThenCorrectValuesAreReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(16u, gfxCoreHelper.getNativeVectorWidthChar(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(8u, gfxCoreHelper.getNativeVectorWidthShort(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(4u, gfxCoreHelper.getNativeVectorWidthInt(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(1u, gfxCoreHelper.getNativeVectorWidthLong(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(1u, gfxCoreHelper.getNativeVectorWidthFloat(gfxCoreHelper.getMinimalSIMDSize()));
    EXPECT_EQ(8u, gfxCoreHelper.getNativeVectorWidthHalf(gfxCoreHelper.getMinimalSIMDSize()));
}

HWTEST_F(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenMinimalGrfSizeIsQueriedThen128IsReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(128u, gfxCoreHelper.getMinimalGrfSize());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, WhenIsFusedEuDispatchEnabledIsCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isFusedEuDispatchEnabled(hardwareInfo, false));
}

HWTEST_F(PipeControlHelperTests, WhenGettingPipeControSizeForInstructionCacheFlushThenReturnCorrectValue) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    size_t actualSize = MemorySynchronizationCommands<FamilyType>::getSizeForInstructionCacheFlush();
    EXPECT_EQ(sizeof(PIPE_CONTROL), actualSize);
}

HWTEST_F(PipeControlHelperTests, WhenProgrammingInstructionCacheFlushThenExpectInstructionCacheInvalidateEnableFieldSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    auto buffer = std::make_unique<uint8_t[]>(128);

    LinearStream stream(buffer.get(), 128);
    MockExecutionEnvironment mockExecutionEnvironment{};
    MemorySynchronizationCommands<FamilyType>::addInstructionCacheFlush(stream);
    PIPE_CONTROL *pipeControl = genCmdCast<PIPE_CONTROL *>(buffer.get());
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_TRUE(pipeControl->getInstructionCacheInvalidateEnable());
}

using ProductHelperCommonTest = Test<DeviceFixture>;

HWTEST2_F(ProductHelperCommonTest, givenBlitterPreferenceWhenEnablingBlitterOperationsSupportThenHonorThePreference, MatchAny) {
    HardwareInfo hardwareInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    productHelper.configureHardwareCustom(&hardwareInfo, nullptr);

    const auto expectedBlitterSupport = productHelper.obtainBlitterPreference(hardwareInfo);
    EXPECT_EQ(expectedBlitterSupport, hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWTEST2_F(ProductHelperCommonTest, whenCallIsAvailableExtendedScratchThenReturnFalse, IsAtMostXe3Core) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isAvailableExtendedScratch());
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

    debugManager.flags.EnableLocalMemory.set(true);
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, gfxCoreHelper.useSystemMemoryPlacementForISA(hardwareInfo));

    debugManager.flags.EnableLocalMemory.set(false);
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, gfxCoreHelper.useSystemMemoryPlacementForISA(hardwareInfo));
}

TEST_F(GfxCoreHelperTest, givenInvalidEngineTypeWhenGettingEngineGroupTypeThenThrow) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_ANY_THROW(gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::NUM_ENGINES, EngineUsage::regular, hardwareInfo));
    EXPECT_ANY_THROW(gfxCoreHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_VECS, EngineUsage::regular, hardwareInfo));
}

HWTEST2_F(ProductHelperCommonTest, givenDebugFlagSetWhenEnablingBlitterOperationsSupportThenHonorTheFlag, MatchAny) {
    DebugManagerStateRestore restore{};
    HardwareInfo hardwareInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();

    debugManager.flags.EnableBlitterOperationsSupport.set(1);
    productHelper.configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_TRUE(hardwareInfo.capabilityTable.blitterOperationsSupported);

    debugManager.flags.EnableBlitterOperationsSupport.set(0);
    productHelper.configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_FALSE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, GivenVariousValuesWhenAlignSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
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

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, GivenVariousValuesWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(0u, gfxCoreHelper.computeSlmValues(hwInfo, 0, nullptr, false));
    EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 1, nullptr, false));
    EXPECT_EQ(1u, gfxCoreHelper.computeSlmValues(hwInfo, 1024, nullptr, false));
    EXPECT_EQ(2u, gfxCoreHelper.computeSlmValues(hwInfo, 1025, nullptr, false));
    EXPECT_EQ(2u, gfxCoreHelper.computeSlmValues(hwInfo, 2048, nullptr, false));
    EXPECT_EQ(3u, gfxCoreHelper.computeSlmValues(hwInfo, 2049, nullptr, false));
    EXPECT_EQ(3u, gfxCoreHelper.computeSlmValues(hwInfo, 4096, nullptr, false));
    EXPECT_EQ(4u, gfxCoreHelper.computeSlmValues(hwInfo, 4097, nullptr, false));
    EXPECT_EQ(4u, gfxCoreHelper.computeSlmValues(hwInfo, 8192, nullptr, false));
    EXPECT_EQ(5u, gfxCoreHelper.computeSlmValues(hwInfo, 8193, nullptr, false));
    EXPECT_EQ(5u, gfxCoreHelper.computeSlmValues(hwInfo, 16384, nullptr, false));
    EXPECT_EQ(6u, gfxCoreHelper.computeSlmValues(hwInfo, 16385, nullptr, false));
    EXPECT_EQ(6u, gfxCoreHelper.computeSlmValues(hwInfo, 32768, nullptr, false));
    EXPECT_EQ(7u, gfxCoreHelper.computeSlmValues(hwInfo, 32769, nullptr, false));
    EXPECT_EQ(7u, gfxCoreHelper.computeSlmValues(hwInfo, 65536, nullptr, false));
}

HWTEST2_F(GfxCoreHelperTest, GivenZeroSlmSizeWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned, IsHeapfulRequired) {
    using SHARED_LOCAL_MEMORY_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto receivedSlmSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(gfxCoreHelper.computeSlmValues(hwInfo, 0, nullptr, false));
    EXPECT_EQ(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_SLM_ENCODES_0K, receivedSlmSize);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, GfxCoreHelperTest, givenGfxCoreHelperWhenGettingPlanarYuvHeightThenHelperReturnsCorrectValue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getPlanarYuvMaxHeight(), 16352u);
}

TEST_F(GfxCoreHelperTest, WhenGettingIsCpuImageTransferPreferredThenFalseIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isCpuImageTransferPreferred(*defaultHwInfo));
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

HWTEST2_F(GfxCoreHelperTest, whenSetNotCompressedFlagThenProperValueReturned, IsAtLeastXe2HpgCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto gmmFlags = gmm->gmmResourceInfo->getResourceFlags();
    gmmFlags->Info.NotCompressed = 0;
    EXPECT_TRUE(gfxCoreHelper.isCompressionAppliedForImportedResource(*gmm));
}

HWTEST2_F(GfxCoreHelperTest, whenSetRenderCompressedFlagThenProperValueReturned, IsAtMostDg2) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto gmmFlags = gmm->gmmResourceInfo->getResourceFlags();
    gmmFlags->Info.RenderCompressed = 1;
    gmmFlags->Info.MediaCompressed = 0;
    EXPECT_TRUE(gfxCoreHelper.isCompressionAppliedForImportedResource(*gmm));
}

HWTEST2_F(GfxCoreHelperTest, whenCheckingCrossEngineCacheFlushRequirementThenReturnTrue, IsAtMostDg2) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.crossEngineCacheFlushRequired());
}

HWTEST2_F(GfxCoreHelperTest, whenSetMediaCompressedFlagThenProperValueReturned, IsAtMostDg2) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto gmmFlags = gmm->gmmResourceInfo->getResourceFlags();
    gmmFlags->Info.RenderCompressed = 0;
    gmmFlags->Info.MediaCompressed = 1;
    EXPECT_TRUE(gfxCoreHelper.isCompressionAppliedForImportedResource(*gmm));
}

HWTEST2_F(GfxCoreHelperTest, whenSetRenderAndMediaCompressedFlagsThenProperValueReturned, IsAtMostDg2) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto gmmFlags = gmm->gmmResourceInfo->getResourceFlags();
    gmmFlags->Info.RenderCompressed = 1;
    gmmFlags->Info.MediaCompressed = 1;
    EXPECT_TRUE(gfxCoreHelper.isCompressionAppliedForImportedResource(*gmm));
}

class MockGmmResourceInfoWithDenyCompression : public MockGmmResourceInfo {
  public:
    MockGmmResourceInfoWithDenyCompression(GMM_RESCREATE_PARAMS *resourceCreateParams) : MockGmmResourceInfo(resourceCreateParams) {}
    MockGmmResourceInfoWithDenyCompression(GMM_RESOURCE_INFO *inputGmmResourceInfo) : MockGmmResourceInfo(inputGmmResourceInfo) {}

    bool isResourceDenyCompressionEnabled() override {
        return true;
    }
};

HWTEST_F(GfxCoreHelperTest, givenResourceDenyCompressionEnabledWhenIsCompressionAppliedForImportedResourceCalledThenReturnsFalse) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    // Create GMM params for a compressed resource
    GMM_RESCREATE_PARAMS gmmParams = {};
    gmmParams.Type = GMM_RESOURCE_TYPE::RESOURCE_BUFFER;
    gmmParams.Format = GMM_FORMAT_GENERIC_8BIT;
    gmmParams.BaseWidth64 = 1024;
    gmmParams.BaseHeight = 1;
    gmmParams.Depth = 1;
    gmmParams.Flags.Info.NotCompressed = 0; // Compression enabled in flags

    // Create our custom mock that returns true for isResourceDenyCompressionEnabled
    auto mockGmmResourceInfo = std::make_unique<MockGmmResourceInfoWithDenyCompression>(&gmmParams);
    MockGmm mockGmm(pDevice->getGmmHelper());
    mockGmm.gmmResourceInfo.reset(mockGmmResourceInfo.release());

    // Even though NotCompressed = 0 (compression enabled), the deny compression should override it
    EXPECT_FALSE(gfxCoreHelper.isCompressionAppliedForImportedResource(mockGmm));
}

HWTEST_F(GfxCoreHelperTest, givenResourceDenyCompressionEnabledWhenRenderAndMediaCompressionAreSetThenIsCompressionAppliedIsFalse) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    // Create GMM params for a compressed resource
    GMM_RESCREATE_PARAMS gmmParams = {};
    gmmParams.Type = GMM_RESOURCE_TYPE::RESOURCE_BUFFER;
    gmmParams.Format = GMM_FORMAT_GENERIC_8BIT;
    gmmParams.BaseWidth64 = 1024;
    gmmParams.BaseHeight = 1;
    gmmParams.Depth = 1;
    gmmParams.Flags.Info.RenderCompressed = 1;
    gmmParams.Flags.Info.MediaCompressed = 1;

    // Create our custom mock that returns true for isResourceDenyCompressionEnabled
    auto mockGmmResourceInfo = std::make_unique<MockGmmResourceInfoWithDenyCompression>(&gmmParams);
    MockGmm mockGmm(pDevice->getGmmHelper());
    mockGmm.gmmResourceInfo.reset(mockGmmResourceInfo.release());

    // Even though Render and Media compression are enabled, the deny compression should override it
    EXPECT_FALSE(gfxCoreHelper.isCompressionAppliedForImportedResource(mockGmm));
}

HWTEST_F(GfxCoreHelperTest, whenAdjustPreemptionSurfaceSizeIsCalledThenCsrSizeDoesntChange) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    size_t csrSize = 1024;
    size_t oldCsrSize = csrSize;
    gfxCoreHelper.adjustPreemptionSurfaceSize(csrSize, pDevice->getRootDeviceEnvironment());
    EXPECT_EQ(oldCsrSize, csrSize);
}

HWTEST_F(GfxCoreHelperTest, whenSetSipKernelDataIsCalledThenSipKernelDataDoesntChange) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    uint32_t *sipKernelBinary = nullptr;
    uint32_t *oldSipKernelBinary = sipKernelBinary;
    size_t kernelBinarySize = 1024;
    size_t oldKernelBinarySize = kernelBinarySize;
    gfxCoreHelper.setSipKernelData(sipKernelBinary, kernelBinarySize, pDevice->getRootDeviceEnvironment());
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

HWTEST2_F(GfxCoreHelperTest, givenAtMostGen12lpPlatformiWhenCheckingIfScratchSpaceSurfaceStateAccessibleThenFalseIsReturned, IsGen12LP) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isScratchSpaceSurfaceStateAccessible());
}

HWTEST2_F(GfxCoreHelperTest, givenAtLeastXeHpPlatformWhenCheckingIfScratchSpaceSurfaceStateAccessibleTheniTrueIsReturned, IsAtLeastXeCore) {
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
    const auto &productHelper = getHelper<ProductHelper>();
    EncodeSurfaceState<FamilyType>::setPitchForScratch(&renderSurfaceState, expectedPitch, productHelper);
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(expectedPitch, gfxCoreHelper.getRenderSurfaceStatePitch(&renderSurfaceState, productHelper));
}

HWTEST_F(GfxCoreHelperTest, whenBlitterSupportIsDisabledThenDontExposeAnyBcsEngine) {
    auto &hwInfo = *this->pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto engineUsageTypes = gfxCoreHelper.getGpgpuEngineInstances(*this->pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    for (auto &engineUsageType : engineUsageTypes) {
        EXPECT_FALSE(EngineHelpers::isBcs(engineUsageType.first));
    }
}

struct CoherentWANotNeeded {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return !TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceGpuNonCoherent;
    }
};

struct ForceNonCoherentMode {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceGpuNonCoherent;
    }
};

HWTEST_F(GfxCoreHelperTest, GivenHwInfoWhenGetBatchBufferEndSizeCalledThenCorrectSizeReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getBatchBufferEndSize(), sizeof(typename FamilyType::MI_BATCH_BUFFER_END));
}

HWTEST_F(GfxCoreHelperTest, GivenHwInfoWhenGetBatchBufferEndReferenceCalledThenCorrectPtrReturned) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getBatchBufferEndReference(), reinterpret_cast<const void *>(&FamilyType::cmdInitBatchBufferEnd));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenPassingCopyEngineTypeThenItsCopyOnly) {
    EXPECT_TRUE(EngineHelper::isCopyOnlyEngineType(EngineGroupType::copy));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenPassingLinkedCopyEngineTypeThenItsCopyOnly) {
    EXPECT_TRUE(EngineHelper::isCopyOnlyEngineType(EngineGroupType::linkedCopy));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenPassingComputeEngineTypeThenItsNotCopyOnly) {
    EXPECT_FALSE(EngineHelper::isCopyOnlyEngineType(EngineGroupType::compute));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenAskingForRelaxedOrderingSupportThenReturnFalse) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isRelaxedOrderingSupported());
}

HWTEST2_F(GfxCoreHelperTest, givenGfxCoreHelperWhenCallCopyThroughLockedPtrEnabledThenReturnFalse, IsNotXeCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo, productHelper));
}

HWTEST_F(GfxCoreHelperTest, givenGfxCoreHelperWhenFlagSetAndCallCopyThroughLockedPtrEnabledThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    debugManager.flags.ExperimentalCopyThroughLock.set(0);
    EXPECT_FALSE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo, productHelper));

    debugManager.flags.ExperimentalCopyThroughLock.set(1);
    EXPECT_TRUE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo, productHelper));
}

HWTEST2_F(GfxCoreHelperTest, givenGfxCoreHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue, IsGen12LP) {
    DebugManagerStateRestore restorer;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    debugManager.flags.SetAmountOfReusableAllocations.set(0);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 0u);

    debugManager.flags.SetAmountOfReusableAllocations.set(1);
    EXPECT_EQ(gfxCoreHelper.getAmountOfAllocationsToFill(), 1u);
}

HWTEST2_F(GfxCoreHelperTest, givenAtMostGen12lpPlatformWhenGettingMinimalScratchSpaceSizeThen1024IsReturned, IsGen12LP) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(1024U, gfxCoreHelper.getMinimalScratchSpaceSize());
}

HWTEST2_F(GfxCoreHelperTest, givenAtLeastXeHpPlatformWhenGettingMinimalScratchSpaceSizeThen64IsReturned, IsAtLeastXeCore) {
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

HWTEST_F(GfxCoreHelperTest, WhenIsDynamicallyPopulatedIsFalseThenGetHighestEnabledDualSubSliceReturnsMaxDualSubSlicesSupported) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = false;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 16;
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto maxDualSubSlice = gfxCoreHelper.getHighestEnabledDualSubSlice(hwInfo);
    EXPECT_EQ(maxDualSubSlice, hwInfo.gtSystemInfo.MaxDualSubSlicesSupported);
}

TEST_F(GfxCoreHelperTest, givenMaxDualSubSlicesSupportedAndNoDSSInfoThenMaxEnabledDualSubSliceIsCalculatedBasedOnMaxDSSPerSlice) {
    HardwareInfo hwInfo = *defaultHwInfo.get();

    constexpr uint32_t numFusedOutSlices = 2u;
    constexpr uint32_t maxSlices = 2u;
    constexpr uint32_t maxSliceIndex = numFusedOutSlices + maxSlices - 1;
    constexpr uint32_t dualSubSlicesPerSlice = 5u;

    constexpr uint32_t maxDualSubSliceEnabled = 20u;

    hwInfo.gtSystemInfo.MaxSlicesSupported = maxSlices;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 0u;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = maxSlices * dualSubSlicesPerSlice;
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;

    for (uint32_t slice = 0u; slice < GT_MAX_SLICE; slice++) {
        hwInfo.gtSystemInfo.SliceInfo[slice].Enabled = false;
        for (uint32_t dualSubSlice = 0; dualSubSlice < GT_MAX_DUALSUBSLICE_PER_SLICE; dualSubSlice++) {
            hwInfo.gtSystemInfo.SliceInfo[slice].DSSInfo[dualSubSlice].Enabled = false;
        }
        for (uint32_t subSlice = 0; subSlice < GT_MAX_SUBSLICE_PER_SLICE; subSlice++) {
            hwInfo.gtSystemInfo.SliceInfo[slice].SubSliceInfo[subSlice].Enabled = false;
        }
    }
    hwInfo.gtSystemInfo.SliceInfo[maxSliceIndex].Enabled = true;

    EXPECT_EQ(maxDualSubSliceEnabled, GfxCoreHelper::getHighestEnabledDualSubSlice(hwInfo));
}

TEST_F(GfxCoreHelperTest, givenNoMaxDualSubSlicesSupportedThenMaxEnabledDualSubSliceIsCalculatedBasedOnSubSlices) {
    HardwareInfo hwInfo = *defaultHwInfo.get();

    constexpr uint32_t maxSlices = 4u;
    constexpr uint32_t maxSliceIndex = maxSlices - 1;
    constexpr uint32_t subSlicesPerSlice = 5u;

    constexpr uint32_t maxSubSliceEnabledOnLastSliceIndex = 6;
    constexpr uint32_t maxSubSliceEnabled = 22u;

    hwInfo.gtSystemInfo.MaxSlicesSupported = maxSlices;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = maxSlices * subSlicesPerSlice;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 0u;
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;

    for (uint32_t slice = 0u; slice < GT_MAX_SLICE; slice++) {
        hwInfo.gtSystemInfo.SliceInfo[slice].Enabled = false;
        for (uint32_t dualSubSlice = 0; dualSubSlice < GT_MAX_DUALSUBSLICE_PER_SLICE; dualSubSlice++) {
            hwInfo.gtSystemInfo.SliceInfo[slice].DSSInfo[dualSubSlice].Enabled = false;
        }
        for (uint32_t subSlice = 0; subSlice < GT_MAX_SUBSLICE_PER_SLICE; subSlice++) {
            hwInfo.gtSystemInfo.SliceInfo[slice].SubSliceInfo[subSlice].Enabled = false;
        }
    }
    hwInfo.gtSystemInfo.SliceInfo[maxSliceIndex].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[maxSliceIndex].SubSliceInfo[maxSubSliceEnabledOnLastSliceIndex].Enabled = true;

    EXPECT_EQ(maxSubSliceEnabled, GfxCoreHelper::getHighestEnabledDualSubSlice(hwInfo));
}

TEST_F(GfxCoreHelperTest, givenNoMaxDualSubSlicesSupportedAndNoSubSliceInfoThenMaxEnabledDualSubSliceIsCalculatedBasedOnMaxSubSlicesPerSlice) {
    HardwareInfo hwInfo = *defaultHwInfo.get();

    constexpr uint32_t numFusedOutSlices = 2u;
    constexpr uint32_t maxSlices = 2u;
    constexpr uint32_t maxSliceIndex = numFusedOutSlices + maxSlices - 1;
    constexpr uint32_t subSlicesPerSlice = 5u;

    constexpr uint32_t maxSubSliceEnabled = 20u;

    hwInfo.gtSystemInfo.MaxSlicesSupported = maxSlices;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = maxSlices * subSlicesPerSlice;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 0u;
    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;

    for (uint32_t slice = 0u; slice < GT_MAX_SLICE; slice++) {
        hwInfo.gtSystemInfo.SliceInfo[slice].Enabled = false;
        for (uint32_t dualSubSlice = 0; dualSubSlice < GT_MAX_DUALSUBSLICE_PER_SLICE; dualSubSlice++) {
            hwInfo.gtSystemInfo.SliceInfo[slice].DSSInfo[dualSubSlice].Enabled = false;
        }
        for (uint32_t subSlice = 0; subSlice < GT_MAX_SUBSLICE_PER_SLICE; subSlice++) {
            hwInfo.gtSystemInfo.SliceInfo[slice].SubSliceInfo[subSlice].Enabled = false;
        }
    }
    hwInfo.gtSystemInfo.SliceInfo[maxSliceIndex].Enabled = true;

    EXPECT_EQ(maxSubSliceEnabled, GfxCoreHelper::getHighestEnabledDualSubSlice(hwInfo));
}

HWTEST2_F(GfxCoreHelperTest, givenLargeGrfIsNotSupportedWhenCalculatingMaxWorkGroupSizeThenAlwaysReturnDeviceDefault, IsGen12LP) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto defaultMaxGroupSize = 42u;

    NEO::KernelDescriptor kernelDescriptor{};
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    kernelDescriptor.kernelAttributes.simdSize = 16;
    kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, defaultMaxGroupSize, rootDeviceEnvironment));

    kernelDescriptor.kernelAttributes.simdSize = 32;
    kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, defaultMaxGroupSize, rootDeviceEnvironment));

    kernelDescriptor.kernelAttributes.simdSize = 16;
    kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.calculateMaxWorkGroupSize(kernelDescriptor, defaultMaxGroupSize, rootDeviceEnvironment));
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

HWTEST_F(GfxCoreHelperTest, WhenIsDynamicallyPopulatedIsTrueThenGetHighestEnabledDualSubSliceReturnsHighestEnabledDualSubSliceId) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.IsDynamicallyPopulated = true;
    hwInfo.gtSystemInfo.MaxSlicesSupported = 1;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = 4;

    for (auto &sliceInfo : hwInfo.gtSystemInfo.SliceInfo) {
        sliceInfo.Enabled = false;
        for (auto &dssInfo : sliceInfo.DSSInfo) {
            dssInfo.Enabled = false;
        }
    }

    hwInfo.gtSystemInfo.SliceInfo[3].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[0].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[1].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[2].Enabled = true;
    hwInfo.gtSystemInfo.SliceInfo[3].DSSInfo[3].Enabled = true;

    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto maxDualSubSlice = gfxCoreHelper.getHighestEnabledDualSubSlice(hwInfo);
    EXPECT_EQ(maxDualSubSlice, 16u);
}

HWTEST_F(ProductHelperCommonTest, givenHwHelperWhenIsFusedEuDisabledForDpasCalledThenFalseReturned) {
    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(gfxCoreHelper.isFusedEuDisabledForDpas(true, nullptr, nullptr, hwInfo));
}
HWTEST_F(ProductHelperCommonTest, givenProductHelperWhenCallingIsCalculationForDisablingEuFusionWithDpasNeededThenFalseReturned) {
    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(gfxCoreHelper.isCalculationForDisablingEuFusionWithDpasNeeded(hwInfo));
}
HWTEST2_F(GfxCoreHelperTest, GivenCooperativeEngineSupportedAndNotUsedWhenAdjustMaxWorkGroupCountIsCalledThenSmallerValueIsReturned, IsAtLeastXe2HpgCore) {

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hwInfo.capabilityTable.defaultEngineType = aub_stream::EngineType::ENGINE_CCS;
    hwInfo.featureTable.flags.ftrRcsNode = false;

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto &revision : revisions) {
        auto hwRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        if (hwRevId == CommonConstants::invalidStepping) {
            continue;
        }
        hwInfo.platform.usRevId = hwRevId;

        for (auto isRcsEnabled : ::testing::Bool()) {
            hwInfo.featureTable.flags.ftrRcsNode = isRcsEnabled;
            for (auto engineGroupType : {EngineGroupType::renderCompute, EngineGroupType::compute, EngineGroupType::cooperativeCompute}) {
                if (productHelper.isCooperativeEngineSupported(hwInfo)) {
                    bool disallowDispatch = (engineGroupType == EngineGroupType::renderCompute) ||
                                            ((engineGroupType == EngineGroupType::compute) && isRcsEnabled);
                    bool applyLimitation = engineGroupType != EngineGroupType::cooperativeCompute;
                    if (disallowDispatch) {
                        EXPECT_EQ(1u, gfxCoreHelper.adjustMaxWorkGroupCount(4u, engineGroupType, rootDeviceEnvironment));
                        EXPECT_EQ(1u, gfxCoreHelper.adjustMaxWorkGroupCount(1024u, engineGroupType, rootDeviceEnvironment));
                    } else if (applyLimitation) {
                        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
                        EXPECT_EQ(1u, gfxCoreHelper.adjustMaxWorkGroupCount(4u, engineGroupType, rootDeviceEnvironment));
                        EXPECT_EQ(256u, gfxCoreHelper.adjustMaxWorkGroupCount(1024u, engineGroupType, rootDeviceEnvironment));
                        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
                        EXPECT_EQ(2u, gfxCoreHelper.adjustMaxWorkGroupCount(4u, engineGroupType, rootDeviceEnvironment));
                        EXPECT_EQ(512u, gfxCoreHelper.adjustMaxWorkGroupCount(1024u, engineGroupType, rootDeviceEnvironment));
                    } else {
                        EXPECT_EQ(4u, gfxCoreHelper.adjustMaxWorkGroupCount(4u, engineGroupType, rootDeviceEnvironment));
                        EXPECT_EQ(1024u, gfxCoreHelper.adjustMaxWorkGroupCount(1024u, engineGroupType, rootDeviceEnvironment));
                    }
                } else {
                    EXPECT_EQ(4u, gfxCoreHelper.adjustMaxWorkGroupCount(4u, engineGroupType, rootDeviceEnvironment));
                    EXPECT_EQ(1024u, gfxCoreHelper.adjustMaxWorkGroupCount(1024u, engineGroupType, rootDeviceEnvironment));
                }
            }
        }
    }
}

HWTEST2_F(GfxCoreHelperTest, GivenGfxHelperWhenAdjustMaxWorkGroupSizeCalledThenOneReturned, DoesNotHaveDispatchAllSupport) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto engineGroupType = EngineGroupType::compute;
    EXPECT_EQ(1u, gfxCoreHelper.adjustMaxWorkGroupCount(1024u, engineGroupType, rootDeviceEnvironment));
}

HWTEST2_F(GfxCoreHelperTest, GivenGfxHelperWhenForceTheoreticalMaxWorkGroupCountThenAdjustMaxWorkGroupSizeReturnNotOne, DoesNotHaveDispatchAllSupport) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceTheoreticalMaxWorkGroupCount.set(true);
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto engineGroupType = EngineGroupType::compute;
    EXPECT_NE(1u, gfxCoreHelper.adjustMaxWorkGroupCount(1024u, engineGroupType, rootDeviceEnvironment));
}

HWTEST_F(GfxCoreHelperTest, givenNumGrfAndSimdSizeWhenAdjustingMaxWorkGroupSizeThenAlwaysReturnDeviceDefault) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    constexpr auto defaultMaxGroupSize = 1024u;

    uint32_t simdSize = 16u;
    uint32_t numGrfRequired = GrfConfig::largeGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.adjustMaxWorkGroupSize(numGrfRequired, simdSize, defaultMaxGroupSize, rootDeviceEnvironment));

    simdSize = 32u;
    numGrfRequired = GrfConfig::largeGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.adjustMaxWorkGroupSize(numGrfRequired, simdSize, defaultMaxGroupSize, rootDeviceEnvironment));

    simdSize = 16u;
    numGrfRequired = GrfConfig::defaultGrfNumber;
    EXPECT_EQ(defaultMaxGroupSize, gfxCoreHelper.adjustMaxWorkGroupSize(numGrfRequired, simdSize, defaultMaxGroupSize, rootDeviceEnvironment));
}

HWTEST2_F(GfxCoreHelperTest, givenParamsWhenCalculateNumThreadsPerThreadGroupThenMethodReturnProperValue, IsAtMostXeCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    std::array<std::array<uint32_t, 3>, 8> values = {{
        {32u, 32u, 1u}, // SIMT Size, totalWorkItems, Max Num of threads
        {32u, 64u, 2u},
        {32u, 128u, 4u},
        {32u, 1024u, 32u},
        {16u, 32u, 2u},
        {16u, 64u, 4u},
        {16u, 128u, 8u},
        {16u, 1024u, 64u},
    }};

    for (auto &[simtSize, totalWgSize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simtSize, totalWgSize, 32u, rootDeviceEnvironment));
    }
}

HWTEST_F(GfxCoreHelperTest, givenFlagRemoveRestrictionsOnNumberOfThreadsInGpgpuThreadGroupWhenCalculateNumThreadsPerThreadGroupThenMethodReturnProperValue) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.RemoveRestrictionsOnNumberOfThreadsInGpgpuThreadGroup.set(1);
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    std::array<std::array<uint32_t, 4>, 8> values = {{
        {32u, 32u, 128u, 1u}, // SIMT Size, totalWorkItems,Grf size, Max Num of threads
        {32u, 64u, 32u, 2u},
        {32u, 128u, 256u, 4u},
        {32u, 1024u, 128u, 32u},
        {16u, 32u, 32u, 2u},
        {16u, 64u, 256u, 4u},
        {16u, 128u, 128u, 8u},
        {16u, 1024u, 256u, 64u},
    }};

    for (auto &[simtSize, totalWgSize, grfsize, expectedNumThreadsPerThreadGroup] : values) {
        EXPECT_EQ(expectedNumThreadsPerThreadGroup, gfxCoreHelper.calculateNumThreadsPerThreadGroup(simtSize, totalWgSize, grfsize, rootDeviceEnvironment));
    }
}

HWTEST2_F(GfxCoreHelperTest, whenGetDefaultDeviceHierarchyThenReturnCompositeHierarchy, IsNotXeHpcCore) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto defaultDeviceHierarchy = gfxCoreHelper.getDefaultDeviceHierarchy();
    EXPECT_EQ(DeviceHierarchyMode::composite, defaultDeviceHierarchy);
}

HWTEST2_F(GfxCoreHelperTest, whenGetSipBinaryFromExternalLibThenReturnFalse, IsAtMostXe3Core) {
    const auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.getSipBinaryFromExternalLib());
}

HWTEST_F(GfxCoreHelperTest, givenContextGroupDisabledWhenContextGroupContextsCountAndSecondaryContextsSupportQueriedThenZeroCountAndFalseIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(0u, gfxCoreHelper.getContextGroupContextsCount());
    EXPECT_FALSE(gfxCoreHelper.areSecondaryContextsSupported());
}

TEST_F(GfxCoreHelperTest, givenContextGroupEnabledWithDebugKeyWhenContextGroupContextsCountAndSecondaryContextsSupportQueriedThenCorrectValuesAreReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ContextGroupSize.set(8);
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(8u, gfxCoreHelper.getContextGroupContextsCount());
    EXPECT_TRUE(gfxCoreHelper.areSecondaryContextsSupported());
    EXPECT_EQ(4u, gfxCoreHelper.getContextGroupHpContextsCount(EngineGroupType::copy, false));
    EXPECT_EQ(0u, gfxCoreHelper.getContextGroupHpContextsCount(EngineGroupType::copy, true));

    debugManager.flags.ContextGroupSize.set(1);
    EXPECT_EQ(1u, gfxCoreHelper.getContextGroupContextsCount());
    EXPECT_FALSE(gfxCoreHelper.areSecondaryContextsSupported());

    debugManager.flags.ContextGroupSize.set(2);
    EXPECT_EQ(2u, gfxCoreHelper.getContextGroupContextsCount());
    EXPECT_TRUE(gfxCoreHelper.areSecondaryContextsSupported());
    EXPECT_EQ(1u, gfxCoreHelper.getContextGroupHpContextsCount(EngineGroupType::copy, false));
    EXPECT_EQ(0u, gfxCoreHelper.getContextGroupHpContextsCount(EngineGroupType::copy, true));
}

HWTEST_F(GfxCoreHelperTest, whenAskingIf48bResourceNeededForCmdBufferThenReturnTrue) {
    EXPECT_TRUE(getHelper<GfxCoreHelper>().is48ResourceNeededForCmdBuffer());
}

HWTEST_F(GfxCoreHelperTest, givenDebugVariableSetWhenAskingForDuplicatedInOrderHostStorageThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = getHelper<GfxCoreHelper>();
    auto &rootExecEnv = *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];

    EXPECT_FALSE(helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);
    EXPECT_TRUE(helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));

    debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(0);
    EXPECT_FALSE(helper.duplicatedInOrderCounterStorageEnabled(rootExecEnv));
}

HWTEST_F(GfxCoreHelperTest, givenDebugVariableSetWhenAskingForInOrderAtomicSignalingThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = getHelper<GfxCoreHelper>();
    auto &rootExecEnv = *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0];

    EXPECT_FALSE(helper.inOrderAtomicSignallingEnabled(rootExecEnv));

    debugManager.flags.InOrderAtomicSignallingEnabled.set(1);
    EXPECT_TRUE(helper.inOrderAtomicSignallingEnabled(rootExecEnv));

    debugManager.flags.InOrderAtomicSignallingEnabled.set(0);
    EXPECT_FALSE(helper.inOrderAtomicSignallingEnabled(rootExecEnv));
}

TEST_F(GfxCoreHelperTest, whenOnlyPerThreadPrivateMemorySizeIsDefinedThenItIsReturnedAsKernelPrivateMemorySize) {
    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize = 0x100u;
    EXPECT_EQ(0x100u, getHelper<GfxCoreHelper>().getKernelPrivateMemSize(kernelDescriptor));
}

HWTEST_F(GfxCoreHelperTest, givenCooperativeKernelWhenAskingForSingleTileDispatchThenReturnTrue) {
    auto &helper = getHelper<GfxCoreHelper>();

    EXPECT_TRUE(helper.singleTileExecImplicitScalingRequired(true));
    EXPECT_FALSE(helper.singleTileExecImplicitScalingRequired(false));
}

HWTEST2_F(GfxCoreHelperTest, whenPrivateScratchSizeIsDefinedThenItIsReturnedAsKernelPrivateMemorySize, IsAtLeastXeCore) {
    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.kernelAttributes.perHwThreadPrivateMemorySize = 0x100u;
    kernelDescriptor.kernelAttributes.privateScratchMemorySize = 0x200u;
    EXPECT_EQ(0x200u, getHelper<GfxCoreHelper>().getKernelPrivateMemSize(kernelDescriptor));
}

HWTEST_F(GfxCoreHelperTest, givenEncodeDispatchKernelWhenUsingGfxHelperThenSameDataPrpvided) {
    auto &helper = getHelper<GfxCoreHelper>();

    uint32_t workDim = 1;
    uint32_t simd = 8;
    size_t lws[3] = {16, 1, 1};
    std::array<uint8_t, 3> walkOrder = {};
    uint32_t requiredWalkOrder = 0u;

    bool encoderReturn = EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd);

    uint32_t helperRequiredWalkOrder = 0u;

    bool helperReturn = helper.isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, helperRequiredWalkOrder, simd);

    EXPECT_EQ(encoderReturn, helperReturn);
    EXPECT_EQ(requiredWalkOrder, helperRequiredWalkOrder);
}

HWTEST_F(GfxCoreHelperTest, whenCallGetMaxPtssIndexThenCorrectValue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(15u, gfxCoreHelper.getMaxPtssIndex(productHelper));
}

HWTEST_F(GfxCoreHelperTest, whenEncodeAdditionalTimestampOffsetsThenNothingEncoded) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    constexpr static auto bufferSize = sizeof(MI_STORE_REGISTER_MEM);
    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    uint64_t fstAddress = 0;
    uint64_t sndAddress = 0;
    MemorySynchronizationCommands<FamilyType>::encodeAdditionalTimestampOffsets(stream, fstAddress, sndAddress, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList storeRegMemList = hwParser.getCommandsList<MI_STORE_REGISTER_MEM>();
    EXPECT_EQ(0u, storeRegMemList.size());
}

HWTEST2_F(GfxCoreHelperTest, GivenVariousValuesWhenCallingCalculateAvailableThreadCountAndThreadCountAvailableIsBiggerThenCorrectValueIsReturned, IsAtMostXe2HpgCore) {
    std::array<std::pair<uint32_t, uint32_t>, 2> grfTestInputs = {{{128, 8},
                                                                   {256, 4}}};
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto expected = expectedThreadCountPerEu * hardwareInfo.gtSystemInfo.EUCount;
        // force always bigger Thread Count available
        hardwareInfo.gtSystemInfo.ThreadCount = 2 * expected;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hardwareInfo, grfCount, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(expected, result);
    }
}

HWTEST2_F(GfxCoreHelperTest, GivenVariousValuesWhenCallingCalculateAvailableThreadCountAndThreadCountAvailableIsSmallerThenThreadCountIsReturned, IsAtMostXe2HpgCore) {
    std::array<std::pair<uint32_t, uint32_t>, 2> grfTestInputs = {{
        {128, 8},
        {256, 4},
    }};
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto calculatedThreadCount = expectedThreadCountPerEu * hardwareInfo.gtSystemInfo.EUCount;
        // force thread count smaller than calculation
        hardwareInfo.gtSystemInfo.ThreadCount = calculatedThreadCount / 2;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hardwareInfo, grfCount, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(hardwareInfo.gtSystemInfo.ThreadCount, result);
    }
}

HWTEST2_F(GfxCoreHelperTest, GivenModifiedGtSystemInfoWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsAtMostXe2HpgCore) {
    std::array<std::pair<uint32_t, uint32_t>, 2> testInputs = {{{64, 256},
                                                                {128, 512}}};
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto hwInfo = hardwareInfo;
    for (const auto &[euCount, expectedThreadCount] : testInputs) {
        // force thread count bigger than expected
        hwInfo.gtSystemInfo.ThreadCount = 1024;
        hwInfo.gtSystemInfo.EUCount = euCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, 256, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(expectedThreadCount, result);
    }
}

HWTEST_F(GfxCoreHelperTest, givenHwHelperWhenAligningThreadGroupCountToDssSizeThenThreadGroupCountChanged) {
    auto &helper = getHelper<GfxCoreHelper>();
    uint32_t threadGroupCountBefore = 4096;
    uint32_t threadCount = threadGroupCountBefore;
    helper.alignThreadGroupCountToDssSize(threadCount, 1, 1, 1);
    EXPECT_NE(threadGroupCountBefore, threadCount);
}

HWTEST_F(GfxCoreHelperTest, givenHwHelperWhenThreadGroupCountIsAlignedToDssThenThreadCountNotChanged) {
    auto &helper = getHelper<GfxCoreHelper>();
    uint32_t dssCount = 16;
    uint32_t threadGroupSize = 32;
    uint32_t threadsPerDss = 2 * threadGroupSize;
    uint32_t maxThreadCount = (dssCount * threadsPerDss) / threadGroupSize;
    uint32_t threadCount = maxThreadCount;
    helper.alignThreadGroupCountToDssSize(threadCount, dssCount, threadsPerDss, threadGroupSize);
    EXPECT_EQ(2 * dssCount, threadCount);
}

HWTEST_F(GfxCoreHelperTest, givenHwHelperWhenThreadGroupCountIsAlignedToDssThenThreadCountChanged) {
    auto &helper = getHelper<GfxCoreHelper>();
    uint32_t dssCount = 16;
    uint32_t threadGroupSize = 32;
    uint32_t threadsPerDss = 2 * threadGroupSize - 1;
    uint32_t maxThreadCount = (dssCount * threadsPerDss) / threadGroupSize;
    uint32_t threadCount = maxThreadCount;
    helper.alignThreadGroupCountToDssSize(threadCount, dssCount, threadsPerDss, threadGroupSize);
    EXPECT_EQ(dssCount, threadCount);
}

HWTEST_F(GfxCoreHelperTest, givenDebugFlagForceUseOnlyGlobalTimestampsSetWhenCallUseOnlyGlobalTimestampsThenTrueIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceUseOnlyGlobalTimestamps.set(1);

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.useOnlyGlobalTimestamps());
}

HWTEST2_F(GfxCoreHelperTest, whenIsCacheFlushPriorImageReadRequiredCalledThenFalseIsReturned, IsAtMostXeCore) {
    auto &helper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(helper.isCacheFlushPriorImageReadRequired());
}

HWTEST2_F(GfxCoreHelperTest, whenIsExtendedUsmPoolSizeEnabledRequiredCalledThenFalseIsReturned, IsAtMostXeCore) {
    auto &helper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(helper.isExtendedUsmPoolSizeEnabled());
}

HWTEST2_F(GfxCoreHelperTest, givenAtLeastXe2HpgWhenSetStallOnlyBarrierThenPipeControlProgrammed, IsAtMostXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    constexpr static auto bufferSize = sizeof(PIPE_CONTROL);

    char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    PipeControlArgs args;
    args.csStallOnly = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, PostSyncMode::noWrite, 0u, 0u, args);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    EXPECT_EQ(1u, pipeControlList.size());
    GenCmdList::iterator itor = pipeControlList.begin();
    EXPECT_TRUE(hwParser.isStallingBarrier<FamilyType>(itor));
}

HWTEST_F(GfxCoreHelperTest, givenCommandCacheInvalidateFlagSetWhenProgrammingBarrierThenExpectFieldSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    constexpr static size_t bufferSize = 64;
    alignas(4) char streamBuffer[bufferSize];
    LinearStream stream(streamBuffer, bufferSize);
    PipeControlArgs args;
    args.commandCacheInvalidateEnable = true;

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(stream, args);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, 0);
    GenCmdList pipeControlList = hwParser.getCommandsList<PIPE_CONTROL>();
    EXPECT_EQ(1u, pipeControlList.size());
    GenCmdList::iterator itor = pipeControlList.begin();
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(pipeControl->getCommandCacheInvalidateEnable());
}

HWTEST2_F(GfxCoreHelperTest, whenGetQueuePriorityLevelsQueriedThen2IsReturned, IsAtMostXe3Core) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(2u, gfxCoreHelper.getQueuePriorityLevels());
}

HWTEST_F(GfxCoreHelperTest, whenGettingWalkerInlineDataSizeThenCorrectValueReturned) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(DefaultWalkerType::getInlineDataSize(), gfxCoreHelper.getDefaultWalkerInlineDataSize());
}

HWTEST_F(GfxCoreHelperTest, whenGettingSurfaceBaseAddressAlignmentMaskThenCorrectValueReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignmentMask(), gfxCoreHelper.getSurfaceBaseAddressAlignmentMask());
}

HWTEST_F(GfxCoreHelperTest, whenGettingSurfaceBaseAddressAlignmentThenCorrectValueReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(EncodeSurfaceState<FamilyType>::getSurfaceBaseAddressAlignment(), gfxCoreHelper.getSurfaceBaseAddressAlignment());
}
