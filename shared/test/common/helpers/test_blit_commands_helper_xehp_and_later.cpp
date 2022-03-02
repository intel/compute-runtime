/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/blit_commands_helper_tests.inl"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "gtest/gtest.h"
#include "test_traits_common.h"

using BlitTests = Test<DeviceFixture>;

using BlitPlatforms = IsAtLeastProduct<IGFX_XE_HP_SDV>;

using namespace NEO;

struct CompressionParamsSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::surfaceStateCompressionParamsSupported;
        }
        return false;
    }
};

HWTEST2_F(BlitTests, givenDeviceWithoutDefaultGmmWhenAppendBlitCommandsForFillBufferThenDstCompressionDisabled, CompressionParamsSupportedMatcher) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_DISABLE);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_NONE);
}

HWTEST2_F(BlitTests, givenGmmWithDisabledCompresionWhenAppendBlitCommandsForFillBufferThenDstCompressionDisabled, CompressionParamsSupportedMatcher) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    gmm->isCompressionEnabled = false;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_DISABLE);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_NONE);
}

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionWhenAppendBlitCommandsForFillBufferThenDstCompressionEnabled, CompressionParamsSupportedMatcher) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    gmm->isCompressionEnabled = true;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_ENABLE);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionWhenAppendBlitCommandsForFillBufferThenSetCompressionFormat, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;

    auto gmmContext = pDevice->getGmmClientContext();
    auto gmm = std::make_unique<MockGmm>(gmmContext);
    gmm->isCompressionEnabled = true;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::LocalMemory);
    mockAllocation.setGmm(gmm.get(), 0);

    uint32_t compressionFormat = gmmContext->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(compressionFormat, blitCmd.getDestinationCompressionFormat());
}

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionAndDebugFlagSetWhenAppendBlitCommandsForFillBufferThenSetCompressionFormat, BlitPlatforms) {
    DebugManagerStateRestore dbgRestore;
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;

    uint32_t newCompressionFormat = 1;
    DebugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    gmm->isCompressionEnabled = true;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::LocalMemory);
    mockAllocation.setGmm(gmm.get(), 0);

    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(newCompressionFormat, blitCmd.getDestinationCompressionFormat());
}

HWTEST2_F(BlitTests, givenA0StepWhenAppendBlitCommandsForFillBufferWithLocalAccessModeCpuAllowedThenSystemMemoryIsUsed, IsXEHP) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);
    HardwareInfo *hwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenA0StepWhenAppendBlitCommandsForFillBufferWithLocalAccessModeCpuDisallowedThenLocalMemoryIsUsed, IsXEHP) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);
    HardwareInfo *hwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, *hwInfo);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
}

HWTEST2_F(BlitTests, givenBStepWhenAppendBlitCommandsForFillBufferWithLocalAccessModeCpuAllowedThenLocalIsUsed, IsXEHP) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);
    HardwareInfo *hwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
}

HWTEST2_F(BlitTests, givenBStepWhenAppendBlitCommandsForFillBufferWithLocalAccessModeCpuDisallowedThenLocalIsUsed, IsXEHP) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);
    HardwareInfo *hwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, *hwInfo);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
}

HWTEST2_F(BlitTests, givenAllocationInSystemMemWhenAppendBlitCommandsForFillBufferWithLocalAccessModeCpuAllowedThenSystemMemIsUsed, IsXEHP) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));

    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    HardwareInfo *hwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, *hwInfo);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenAllocationInSystemMemWhenAppendBlitCommandsForFillBufferWithLocalAccessModeCpuDisallowedThenSystemMemIsUsed, IsXEHP) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed));

    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    HardwareInfo *hwInfo = pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->getMutableHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo->platform.eProductFamily);
    hwInfo->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, *hwInfo);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenOverridedMocksValueWhenAppendBlitCommandsForFillBufferThenDebugMocksValueIsSet, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    uint32_t mockValue = 5;
    DebugManager.flags.OverrideBlitterMocs.set(mockValue);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationMOCS(), mockValue);
}

