/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/hw_helper_tests.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "test_traits_common.h"

#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>

using namespace NEO;

TEST(HwHelperSimpleTest, givenDebugVariableWhenAskingForCompressionThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    HardwareInfo localHwInfo = *defaultHwInfo;

    // debug variable not set
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(HwHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_FALSE(HwHelper::compressedImagesSupported(localHwInfo));

    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(HwHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_TRUE(HwHelper::compressedImagesSupported(localHwInfo));

    // debug variable set
    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
    DebugManager.flags.RenderCompressedImagesEnabled.set(1);
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(HwHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_TRUE(HwHelper::compressedImagesSupported(localHwInfo));

    DebugManager.flags.RenderCompressedBuffersEnabled.set(0);
    DebugManager.flags.RenderCompressedImagesEnabled.set(0);
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    localHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_FALSE(HwHelper::compressedBuffersSupported(localHwInfo));
    EXPECT_FALSE(HwHelper::compressedImagesSupported(localHwInfo));
}

TEST_F(HwHelperTest, WhenGettingHelperThenValidHelperReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);
}

HWTEST_F(HwHelperTest, givenHwHelperWhenAskingForTimestampPacketAlignmentThenReturnFourCachelines) {
    auto &helper = HwHelper::get(renderCoreFamily);

    constexpr auto expectedAlignment = MemoryConstants::cacheLineSize * 4;

    EXPECT_EQ(expectedAlignment, helper.getTimestampPacketAllocatorAlignment());
}

HWTEST_F(HwHelperTest, givenHwHelperWhenGettingISAPaddingThenCorrectValueIsReturned) {
    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_EQ(hwHelper.getPaddingForISAAllocation(), 512u);
}

HWTEST_F(HwHelperTest, WhenSettingRenderSurfaceStateForBufferThenL1CachePolicyIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    class mockHwHelperHw : public HwHelperHw<FamilyType> {
      public:
        bool called = false;
        using HwHelperHw<FamilyType>::HwHelperHw;
        mockHwHelperHw() {}
        void setL1CachePolicy(bool useL1Cache, typename FamilyType::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) override {
            HwHelperHw<FamilyType>::setL1CachePolicy(useL1Cache, surfaceState, hwInfo);
            called = true;
        }
    };

    mockHwHelperHw helper;
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

    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, false,
                                          false);
    ASSERT_EQ(helper.called, true);
    helper.called = false;

    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, false,
                                          true);
    ASSERT_EQ(helper.called, true);
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, WhenGettingBindingTableStateSurfaceStatePointerThenCorrectPointerIsReturned) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    BINDING_TABLE_STATE bindingTableState[4];

    bindingTableState[2].getRawData(0) = 0x00123456;

    auto &helper = HwHelper::get(renderCoreFamily);

    auto pointer = helper.getBindingTableStateSurfaceStatePointer(bindingTableState, 2);
    EXPECT_EQ(0x00123456u, pointer);
}

HWTEST_F(HwHelperTest, WhenGettingBindingTableStateSizeThenCorrectSizeIsReturned) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    auto &helper = HwHelper::get(renderCoreFamily);

    auto pointer = helper.getBindingTableStateSize();
    EXPECT_EQ(sizeof(BINDING_TABLE_STATE), pointer);
}

TEST_F(HwHelperTest, WhenGettingBindingTableStateAlignementThenCorrectSizeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(0u, helper.getBindingTableStateAlignement());
}

HWTEST_F(HwHelperTest, WhenGettingInterfaceDescriptorDataSizeThenCorrectSizeIsReturned) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    auto &helper = HwHelper::get(renderCoreFamily);

    EXPECT_EQ(sizeof(INTERFACE_DESCRIPTOR_DATA), helper.getInterfaceDescriptorDataSize());
}

TEST_F(HwHelperTest, givenDebuggingInactiveWhenSipKernelTypeIsQueriedThenCsrTypeIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto sipType = helper.getSipKernelType(false);
    EXPECT_EQ(SipKernelType::Csr, sipType);
}

TEST_F(HwHelperTest, givenEngineTypeRcsWhenCsTraitsAreQueiredThenCorrectNameInTraitsIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto &csTraits = helper.getCsTraits(aub_stream::ENGINE_RCS);
    EXPECT_STREQ("RCS", csTraits.name);
}

using isTglLpOrBelow = IsAtMostProduct<IGFX_TIGERLAKE_LP>;
HWTEST2_F(HwHelperTest, givenHwHelperWhenGettingThreadsPerEUConfigsThenNoConfigsAreReturned, isTglLpOrBelow) {
    auto &helper = HwHelper::get(renderCoreFamily);

    auto &configs = helper.getThreadsPerEUConfigs();
    EXPECT_EQ(0U, configs.size());
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenHwHelperWhenGetGpuTimeStampInNSIsCalledThenCorrectValueIsReturned) {

    auto &helper = HwHelper::get(renderCoreFamily);
    auto timeStamp = 0x00ff'ffff'ffff;
    auto frequency = 123456.0;
    auto result = static_cast<uint64_t>(timeStamp * frequency);
    EXPECT_EQ(result, helper.getGpuTimeStampInNS(timeStamp, frequency));
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
    MemorySynchronizationCommands<FamilyType>::addPipeControlAndProgramPostSyncOperation(
        stream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, address, immediateData, hardwareInfo, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForPipeControlWithPostSyncOperation(hardwareInfo) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hardwareInfo);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), pipeControlLocationSize));
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_TRUE(memcmp(pipeControl, &expectedPipeControl, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST_F(PipeControlHelperTests, givenHwHelperwhenAskingForDcFlushThenReturnTrue) {
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo));
}