HWTEST2_F(BlitTests, givenOverridedBliterTargetToZeroWhenAppendBlitCommandsForFillBufferThenUseSystemMem, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.OverrideBlitterTargetMemory.set(0);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenOverridedBliterTargetToOneWhenAppendBlitCommandsForFillBufferThenUseLocalMem, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.OverrideBlitterTargetMemory.set(1);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
}

HWTEST2_F(BlitTests, givenOverridedBliterTargetToTwoWhenAppendBlitCommandsForFillBufferThenUseDefaultMem, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.OverrideBlitterTargetMemory.set(2);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, GivenCpuAccessToLocalMemoryWhenGettingMaxBlitSizeThenValuesAreOverriden, BlitPlatforms) {
    DebugManagerStateRestore restore{};

    UltDeviceFactory deviceFactory{1, 2};
    int32_t testedLocalMemoryAccessModes[] = {static_cast<int32_t>(LocalMemoryAccessMode::Default),
                                              static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed),
                                              static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessDisallowed)};

    for (auto localMemoryAccessModeOverride : testedLocalMemoryAccessModes) {
        DebugManager.flags.ForceLocalMemoryAccessMode.set(localMemoryAccessModeOverride);

        bool isBlitSizeOverridden = (localMemoryAccessModeOverride == static_cast<int32_t>(LocalMemoryAccessMode::CpuAccessAllowed));

        if (isBlitSizeOverridden) {
            EXPECT_EQ(1024u, BlitCommandsHelper<FamilyType>::getMaxBlitWidth(deviceFactory.rootDevices[0]->getRootDeviceEnvironment()));
            EXPECT_EQ(1024u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(deviceFactory.rootDevices[0]->getRootDeviceEnvironment()));
        } else {
            EXPECT_EQ(BlitterConstants::maxBlitWidth,
                      BlitCommandsHelper<FamilyType>::getMaxBlitWidth(deviceFactory.rootDevices[0]->getRootDeviceEnvironment()));
            EXPECT_EQ(BlitterConstants::maxBlitHeight,
                      BlitCommandsHelper<FamilyType>::getMaxBlitHeight(deviceFactory.rootDevices[0]->getRootDeviceEnvironment()));
        }
    }
}

struct BlitTestsTestXeHP : BlitColorTests {};

template <typename FamilyType>
class GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedXEHP : public GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType> {
  public:
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedXEHP(Device *device) : GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammed<FamilyType>(device) {}
};

template <typename FamilyType>
typename FamilyType::XY_COLOR_BLT::COLOR_DEPTH getColorDepth(size_t patternSize) {
    using COLOR_DEPTH = typename FamilyType::XY_COLOR_BLT::COLOR_DEPTH;
    COLOR_DEPTH depth = {};
    switch (patternSize) {
    case 1:
        depth = COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR;
        break;
    case 2:
        depth = COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR;
        break;
    case 4:
        depth = COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR;
        break;
    case 8:
        depth = COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR;
        break;
    case 16:
        depth = COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR;
        break;
    }
    return depth;
}

HWTEST2_P(BlitTestsTestXeHP, givenCommandStreamWhenCallToDispatchMemoryFillThenColorDepthAreProgrammedCorrectly, IsXeHpCore) {
    auto patternSize = GetParam();
    auto expecttedDepth = getColorDepth<FamilyType>(patternSize);
    GivenLinearStreamWhenCallDispatchBlitMemoryColorFillThenCorrectDepthIsProgrammedXEHP<FamilyType> test(pDevice);
    test.TestBodyImpl(patternSize, expecttedDepth);
}

INSTANTIATE_TEST_CASE_P(size_t,
                        BlitTestsTestXeHP,
                        testing::Values(1,
                                        2,
                                        4,
                                        8,
                                        16));

HWTEST2_F(BlitTests, givenOneBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsXeHpCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 1;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenTwoBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsXeHpCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 2;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenFourBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsXeHpCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 4;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenEightBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsXeHpCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 8;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenSixteenBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsXeHpCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 16;
    BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd);
    EXPECT_EQ(bltCmd.getColorDepth(), XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
}

HWTEST2_F(BlitTests, givenIncorrectBytePerPixelWhenAppendColorDepthThenCorrectDepthIsSet, IsXeHpCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    BlitProperties properties = {};
    properties.bytesPerPixel = 48;
    EXPECT_THROW(BlitCommandsHelper<FamilyType>::appendColorDepth(properties, bltCmd), std::exception);
}

HWTEST2_F(BlitTests, givenNotTiledSrcAndDestinationWhenAppendTilingTypeThenCorrectTilingIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    BlitCommandsHelper<FamilyType>::appendTilingType(GMM_NOT_TILED, GMM_NOT_TILED, bltCmd);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::TILING::TILING_LINEAR);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::TILING::TILING_LINEAR);
}
HWTEST2_F(BlitTests, givenTiled4SrcAndDestinationAppendTilingTypeThenCorrectTilingIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    BlitCommandsHelper<FamilyType>::appendTilingType(GMM_TILED_4, GMM_TILED_4, bltCmd);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::TILING::TILING_TILE4);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::TILING::TILING_TILE4);
}

HWTEST2_F(BlitTests, givenTiled64SrcAndDestinationAppendTilingTypeThenCorrectTilingIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    BlitCommandsHelper<FamilyType>::appendTilingType(GMM_TILED_64, GMM_TILED_64, bltCmd);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::TILING::TILING_TILE64);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::TILING::TILING_TILE64);
}

HWTEST2_F(BlitTests, givenTiled4SrcAndDestinationAppendImageCommandsThenCorrectTiledIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.Tile4 = true;
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::TILING::TILING_TILE4);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::TILING::TILING_TILE4);
}

HWTEST2_F(BlitTests, givenNotTiled64SrcAndDestinationAppendImageCommandsThenCorrectTiledIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.Tile64 = true;
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::TILING::TILING_TILE64);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::TILING::TILING_TILE64);
}

HWTEST2_F(BlitTests, givenNotTiledSrcAndDestinationAppendImageCommandsThenCorrectTiledIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.Tile64 = false;
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getSourceTiling(), XY_COPY_BLT::TILING::TILING_LINEAR);
    EXPECT_EQ(bltCmd.getDestinationTiling(), XY_COPY_BLT::TILING::TILING_LINEAR);
}

HWTEST2_F(BlitTests, givenGmmParamsWhenAppendSurfaceTypeThenCorrectSurfaceTypeIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    std::tuple<GMM_RESOURCE_TYPE_ENUM, typename XY_COPY_BLT::SURFACE_TYPE, uint32_t> testParams[]{
        {GMM_RESOURCE_TYPE::RESOURCE_1D, XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D, 1u},
        {GMM_RESOURCE_TYPE::RESOURCE_2D, XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, 1u},
        {GMM_RESOURCE_TYPE::RESOURCE_3D, XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, 1u},
        {GMM_RESOURCE_TYPE::RESOURCE_1D, XY_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, 10u}};

    for (const auto &[resourceType, expectedSurfaceType, arraySize] : testParams) {
        auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
        auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
        resourceInfo->mockResourceCreateParams.Type = resourceType;
        resourceInfo->mockResourceCreateParams.ArraySize = arraySize;
        MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                                 reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                 MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
        MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                                 reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                 MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
        mockAllocationSrc.setGmm(gmm.get(), 0);
        mockAllocationDst.setGmm(gmm.get(), 0);
        auto bltCmd = FamilyType::cmdInitXyCopyBlt;
        BlitProperties properties = {};
        properties.srcAllocation = &mockAllocationSrc;
        properties.dstAllocation = &mockAllocationDst;
        BlitCommandsHelper<FamilyType>::appendSurfaceType(properties, bltCmd);

        EXPECT_EQ(bltCmd.getDestinationSurfaceType(), expectedSurfaceType);
        EXPECT_EQ(bltCmd.getSourceSurfaceType(), expectedSurfaceType);
    }
}