HWTEST_F(PipeControlHelperTests, givenDcFlushNotAllowedWhenProgrammingPipeControlThenDontSetDcFlush) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);

    PipeControlArgs args;
    args.dcFlushEnable = true;

    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

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
    MemorySynchronizationCommands<FamilyType>::addPipeControlAndProgramPostSyncOperation(
        stream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, address, immediateData, hardwareInfo, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForPipeControlWithPostSyncOperation(hardwareInfo) - sizeof(PIPE_CONTROL);
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
    MemorySynchronizationCommands<FamilyType>::addPipeControlAndProgramPostSyncOperation(
        stream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, address, immediateData, hardwareInfo, args);
    auto additionalPcSize = MemorySynchronizationCommands<FamilyType>::getSizeForPipeControlWithPostSyncOperation(hardwareInfo) - sizeof(PIPE_CONTROL);
    auto pipeControlLocationSize = additionalPcSize - MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hardwareInfo);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), pipeControlLocationSize));
    ASSERT_NE(nullptr, pipeControl);

    EXPECT_EQ(sizeof(PIPE_CONTROL) + additionalPcSize, stream.getUsed());
    EXPECT_TRUE(memcmp(pipeControl, &expectedPipeControl, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST_F(PipeControlHelperTests, WhenIsDcFlushAllowedIsCalledThenCorrectResultIsReturned) {
    auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(false, *defaultHwInfo));
    EXPECT_EQ(hwInfoConfig.isDcFlushAllowed(), MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *defaultHwInfo));
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

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenNoAllocationProvidedThenUseArgumentsasInput) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto gmmHelper = pDevice->getGmmHelper();

    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(sizeof(RENDER_SURFACE_STATE), helper.getRenderSurfaceStateSize());

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;
    length.Length = static_cast<uint32_t>(size - 1);
    uint64_t addr = 0x2000;
    size_t offset = 0x1000;
    uint32_t pitch = 0x40;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, offset, pitch, nullptr, false, type, true, false);

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
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1000;
    addr = 0x2001;
    length.Length = static_cast<uint32_t>(size - 1);
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    size = 0x1005;
    length.Length = static_cast<uint32_t>(alignUp(size, 4) - 1);
    isReadOnly = true;
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, nullptr, isReadOnly, type, true, false);
    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), state->getMemoryObjectControlState());
    EXPECT_EQ(length.SurfaceState.Depth + 1u, state->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, state->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, state->getHeight());
    EXPECT_EQ(addr, state->getSurfaceBaseAddress());

    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenAllocationProvidedThenUseAllocationAsInput) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    SURFACE_STATE_BUFFER_LENGTH length;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    length.Length = static_cast<uint32_t>(allocSize - 1);
    GraphicsAllocation allocation(0, AllocationType::UNKNOWN, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 0u);
    allocation.setDefaultGmm(new Gmm(pDevice->getGmmClientContext(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, true, false);
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

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxDisabledThenSetCoherencyToGpuAndAuxModeToCompression) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 0u);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmClientContext(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    allocation.getDefaultGmm()->isCompressionEnabled = true;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, state->getCoherencyType());
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(state, allocation.getDefaultGmm()));

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenGmmCompressionDisabledAndAllocationEnabledAnNonAuxDisabledThenSetCoherencyToIaAndAuxModeToNone) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 1);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmClientContext(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenOverrideMocsIndexForScratchSpaceWhenSurfaceStateIsProgrammedForScratchSpaceThenOverrideMocsIndexWithCorrectValue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.OverrideMocsIndexForScratchSpace.set(1);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 1);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmClientContext(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, false, false);

    auto mocsProgrammed = state->getMemoryObjectControlState() >> 1;
    EXPECT_EQ(1u, mocsProgrammed);

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, givenCreatedSurfaceStateBufferWhenGmmAndAllocationCompressionEnabledAnNonAuxEnabledThenSetCoherencyToIaAndAuxModeToNone) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    void *stateBuffer = alignedMalloc(sizeof(RENDER_SURFACE_STATE), sizeof(RENDER_SURFACE_STATE));
    ASSERT_NE(nullptr, stateBuffer);
    RENDER_SURFACE_STATE *state = reinterpret_cast<RENDER_SURFACE_STATE *>(stateBuffer);

    memset(stateBuffer, 0, sizeof(RENDER_SURFACE_STATE));
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;

    void *cpuAddr = reinterpret_cast<void *>(0x4000);
    uint64_t gpuAddr = 0x4000u;
    size_t allocSize = size;
    GraphicsAllocation allocation(0, AllocationType::BUFFER, cpuAddr, gpuAddr, 0u, allocSize, MemoryPool::MemoryNull, 1u);
    allocation.setDefaultGmm(new Gmm(rootDeviceEnvironment.getGmmClientContext(), allocation.getUnderlyingBuffer(), allocation.getUnderlyingBufferSize(), 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    allocation.getDefaultGmm()->isCompressionEnabled = true;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;
    helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, stateBuffer, size, addr, 0, pitch, &allocation, false, type, true, false);
    EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), state->getCoherencyType());
    EXPECT_EQ(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE, state->getAuxiliarySurfaceMode());

    delete allocation.getDefaultGmm();
    alignedFree(stateBuffer);
}