HWTEST2_F(BlitTests, givenInvalidResourceWhenAppendSurfaceTypeThenSurfaceTypeDoesNotChange, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_INVALID;

    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;

    auto srcSurfaceType = bltCmd.getSourceSurfaceType();
    auto dstSurfaceType = bltCmd.getDestinationSurfaceType();

    BlitCommandsHelper<FamilyType>::appendSurfaceType(properties, bltCmd);

    EXPECT_EQ(bltCmd.getSourceSurfaceType(), srcSurfaceType);
    EXPECT_EQ(bltCmd.getDestinationSurfaceType(), dstSurfaceType);
}

HWTEST2_F(BlitTests, givenResourcesWithoutGmmsWhenAppendSurfaceTypeThenSurfaceTypeDoesNotChange, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;

    auto srcSurfaceType = bltCmd.getSourceSurfaceType();
    auto dstSurfaceType = bltCmd.getDestinationSurfaceType();
    BlitCommandsHelper<FamilyType>::appendSurfaceType(properties, bltCmd);

    EXPECT_EQ(bltCmd.getSourceSurfaceType(), srcSurfaceType);
    EXPECT_EQ(bltCmd.getDestinationSurfaceType(), dstSurfaceType);
}

HWTEST2_F(BlitTests, givenGmmParamsWhenGetBlitAllocationPropertiesIsCalledThenCompressionFormatIsSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;

    std::tuple<bool, bool, bool> params[]{
        {false, false, false},
        {false, true, true},
        {true, false, true}};

    for (auto &[mediaCompressed, renderCompressed, compressionExpected] : params) {
        auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
        auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
        auto &resInfo = resourceInfo->getResourceFlags()->Info;
        resInfo.MediaCompressed = mediaCompressed;
        resInfo.RenderCompressed = renderCompressed;
        MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                                 reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                 MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
        mockAllocationSrc.setGmm(gmm.get(), 0);
        BlitProperties properties = {};
        properties.srcAllocation = &mockAllocationSrc;
        uint32_t qPitch = static_cast<uint32_t>(properties.copySize.y);
        GMM_TILE_TYPE tileType = GMM_NOT_TILED;
        uint32_t mipTailLod = 0;
        uint32_t compressionFormat = 0;
        auto rowPitch = static_cast<uint32_t>(properties.srcRowPitch);
        BlitCommandsHelper<FamilyType>::getBlitAllocationProperties(*properties.srcAllocation, rowPitch, qPitch, tileType, mipTailLod, compressionFormat, pDevice->getRootDeviceEnvironment());

        if (compressionExpected) {
            EXPECT_GT(compressionFormat, 0u);
        } else {
            EXPECT_EQ(compressionFormat, 0u);
        }
    }
}

HWTEST2_F(BlitTests, givenA0orA1SteppingAndCpuLocalMemoryAccessWhenCallingAppendExtraMemoryPropertiesThenTargetMemoryIsSet, IsXeHpCore) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);

    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    std::array<std::pair<uint32_t, typename XY_BLOCK_COPY_BLT::TARGET_MEMORY>, 3> testParams = {
        {{REVISION_A0, XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM},
         {REVISION_A1, XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM},
         {REVISION_B, XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM}}};

    for (const auto &[revision, expectedTargetMemory] : testParams) {
        auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
        auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
        hwInfo->platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, *hwInfo);

        BlitCommandsHelper<FamilyType>::appendExtraMemoryProperties(bltCmd, rootDeviceEnvironment);
        EXPECT_EQ(bltCmd.getSourceTargetMemory(), expectedTargetMemory);
        EXPECT_EQ(bltCmd.getDestinationTargetMemory(), expectedTargetMemory);
    }
}

struct MyMockResourecInfo : public GmmResourceInfo {
    using GmmResourceInfo::resourceInfo;

    MyMockResourecInfo(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *inputParams) : GmmResourceInfo(clientContext, inputParams){};
    MyMockResourecInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) : GmmResourceInfo(clientContext, inputGmmResourceInfo){};
    size_t getRenderPitch() override {
        return pitch;
    }
    uint32_t getQPitch() override {
        return 0;
    }
    GMM_RESOURCE_FLAG *getResourceFlags() override {
        return &flags;
    }
    uint32_t getMipTailStartLodSurfaceState() override {
        return 0;
    }
    size_t pitch = 0;
    GMM_RESOURCE_FLAG flags = {};
};
HWTEST2_F(BlitTests, givenResourceWithoutGmmWhenAppendImageCommandsThenPitchEqualPropertiesValue, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));

    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.dstRowPitch = 0x100;
    properties.srcRowPitch = 0x100;
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

    EXPECT_EQ(bltCmd.getDestinationPitch(), properties.dstRowPitch);
    EXPECT_EQ(bltCmd.getSourcePitch(), properties.srcRowPitch);
}
HWTEST2_F(BlitTests, givenInputAndDefaultSlicePitchWhenAppendBlitCommandsForImagesIsCalledThenSlicePitchIsCorrect, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstRowPitch = 0x100;
    properties.srcRowPitch = 0x200;
    properties.copySize = {10, 20, 1};
    properties.srcSize = {20, 18, 1};
    properties.dstSize = {18, 20, 1};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    {
        uint32_t inputSlicePitch = 0x4000;
        uint32_t srcSlicePitch = inputSlicePitch;
        uint32_t dstSlicePitch = inputSlicePitch;
        BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
        EXPECT_EQ(inputSlicePitch, srcSlicePitch);
        EXPECT_EQ(inputSlicePitch, dstSlicePitch);
    }
    {
        uint32_t expectedSrcSlicePitch = static_cast<uint32_t>(properties.srcSize.y * properties.srcRowPitch);
        uint32_t expectedDstSlicePitch = static_cast<uint32_t>(properties.dstSize.y * properties.dstRowPitch);
        uint32_t srcSlicePitch = 0;
        uint32_t dstSlicePitch = 0;
        BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
        EXPECT_EQ(expectedSrcSlicePitch, srcSlicePitch);
        EXPECT_EQ(expectedDstSlicePitch, dstSlicePitch);
    }
}

HWTEST2_F(BlitTests, givenResourceInfoWithZeroPitchWhenAppendImageCommandsThenPitchEqualPropertiesValue, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    GMM_RESCREATE_PARAMS gmmParams = {};
    gmm->gmmResourceInfo.reset(new MyMockResourecInfo(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams));
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.dstRowPitch = 0x100;
    properties.srcRowPitch = 0x100;

    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getDestinationPitch(), properties.dstRowPitch);
    EXPECT_EQ(bltCmd.getSourcePitch(), properties.srcRowPitch);
}

HWTEST2_F(BlitTests, givenTiledAllocationWhenAppendBlitCommandsForImagesThenBlitCmdIsCorrect, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfo = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfo->pitch = 0x100;
    gmm->gmmResourceInfo.reset(myResourecInfo.release());
    auto flags = gmm->gmmResourceInfo->getResourceFlags();
    flags->Info.Tile64 = true;
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.dstRowPitch = 0x1000;
    properties.srcRowPitch = 0x1000;

    properties.srcAllocation = &mockAllocationSrc;
    properties.clearColorAllocation = &mockClearColor;
    properties.dstAllocation = &mockAllocationDst;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getDestinationPitch(), gmm->gmmResourceInfo->getRenderPitch() / sizeof(uint32_t));
    EXPECT_EQ(bltCmd.getSourcePitch(), gmm->gmmResourceInfo->getRenderPitch() / sizeof(uint32_t));
    EXPECT_NE(bltCmd.getDestinationPitch(), static_cast<uint32_t>(properties.dstRowPitch));
    EXPECT_NE(bltCmd.getSourcePitch(), static_cast<uint32_t>(properties.srcRowPitch));
}