HWTEST_F(HwHelperTest, DISABLED_profilingCreationOfRenderSurfaceStateVsMemcpyOfCachelineAlignedBuffer) {
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
    auto &helper = HwHelper::get(renderCoreFamily);

    size_t size = 0x1000;
    uint64_t addr = 0x2000;
    uint32_t pitch = 0;
    SURFACE_TYPE type = RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER;

    for (uint32_t i = 0; i < maxLoop; ++i) {
        auto t1 = std::chrono::high_resolution_clock::now();
        helper.setRenderSurfaceStateForBuffer(rootDeviceEnvironment, surfaceStates[i], size, addr, 0, pitch, nullptr, false, type, true, false);
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

HWTEST_F(HwHelperTest, WhenTestingIfL3ConfigProgrammableThenCorrectValueIsReturned) {
    bool PreambleHelperL3Config;
    bool isL3Programmable;
    const HardwareInfo &hwInfo = *defaultHwInfo;

    PreambleHelperL3Config =
        PreambleHelper<FamilyType>::isL3Configurable(*defaultHwInfo);
    isL3Programmable =
        HwHelperHw<FamilyType>::get().isL3Configurable(hwInfo);

    EXPECT_EQ(PreambleHelperL3Config, isL3Programmable);
}

TEST(HwHelperCacheFlushTest, givenEnableCacheFlushFlagIsEnableWhenPlatformDoesNotSupportThenOverrideAndReturnSupportTrue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_TRUE(HwHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(HwHelperCacheFlushTest, givenEnableCacheFlushFlagIsDisableWhenPlatformSupportsThenOverrideAndReturnSupportFalse) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_FALSE(HwHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(HwHelperCacheFlushTest, givenEnableCacheFlushFlagIsReadPlatformSettingWhenPlatformDoesNotSupportThenReturnSupportFalse) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(-1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_FALSE(HwHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

TEST(HwHelperCacheFlushTest, givenEnableCacheFlushFlagIsReadPlatformSettingWhenPlatformSupportsThenReturnSupportTrue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(-1);

    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.supportCacheFlushAfterWalker = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_TRUE(HwHelper::cacheFlushAfterWalkerSupported(device->getHardwareInfo()));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenHwHelperWhenGettingGlobalTimeStampBitsThenCorrectValueIsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(helper.getGlobalTimeStampBits(), 36U);
}

TEST_F(HwHelperTest, givenEnableLocalMemoryDebugVarAndOsEnableLocalMemoryWhenSetThenGetEnableLocalMemoryReturnsCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    VariableBackup<bool> orgOsEnableLocalMemory(&OSInterface::osEnableLocalMemory);
    auto &helper = HwHelper::get(renderCoreFamily);

    DebugManager.flags.EnableLocalMemory.set(0);
    EXPECT_FALSE(helper.getEnableLocalMemory(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(1);
    EXPECT_TRUE(helper.getEnableLocalMemory(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(-1);

    OSInterface::osEnableLocalMemory = false;
    EXPECT_FALSE(helper.getEnableLocalMemory(hardwareInfo));

    OSInterface::osEnableLocalMemory = true;
    EXPECT_EQ(helper.isLocalMemoryEnabled(hardwareInfo), helper.getEnableLocalMemory(hardwareInfo));
}

TEST_F(HwHelperTest, givenAUBDumpForceAllToLocalMemoryDebugVarWhenSetThenGetEnableLocalMemoryReturnsCorrectValue) {
    DebugManagerStateRestore dbgRestore;
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hardwareInfo));
    auto &helper = HwHelper::get(renderCoreFamily);

    DebugManager.flags.AUBDumpForceAllToLocalMemory.set(true);
    EXPECT_TRUE(helper.getEnableLocalMemory(hardwareInfo));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenVariousCachesRequestThenCorrectMocsIndexesAreReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getGmmHelper();
    auto expectedMocsForL3off = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    auto expectedMocsForL3on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
    auto expectedMocsForL3andL1on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;

    auto mocsIndex = helper.getMocsIndex(*gmmHelper, false, true);
    EXPECT_EQ(expectedMocsForL3off, mocsIndex);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, false);
    EXPECT_EQ(expectedMocsForL3on, mocsIndex);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, true);
    if (mocsIndex != expectedMocsForL3andL1on) {
        EXPECT_EQ(expectedMocsForL3on, mocsIndex);
    } else {
        EXPECT_EQ(expectedMocsForL3andL1on, mocsIndex);
    }
}

HWTEST_F(HwHelperTest, whenQueryingMaxNumSamplersThenReturnSixteen) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(16u, helper.getMaxNumSamplers());
}

HWTEST_F(HwHelperTest, givenKernelInfoWhenCheckingRequiresAuxResolvesThenCorrectValuesAreReturned) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);
    HardwareInfo hwInfo = *defaultHwInfo;
    KernelInfo kernelInfo{};

    ArgDescriptor argDescriptorValue(ArgDescriptor::ArgType::ArgTValue);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorValue);
    EXPECT_FALSE(clHwHelper.requiresAuxResolves(kernelInfo, hwInfo));

    ArgDescriptor argDescriptorPointer(ArgDescriptor::ArgType::ArgTPointer);
    argDescriptorPointer.as<ArgDescPointer>().accessedUsingStatelessAddressingMode = true;
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorPointer);
    EXPECT_TRUE(clHwHelper.requiresAuxResolves(kernelInfo, hwInfo));
}

HWTEST_F(HwHelperTest, givenDebugVariableSetWhenAskingForAuxTranslationModeThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    EXPECT_EQ(UnitTestHelper<FamilyType>::requiredAuxTranslationMode, HwHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    if (HwHelperHw<FamilyType>::getAuxTranslationMode(hwInfo) == AuxTranslationMode::Blit) {
        hwInfo.capabilityTable.blitterOperationsSupported = false;

        EXPECT_EQ(AuxTranslationMode::Builtin, HwHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));
    }

    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::None));
    EXPECT_EQ(AuxTranslationMode::None, HwHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    hwInfo.capabilityTable.blitterOperationsSupported = false;
    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Blit));
    EXPECT_EQ(AuxTranslationMode::Builtin, HwHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    hwInfo.capabilityTable.blitterOperationsSupported = true;
    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Blit));
    EXPECT_EQ(AuxTranslationMode::Blit, HwHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));

    DebugManager.flags.ForceAuxTranslationMode.set(static_cast<int32_t>(AuxTranslationMode::Builtin));
    EXPECT_EQ(AuxTranslationMode::Builtin, HwHelperHw<FamilyType>::getAuxTranslationMode(hwInfo));
}