HWTEST2_F(BlitTests, givenAlocationsWhenAppendBlitCommandsForImagesThenSurfaceSizesAreProgrammedCorrectly, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor;
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto blitCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.dstRowPitch = 0x100;
    properties.srcRowPitch = 0x100;
    properties.srcSize = {8, 10, 12};
    properties.dstSize = {12, 8, 10};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, blitCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

    EXPECT_EQ(blitCmd.getSourceSurfaceWidth(), properties.srcSize.x);
    EXPECT_EQ(blitCmd.getSourceSurfaceHeight(), properties.srcSize.y);
    EXPECT_EQ(blitCmd.getSourceSurfaceDepth(), properties.srcSize.z);
    EXPECT_EQ(blitCmd.getDestinationSurfaceWidth(), properties.dstSize.x);
    EXPECT_EQ(blitCmd.getDestinationSurfaceHeight(), properties.dstSize.y);
    EXPECT_EQ(blitCmd.getDestinationSurfaceDepth(), properties.dstSize.z);
}

HWTEST2_F(BlitTests, givenLinearResourceInfoWithNotZeroPitchWhenAppendImageCommandsThenPitchEqualValueFromProperties, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
    GMM_RESCREATE_PARAMS gmmParams = {};
    auto myResourecInfo = std::make_unique<MyMockResourecInfo>(pDevice->getRootDeviceEnvironment().getGmmClientContext(), &gmmParams);
    myResourecInfo->pitch = 0x100;
    myResourecInfo->flags.Info.Linear = 1;
    gmm->gmmResourceInfo.reset(myResourecInfo.release());
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    mockAllocationSrc.setGmm(gmm.get(), 0);
    mockAllocationDst.setGmm(gmm.get(), 0);
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.dstRowPitch = 0x1000;
    properties.srcRowPitch = 0x1000;

    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.clearColorAllocation = &mockClearColor;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getDestinationPitch(), properties.dstRowPitch);
    EXPECT_EQ(bltCmd.getSourcePitch(), properties.dstRowPitch);
    EXPECT_NE(bltCmd.getDestinationPitch(), gmm->gmmResourceInfo->getRenderPitch());
    EXPECT_NE(bltCmd.getSourcePitch(), gmm->gmmResourceInfo->getRenderPitch());
}

HWTEST2_F(BlitTests, givenLinearResorcesWhenAppendSliceOffsetsThenAdressAreOffsetted, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockClearColor(reinterpret_cast<void *>(0x1234), sizeof(uint32_t));
    properties.copySize = {0x10, 0x10, 0x1};
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;
    properties.srcSlicePitch = 0x4000;
    properties.dstSlicePitch = 0x8000;
    properties.srcGpuAddress = mockAllocationSrc.getGpuAddress();
    properties.dstGpuAddress = mockAllocationDst.getGpuAddress();
    properties.clearColorAllocation = &mockClearColor;
    properties.bytesPerPixel = 1;
    bltCmd.setSourceTiling(XY_COPY_BLT::TILING::TILING_LINEAR);
    bltCmd.setDestinationTiling(XY_COPY_BLT::TILING::TILING_LINEAR);
    uint32_t index = 1;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, bltCmd, index, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getDestinationBaseAddress(), ptrOffset(mockAllocationDst.getGpuAddress(), dstSlicePitch));
    EXPECT_EQ(bltCmd.getSourceBaseAddress(), ptrOffset(mockAllocationSrc.getGpuAddress(), srcSlicePitch));
}