HWTEST_F(HwHelperTest, givenDebugFlagWhenCheckingIfBufferIsSuitableForCompressionThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = HwHelper::get(renderCoreFamily);

    DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(0);
    EXPECT_FALSE(helper.isBufferSizeSuitableForCompression(0, *defaultHwInfo));
    EXPECT_FALSE(helper.isBufferSizeSuitableForCompression(KB, *defaultHwInfo));
    EXPECT_FALSE(helper.isBufferSizeSuitableForCompression(KB + 1, *defaultHwInfo));

    DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(1);
    EXPECT_TRUE(helper.isBufferSizeSuitableForCompression(0, *defaultHwInfo));
    EXPECT_TRUE(helper.isBufferSizeSuitableForCompression(KB, *defaultHwInfo));
    EXPECT_TRUE(helper.isBufferSizeSuitableForCompression(KB + 1, *defaultHwInfo));
}

HWTEST_F(HwHelperTest, givenHwHelperWhenAskingForTilingSupportThenReturnValidValue) {
    bool tilingSupported = UnitTestHelper<FamilyType>::tiledImagesSupported;

    const uint32_t numImageTypes = 6;
    const cl_mem_object_type imgTypes[numImageTypes] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE1D_BUFFER,
                                                        CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D};
    cl_image_desc imgDesc = {};
    MockContext context;
    cl_int retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, 0, 1, nullptr, retVal));

    auto &helper = HwHelper::get(renderCoreFamily);

    for (uint32_t i = 0; i < numImageTypes; i++) {
        imgDesc.image_type = imgTypes[i];
        imgDesc.buffer = nullptr;

        bool allowedType = imgTypes[i] == (CL_MEM_OBJECT_IMAGE2D) || (imgTypes[i] == CL_MEM_OBJECT_IMAGE3D) ||
                           (imgTypes[i] == CL_MEM_OBJECT_IMAGE2D_ARRAY);

        // non shared context, dont force linear storage
        EXPECT_EQ((tilingSupported & allowedType), helper.tilingAllowed(false, Image::isImage1d(imgDesc), false));
        {
            DebugManagerStateRestore restore;
            DebugManager.flags.ForceLinearImages.set(true);
            // non shared context, dont force linear storage + debug flag
            EXPECT_FALSE(helper.tilingAllowed(false, Image::isImage1d(imgDesc), false));
        }
        // shared context, dont force linear storage
        EXPECT_FALSE(helper.tilingAllowed(true, Image::isImage1d(imgDesc), false));
        // non shared context,  force linear storage
        EXPECT_FALSE(helper.tilingAllowed(false, Image::isImage1d(imgDesc), true));

        // non shared context, dont force linear storage + create from buffer
        imgDesc.buffer = buffer.get();
        EXPECT_FALSE(helper.tilingAllowed(false, Image::isImage1d(imgDesc), false));
    }
}

HWTEST_F(HwHelperTest, WhenIsBankOverrideRequiredIsCalledThenFalseIsReturned) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.isBankOverrideRequired(hardwareInfo));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, GivenVariousValuesWhenCallingGetBarriersCountFromHasBarrierThenCorrectValueIsReturned) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_EQ(0u, hwHelper.getBarriersCountFromHasBarriers(0u));
    EXPECT_EQ(1u, hwHelper.getBarriersCountFromHasBarriers(1u));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, GivenVariousValuesWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto result = hwHelper.calculateAvailableThreadCount(
        hardwareInfo.platform.eProductFamily,
        0,
        hardwareInfo.gtSystemInfo.EUCount,
        hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount);
    EXPECT_EQ(hardwareInfo.gtSystemInfo.ThreadCount, result);
}