HWTEST2_F(BlitTests, givenTiledResorcesWhenAppendSliceOffsetsThenIndexsAreSet, IsXeHpCore) {
    using XY_COPY_BLT = typename FamilyType::XY_COPY_BLT;
    auto bltCmd = FamilyType::cmdInitXyCopyBlt;
    BlitProperties properties = {};
    MockGraphicsAllocation mockAllocationSrc(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, AllocationType::INTERNAL_HOST_MEMORY,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    bltCmd.setSourceTiling(XY_COPY_BLT::TILING::TILING_TILE64);
    bltCmd.setDestinationTiling(XY_COPY_BLT::TILING::TILING_TILE64);
    uint32_t index = 1;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);
    BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, bltCmd, index, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);
    EXPECT_EQ(bltCmd.getDestinationArrayIndex(), index + 1);
    EXPECT_EQ(bltCmd.getSourceArrayIndex(), index + 1);
}

HWTEST2_F(BlitTests, givenMemorySizeTwiceBiggerThanMaxWidthWhenFillPatternWithBlitThenHeightIsTwo, IsXeHpCore) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    using COLOR_DEPTH = typename XY_COLOR_BLT::COLOR_DEPTH;
    uint32_t pattern[4] = {1, 0, 0, 0};
    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation mockAllocation(0, AllocationType::INTERNAL_HOST_MEMORY,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, (2 * BlitterConstants::maxBlitWidth * sizeof(uint32_t)),
                                          MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::dispatchBlitMemoryColorFill(&mockAllocation, 0, pattern, sizeof(uint32_t), stream, mockAllocation.getUnderlyingBufferSize(), *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(stream.getCpuBase(), 0), stream.getUsed()));
    auto itor = find<XY_COLOR_BLT *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    {
        auto cmd = genCmdCast<XY_COLOR_BLT *>(*itor);
        EXPECT_EQ(cmd->getDestinationSurfaceType(), XY_COLOR_BLT::DESTINATION_SURFACE_TYPE::DESTINATION_SURFACE_TYPE_2D);
    }
}

using IsXeHPOrAbove = IsAtLeastProduct<IGFX_XE_HP_SDV>;

HWTEST2_F(BlitTests, givenEnabledGlobalCacheInvalidationWhenProgrammingGlobalSequencerFlushThenCommandsAreProgrammed, IsXeHPOrAbove) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.GlobalSequencerFlushOnCopyEngine.set(true);

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_IMM) + sizeof(MI_SEMAPHORE_WAIT);
    auto val = BlitCommandsHelper<FamilyType>::getSizeForGlobalSequencerFlush();
    EXPECT_EQ(expectedSize, val);

    BlitCommandsHelper<FamilyType>::programGlobalSequencerFlush(stream);
    EXPECT_EQ(expectedSize, stream.getUsed());

    auto lriCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(streamBuffer);
    EXPECT_EQ(0xB404u, lriCmd->getRegisterOffset());
    EXPECT_EQ(1u, lriCmd->getDataDword());

    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(++lriCmd);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::REGISTER_POLL_MODE::REGISTER_POLL_MODE_REGISTER_POLL, semaphoreCmd->getRegisterPollMode());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());
    EXPECT_EQ(0xB404u, semaphoreCmd->getSemaphoreGraphicsAddress());
    EXPECT_EQ(0u, semaphoreCmd->getSemaphoreDataDword());
}

HWTEST2_F(BlitTests, givenDisabledGlobalCacheInvalidationWhenProgrammingGlobalSequencerFlushThenCommandsAreProgrammed, IsXeHPOrAbove) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.GlobalSequencerFlushOnCopyEngine.set(false);

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    size_t expectedSize = 0u;
    auto val = BlitCommandsHelper<FamilyType>::getSizeForGlobalSequencerFlush();
    EXPECT_EQ(expectedSize, val);

    BlitCommandsHelper<FamilyType>::programGlobalSequencerFlush(stream);
    EXPECT_EQ(0u, stream.getUsed());
}

HWTEST2_F(BlitTests, givenBcsCommandsHelperWhenMiArbCheckWaRequiredThenReturnTrue, IsXeHPOrAbove) {
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::miArbCheckWaRequired());
}