HWTEST_F(HwHelperTest, givenDefaultHwHelperHwWhenIsOffsetToSkipSetFFIDGPWARequiredCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.isOffsetToSkipSetFFIDGPWARequired(hardwareInfo));
}

HWTEST_F(HwHelperTest, givenDefaultHwHelperHwWhenIsForceDefaultRCSEngineWARequiredCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    EXPECT_FALSE(HwHelperHw<FamilyType>::isForceDefaultRCSEngineWARequired(hardwareInfo));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenDefaultHwHelperHwWhenIsWorkaroundRequiredCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
}

HWTEST_F(HwHelperTest, givenDefaultHwHelperHwWhenMinimalSIMDSizeIsQueriedThen8IsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(8u, helper.getMinimalSIMDSize());
}

HWTEST_F(HwHelperTest, givenLockableAllocationWhenGettingIsBlitCopyRequiredForLocalMemoryThenCorrectValuesAreReturned) {
    DebugManagerStateRestore restore{};
    auto &helper = HwHelper::get(renderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);

    auto expectedDefaultValue = (hwInfoConfig.getLocalMemoryAccessMode(hwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed);
    EXPECT_EQ(expectedDefaultValue, helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    graphicsAllocation.overrideMemoryPool(MemoryPool::System64KBPages);
    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
}

HWTEST_F(HwHelperTest, givenNotLockableAllocationWhenGettingIsBlitCopyRequiredForLocalMemoryThenCorrectValuesAreReturned) {
    DebugManagerStateRestore restore{};
    auto &helper = HwHelper::get(renderCoreFamily);
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::SVM_GPU);
    EXPECT_FALSE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);

    MockGmm mockGmm(pDevice->getGmmClientContext(), nullptr, 100, 100, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    mockGmm.resourceParams.Flags.Info.NotLockable = true;
    graphicsAllocation.setDefaultGmm(&mockGmm);

    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    EXPECT_TRUE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    graphicsAllocation.overrideMemoryPool(MemoryPool::System64KBPages);
    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
}

HWTEST2_F(HwHelperTest, givenDefaultHwHelperHwWhenGettingIsBlitCopyRequiredForLocalMemoryThenFalseIsReturned, IsAtMostGen11) {
    auto &helper = HwHelper::get(renderCoreFamily);
    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);
    graphicsAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    EXPECT_FALSE(helper.isBlitCopyRequiredForLocalMemory(*defaultHwInfo, graphicsAllocation));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, WhenIsFusedEuDispatchEnabledIsCalledThenFalseIsReturned) {
    if (hardwareInfo.platform.eRenderCoreFamily == IGFX_GEN12LP_CORE) {
        GTEST_SKIP();
    }
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isFusedEuDispatchEnabled(hardwareInfo));
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

using HwInfoConfigCommonTest = ::testing::Test;

HWTEST2_F(HwInfoConfigCommonTest, givenBlitterPreferenceWhenEnablingBlitterOperationsSupportThenHonorThePreference, IsAtLeastGen12lp) {
    HardwareInfo hardwareInfo = *defaultHwInfo;

    auto hwInfoConfig = HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    hwInfoConfig->configureHardwareCustom(&hardwareInfo, nullptr);

    const auto expectedBlitterSupport = hwInfoConfig->obtainBlitterPreference(hardwareInfo);
    EXPECT_EQ(expectedBlitterSupport, hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWTEST_F(HwHelperTest, givenHwHelperWhenAskingForIsaSystemMemoryPlacementThenReturnFalseIfLocalMemorySupported) {
    DebugManagerStateRestore restorer;
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    auto localMemoryEnabled = hwHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, hwHelper.useSystemMemoryPlacementForISA(hardwareInfo));

    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    localMemoryEnabled = hwHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, hwHelper.useSystemMemoryPlacementForISA(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(true);
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    localMemoryEnabled = hwHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, hwHelper.useSystemMemoryPlacementForISA(hardwareInfo));

    DebugManager.flags.EnableLocalMemory.set(false);
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    localMemoryEnabled = hwHelper.getEnableLocalMemory(hardwareInfo);
    EXPECT_NE(localMemoryEnabled, hwHelper.useSystemMemoryPlacementForISA(hardwareInfo));
}

TEST_F(HwHelperTest, givenInvalidEngineTypeWhenGettingEngineGroupTypeThenThrow) {
    HwHelper &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_ANY_THROW(hwHelper.getEngineGroupType(aub_stream::EngineType::NUM_ENGINES, EngineUsage::Regular, hardwareInfo));
    EXPECT_ANY_THROW(hwHelper.getEngineGroupType(aub_stream::EngineType::ENGINE_VECS, EngineUsage::Regular, hardwareInfo));
}

HWTEST2_F(HwInfoConfigCommonTest, givenDebugFlagSetWhenEnablingBlitterOperationsSupportThenHonorTheFlag, IsAtLeastGen12lp) {
    DebugManagerStateRestore restore{};
    HardwareInfo hardwareInfo = *defaultHwInfo;

    auto hwInfoConfig = HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    hwInfoConfig->configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_TRUE(hardwareInfo.capabilityTable.blitterOperationsSupported);

    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    hwInfoConfig->configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_FALSE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, GivenVariousValuesWhenAlignSlmSizeIsCalledThenCorrectValueIsReturned) {
    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        EXPECT_EQ(0u, HwHelperHw<FamilyType>::get().alignSlmSize(0));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(1));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(1024));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(1025));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(2048));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(2049));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(4096));
        EXPECT_EQ(8192u, HwHelperHw<FamilyType>::get().alignSlmSize(4097));
        EXPECT_EQ(8192u, HwHelperHw<FamilyType>::get().alignSlmSize(8192));
        EXPECT_EQ(16384u, HwHelperHw<FamilyType>::get().alignSlmSize(8193));
        EXPECT_EQ(16384u, HwHelperHw<FamilyType>::get().alignSlmSize(12288));
        EXPECT_EQ(16384u, HwHelperHw<FamilyType>::get().alignSlmSize(16384));
        EXPECT_EQ(32768u, HwHelperHw<FamilyType>::get().alignSlmSize(16385));
        EXPECT_EQ(32768u, HwHelperHw<FamilyType>::get().alignSlmSize(24576));
        EXPECT_EQ(32768u, HwHelperHw<FamilyType>::get().alignSlmSize(32768));
        EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(32769));
        EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(49152));
        EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(65535));
        EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(65536));
    } else {
        EXPECT_EQ(0u, HwHelperHw<FamilyType>::get().alignSlmSize(0));
        EXPECT_EQ(1024u, HwHelperHw<FamilyType>::get().alignSlmSize(1));
        EXPECT_EQ(1024u, HwHelperHw<FamilyType>::get().alignSlmSize(1024));
        EXPECT_EQ(2048u, HwHelperHw<FamilyType>::get().alignSlmSize(1025));
        EXPECT_EQ(2048u, HwHelperHw<FamilyType>::get().alignSlmSize(2048));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(2049));
        EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(4096));
        EXPECT_EQ(8192u, HwHelperHw<FamilyType>::get().alignSlmSize(4097));
        EXPECT_EQ(8192u, HwHelperHw<FamilyType>::get().alignSlmSize(8192));
        EXPECT_EQ(16384u, HwHelperHw<FamilyType>::get().alignSlmSize(8193));
        EXPECT_EQ(16384u, HwHelperHw<FamilyType>::get().alignSlmSize(16384));
        EXPECT_EQ(32768u, HwHelperHw<FamilyType>::get().alignSlmSize(16385));
        EXPECT_EQ(32768u, HwHelperHw<FamilyType>::get().alignSlmSize(32768));
        EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(32769));
        EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(65536));
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, GivenVariousValuesWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto hwInfo = *defaultHwInfo;

    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        EXPECT_EQ(0u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 0));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 1));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 1024));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 1025));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 2048));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 2049));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 4096));
        EXPECT_EQ(2u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 4097));
        EXPECT_EQ(2u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 8192));
        EXPECT_EQ(4u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 8193));
        EXPECT_EQ(4u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 12288));
        EXPECT_EQ(4u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 16384));
        EXPECT_EQ(8u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 16385));
        EXPECT_EQ(8u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 24576));
        EXPECT_EQ(8u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 32768));
        EXPECT_EQ(16u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 32769));
        EXPECT_EQ(16u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 49152));
        EXPECT_EQ(16u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 65535));
        EXPECT_EQ(16u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 65536));
    } else {
        EXPECT_EQ(0u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 0));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 1));
        EXPECT_EQ(1u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 1024));
        EXPECT_EQ(2u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 1025));
        EXPECT_EQ(2u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 2048));
        EXPECT_EQ(3u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 2049));
        EXPECT_EQ(3u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 4096));
        EXPECT_EQ(4u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 4097));
        EXPECT_EQ(4u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 8192));
        EXPECT_EQ(5u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 8193));
        EXPECT_EQ(5u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 16384));
        EXPECT_EQ(6u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 16385));
        EXPECT_EQ(6u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 32768));
        EXPECT_EQ(7u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 32769));
        EXPECT_EQ(7u, HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 65536));
    }
}

HWTEST_F(HwHelperTest, GivenZeroSlmSizeWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    using SHARED_LOCAL_MEMORY_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
    auto hwInfo = *defaultHwInfo;

    auto receivedSlmSize = static_cast<SHARED_LOCAL_MEMORY_SIZE>(HwHelperHw<FamilyType>::get().computeSlmValues(hwInfo, 0));
    EXPECT_EQ(SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K, receivedSlmSize);
}

HWTEST2_F(HwHelperTest, givenHwHelperWhenCheckingSipWaThenFalseIsReturned, isTglLpOrBelow) {
    auto &helper = HwHelper::get(renderCoreFamily);

    EXPECT_FALSE(helper.isSipWANeeded(*defaultHwInfo));
}
HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenHwHelperWhenGettingPlanarYuvHeightThenHelperReturnsCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(helper.getPlanarYuvMaxHeight(), 16352u);
}

TEST_F(HwHelperTest, WhenGettingIsCpuImageTransferPreferredThenFalseIsReturned) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.isCpuImageTransferPreferred(*defaultHwInfo));
}

TEST_F(HwHelperTest, whenFtrGpGpuMidThreadLevelPreemptFeatureDisabledThenFalseIsReturned) {
    HwHelper &hwHelper = HwHelper::get(renderCoreFamily);
    FeatureTable featureTable = {};
    featureTable.flags.ftrGpGpuMidThreadLevelPreempt = false;
    bool result = hwHelper.isAdditionalFeatureFlagRequired(&featureTable);
    EXPECT_FALSE(result);
}

HWTEST_F(HwHelperTest, whenGettingDefaultRevisionIdThenCorrectValueIsReturned) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    auto revisionId = hwHelper.getDefaultRevisionId(*defaultHwInfo);
    if ((defaultHwInfo->platform.eRenderCoreFamily == IGFX_GEN9_CORE) &&
        (strcmp(defaultHwInfo->capabilityTable.platformType, "core") == 0)) {
        EXPECT_EQ(9u, revisionId);
    } else {
        EXPECT_EQ(0u, revisionId);
    }
}

HWTEST_F(HwHelperTest, whenGettingNumberOfCacheRegionsThenReturnZero) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(0u, hwHelper.getNumCacheRegions());
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, whenCheckingForSmallKernelPreferenceThenFalseIsReturned) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.preferSmallWorkgroupSizeForKernel(0u, this->pDevice->getHardwareInfo()));
    EXPECT_FALSE(hwHelper.preferSmallWorkgroupSizeForKernel(20000u, this->pDevice->getHardwareInfo()));
}

TEST_F(HwHelperTest, givenGenHelperWhenKernelArgumentIsNotPureStatefulThenRequireNonAuxMode) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : {false, true}) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;

        EXPECT_EQ(!argAsPtr.isPureStateful(), clHwHelper.requiresNonAuxMode(argAsPtr, *defaultHwInfo));
    }
}

HWTEST_F(HwHelperTest, whenSetCompressedFlagThenProperFlagSet) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    gmm->resourceParams.Flags.Info.RenderCompressed = 0;

    hwHelper.applyRenderCompressionFlag(*gmm, 1);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.RenderCompressed);

    hwHelper.applyRenderCompressionFlag(*gmm, 0);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.RenderCompressed);
}

HWTEST_F(HwHelperTest, whenAdjustPreemptionSurfaceSizeIsCalledThenCsrSizeDoesntChange) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    size_t csrSize = 1024;
    size_t oldCsrSize = csrSize;
    hwHelper.adjustPreemptionSurfaceSize(csrSize);
    EXPECT_EQ(oldCsrSize, csrSize);
}

HWTEST_F(HwHelperTest, whenSetSipKernelDataIsCalledThenSipKernelDataDoesntChange) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    uint32_t *sipKernelBinary = nullptr;
    uint32_t *oldSipKernelBinary = sipKernelBinary;
    size_t kernelBinarySize = 1024;
    size_t oldKernelBinarySize = kernelBinarySize;
    hwHelper.setSipKernelData(sipKernelBinary, kernelBinarySize);
    EXPECT_EQ(oldKernelBinarySize, kernelBinarySize);
    EXPECT_EQ(oldSipKernelBinary, sipKernelBinary);
}

HWTEST_F(HwHelperTest, whenIsSipKernelAsHexadecimalArrayPreferredIsCalledThenReturnFalse) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.isSipKernelAsHexadecimalArrayPreferred());
}

using isXeHpCoreOrBelow = IsAtMostProduct<IGFX_XE_HP_SDV>;
HWTEST2_F(HwHelperTest, givenXeHPAndBelowPlatformWhenCheckingIfUnTypedDataPortCacheFlushRequiredThenReturnFalse, isXeHpCoreOrBelow) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.unTypedDataPortCacheFlushRequired());
}

HWTEST2_F(HwHelperTest, givenXeHPAndBelowPlatformPlatformWhenCheckingIfEngineTypeRemappingIsRequiredThenReturnFalse, isXeHpCoreOrBelow) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.isEngineTypeRemappingToHwSpecificRequired());
}

HWTEST2_F(HwHelperTest, givenAtMostGen12lpPlatformiWhenCheckingIfScratchSpaceSurfaceStateAccessibleThenFalseIsReturned, IsAtMostGen12lp) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.isScratchSpaceSurfaceStateAccessible());
}

HWTEST2_F(HwHelperTest, givenAtLeastXeHpPlatformWhenCheckingIfScratchSpaceSurfaceStateAccessibleTheniTrueIsReturned, IsAtLeastXeHpCore) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(hwHelper.isScratchSpaceSurfaceStateAccessible());
}

HWTEST_F(HwHelperTest, givenGetRenderSurfaceStateBaseAddressCalledThenCorrectValueIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE renderSurfaceState;
    uint64_t expectedBaseAddress = 0x1122334455667788;
    renderSurfaceState.setSurfaceBaseAddress(expectedBaseAddress);
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(expectedBaseAddress, hwHelper.getRenderSurfaceStateBaseAddress(&renderSurfaceState));
}

HWTEST_F(HwHelperTest, givenGetRenderSurfaceStatePitchCalledThenCorrectValueIsReturned) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE renderSurfaceState;
    uint32_t expectedPitch = 0x400;
    renderSurfaceState.setSurfacePitch(expectedPitch);
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(expectedPitch, hwHelper.getRenderSurfaceStatePitch(&renderSurfaceState));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwHelperTest, givenCLImageFormatsWhenCallingIsFormatRedescribableThenCorrectValueReturned) {
    static const cl_image_format redescribeFormats[] = {
        {CL_R, CL_UNSIGNED_INT8},
        {CL_R, CL_UNSIGNED_INT16},
        {CL_R, CL_UNSIGNED_INT32},
        {CL_RG, CL_UNSIGNED_INT32},
        {CL_RGBA, CL_UNSIGNED_INT32},
    };
    MockContext context;
    auto &clHwHelper = ClHwHelper::get(context.getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);

    const ArrayRef<const ClSurfaceFormatInfo> formats = SurfaceFormats::readWrite();
    for (const auto &format : formats) {
        const cl_image_format oclFormat = format.OCLImageFormat;
        bool expectedResult = true;
        for (const auto &nonRedescribableFormat : redescribeFormats) {
            expectedResult &= (memcmp(&oclFormat, &nonRedescribableFormat, sizeof(cl_image_format)) != 0);
        }
        EXPECT_EQ(expectedResult, clHwHelper.isFormatRedescribable(oclFormat));
    }
}

TEST(HwHelperTests, whenBlitterSupportIsDisabledThenDontExposeAnyBcsEngine) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    hwInfo.featureTable.ftrBcsInfo.set(0);
    const auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto engineUsageTypes = hwHelper.getGpgpuEngineInstances(hwInfo);
    for (auto &engineUsageType : engineUsageTypes) {
        EXPECT_FALSE(EngineHelpers::isBcs(engineUsageType.first));
    }
}

using NotXeHPOrDG2 = AreNotGfxCores<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
HWTEST2_F(HwHelperTest, givenNotAtsOrDg2WhenDisableL3ForDebugCalledThenFalseIsReturned, NotXeHPOrDG2) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.disableL3CacheForDebug(*defaultHwInfo));
}

using ATSOrDG2 = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
HWTEST2_F(HwHelperTest, givenAtsOrDg2WhenDisableL3ForDebugCalledThenTrueIsReturned, ATSOrDG2) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(hwHelper.disableL3CacheForDebug(*defaultHwInfo));
}

HWTEST_F(HwHelperTest, givenHwHelperWhenGettingIfRevisionSpecificBinaryBuiltinIsRequiredThenFalseIsReturned) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.isRevisionSpecificBinaryBuiltinRequired());
}

HWTEST2_F(HwHelperTest, givenDG2HwHelperWhenGettingIsPlatformFlushTaskEnabledThenTrueIsReturned, IsDG2) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_TRUE(hwHelper.isPlatformFlushTaskEnabled(*defaultHwInfo));
}

HWTEST2_F(HwHelperTest, givenPvcHwHelperWhenGettingIsPlatformFlushTaskEnabledThenTrueIsReturned, IsPVC) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_TRUE(hwHelper.isPlatformFlushTaskEnabled(*defaultHwInfo));
}

HWTEST2_F(HwHelperTest, givenAtMostGen12lpHwHelperWhenGettingIsPlatformFlushTaskEnabledThenFalseIsReturned, IsAtMostGen12lp) {
    auto &hwHelper = NEO::HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.isPlatformFlushTaskEnabled(*defaultHwInfo));
}

struct CoherentWANotNeeded {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if (productFamily == IGFX_BROADWELL)
            return false;
        return !TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceGpuNonCoherent;
    }
};
HWTEST2_F(HwHelperTest, givenHwInfoConfigWhenCheckingForceNonGpuCoherencyWAThenPassedValueReturned, CoherentWANotNeeded) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_TRUE(hwHelper.forceNonGpuCoherencyWA(true));
    EXPECT_FALSE(hwHelper.forceNonGpuCoherencyWA(false));
}

struct ForceNonCoherentMode {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if (productFamily == IGFX_BROADWELL)
            return false;
        return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceGpuNonCoherent;
    }
};

HWTEST2_F(HwHelperTest, givenHwInfoConfigWhenCheckingForceNonGpuCoherencyWAThenFalseIsReturned, ForceNonCoherentMode) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(hwHelper.forceNonGpuCoherencyWA(true));
    EXPECT_FALSE(hwHelper.forceNonGpuCoherencyWA(false));
}

HWTEST_F(HwHelperTest, GivenHwInfoWhenGetBatchBufferEndSizeCalledThenCorrectSizeReturned) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(hwHelper.getBatchBufferEndSize(), sizeof(typename FamilyType::MI_BATCH_BUFFER_END));
}

HWTEST_F(HwHelperTest, GivenHwInfoWhenGetBatchBufferEndReferenceCalledThenCorrectPtrReturned) {
    const auto &hwHelper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(hwHelper.getBatchBufferEndReference(), reinterpret_cast<const void *>(&FamilyType::cmdInitBatchBufferEnd));
}