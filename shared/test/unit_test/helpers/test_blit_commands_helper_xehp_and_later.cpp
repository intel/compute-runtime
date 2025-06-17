/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/blit_commands_helper_tests.inl"

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
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_DISABLE);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_NONE);
}

HWTEST2_F(BlitTests, givenGmmWithDisabledCompresionWhenAppendBlitCommandsForFillBufferThenDstCompressionDisabled, CompressionParamsSupportedMatcher) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(false);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);
    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_DISABLE);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_NONE);
}

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionWhenAppendBlitCommandsForFillBufferThenDstCompressionEnabled, CompressionParamsSupportedMatcher) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);
    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationCompressionEnable(), XY_COLOR_BLT::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_COMPRESSION_ENABLE);
    EXPECT_EQ(blitCmd.getDestinationAuxiliarysurfacemode(), XY_COLOR_BLT::DESTINATION_AUXILIARY_SURFACE_MODE::DESTINATION_AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionWhenAppendBlitCommandsForFillBufferThenSetCompressionFormat, BlitPlatforms) {
    auto blitCmd = FamilyType::cmdInitXyColorBlt;

    auto gmmContext = pDevice->getGmmHelper();
    auto gmm = std::make_unique<MockGmm>(gmmContext);
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    uint32_t compressionFormat = gmmContext->getClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(compressionFormat, blitCmd.getDestinationCompressionFormat());
}

HWTEST2_F(BlitTests, givenGmmWithEnabledCompresionAndDebugFlagSetWhenAppendBlitCommandsForFillBufferThenSetCompressionFormat, BlitPlatforms) {
    DebugManagerStateRestore dbgRestore;
    auto blitCmd = FamilyType::cmdInitXyColorBlt;

    uint32_t newCompressionFormat = 1;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(newCompressionFormat));

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    gmm->setCompressionEnabled(true);
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory, reinterpret_cast<void *>(0x1234),
                                          0x1000, 0, sizeof(uint32_t), MemoryPool::localMemory, MemoryManager::maxOsContextCount);
    mockAllocation.setGmm(gmm.get(), 0);

    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(newCompressionFormat, blitCmd.getDestinationCompressionFormat());
}

HWTEST2_F(BlitTests, givenOverridedMocksValueWhenAppendBlitCommandsForFillBufferThenDebugMocksValueIsSet, BlitPlatforms) {
    DebugManagerStateRestore dbgRestore;
    uint32_t mockValue = pDevice->getGmmHelper()->getL3EnabledMOCS() + 1;

    debugManager.flags.OverrideBlitterMocs.set(mockValue);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);
    EXPECT_EQ(blitCmd.getDestinationMOCS(), mockValue);
}

HWTEST2_F(BlitTests, givenOverridedBliterTargetToZeroWhenAppendBlitCommandsForFillBufferThenUseSystemMem, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.OverrideBlitterTargetMemory.set(0);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_SYSTEM_MEM);
}

HWTEST2_F(BlitTests, givenOverridedBliterTargetToOneWhenAppendBlitCommandsForFillBufferThenUseLocalMem, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.OverrideBlitterTargetMemory.set(1);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
}

HWTEST2_F(BlitTests, givenOverridedBliterTargetToTwoWhenAppendBlitCommandsForFillBufferThenUseDefaultMem, BlitPlatforms) {
    using XY_COLOR_BLT = typename FamilyType::XY_COLOR_BLT;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.OverrideBlitterTargetMemory.set(2);

    auto blitCmd = FamilyType::cmdInitXyColorBlt;
    MockGraphicsAllocation mockAllocation(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                          reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                          MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitCommandsHelper<FamilyType>::appendBlitMemoryOptionsForFillBuffer(&mockAllocation, blitCmd, *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]);

    EXPECT_EQ(blitCmd.getDestinationTargetMemory(), XY_COLOR_BLT::DESTINATION_TARGET_MEMORY::DESTINATION_TARGET_MEMORY_LOCAL_MEM);
}

HWTEST2_F(BlitTests, GivenCpuAccessToLocalMemoryWhenGettingMaxBlitSizeThenValuesAreOverriden, BlitPlatforms) {
    DebugManagerStateRestore restore{};

    UltDeviceFactory deviceFactory{1, 2};
    int32_t testedLocalMemoryAccessModes[] = {static_cast<int32_t>(LocalMemoryAccessMode::defaultMode),
                                              static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessAllowed),
                                              static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessDisallowed)};

    for (auto localMemoryAccessModeOverride : testedLocalMemoryAccessModes) {
        debugManager.flags.ForceLocalMemoryAccessMode.set(localMemoryAccessModeOverride);

        bool isBlitSizeOverridden = (localMemoryAccessModeOverride == static_cast<int32_t>(LocalMemoryAccessMode::cpuAccessAllowed));

        if (isBlitSizeOverridden) {
            EXPECT_EQ(1024u, BlitCommandsHelper<FamilyType>::getMaxBlitWidth(deviceFactory.rootDevices[0]->getRootDeviceEnvironment()));
            EXPECT_EQ(1024u, BlitCommandsHelper<FamilyType>::getMaxBlitHeight(deviceFactory.rootDevices[0]->getRootDeviceEnvironment(), false));
        } else {
            EXPECT_EQ(BlitterConstants::maxBlitWidth,
                      BlitCommandsHelper<FamilyType>::getMaxBlitWidth(deviceFactory.rootDevices[0]->getRootDeviceEnvironment()));
            EXPECT_EQ(BlitterConstants::maxBlitHeight,
                      BlitCommandsHelper<FamilyType>::getMaxBlitHeight(deviceFactory.rootDevices[0]->getRootDeviceEnvironment(), false));
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

HWTEST2_F(BlitTests, givenPlaneWhenGetBlitAllocationPropertiesIsCalledThenCompressionFormatIsProperlyAdjusted, CompressionParamsSupportedMatcher) {
    struct {
        uint8_t returnedCompressionFormat;
        uint8_t expectedCompressionFormat;
        GMM_YUV_PLANE_ENUM plane;
    } testInputs[] = {
        // regular image
        {0x0, 0x0, GMM_NO_PLANE},
        {0xF, 0xF, GMM_NO_PLANE},
        {0x10, 0x10, GMM_NO_PLANE},
        {0x1F, 0x1F, GMM_NO_PLANE},
        // luma plane
        {0x0, 0x0, GMM_PLANE_Y},
        {0xF, 0xF, GMM_PLANE_Y},
        {0x10, 0x0, GMM_PLANE_Y},
        {0x1F, 0xF, GMM_PLANE_Y},
        // chroma plane
        {0x0, 0x10, GMM_PLANE_U},
        {0x0, 0x10, GMM_PLANE_V},
        {0xF, 0x1F, GMM_PLANE_U},
        {0xF, 0x1F, GMM_PLANE_V},
        {0x10, 0x10, GMM_PLANE_U},
        {0x10, 0x10, GMM_PLANE_V},
        {0x1F, 0x1F, GMM_PLANE_U},
        {0x1F, 0x1F, GMM_PLANE_V},
    };

    auto gmm = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto &resInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->getResourceFlags()->Info;
    resInfo.MediaCompressed = true;
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    auto gmmClientContext = static_cast<MockGmmClientContext *>(pDevice->getGmmClientContext());
    mockAllocationSrc.setGmm(gmm.get(), 0);
    BlitProperties properties = {};
    properties.srcAllocation = &mockAllocationSrc;
    uint32_t qPitch = static_cast<uint32_t>(properties.copySize.y);
    GMM_TILE_TYPE tileType = GMM_NOT_TILED;
    uint32_t mipTailLod = 0;
    uint32_t compressionFormat = 0;
    auto rowPitch = static_cast<uint32_t>(properties.srcRowPitch);

    for (auto &testInput : testInputs) {
        gmmClientContext->compressionFormatToReturn = testInput.returnedCompressionFormat;
        BlitCommandsHelper<FamilyType>::getBlitAllocationProperties(*properties.srcAllocation, rowPitch, qPitch, tileType, mipTailLod,
                                                                    compressionFormat, pDevice->getRootDeviceEnvironment(), testInput.plane);

        EXPECT_EQ(testInput.expectedCompressionFormat, compressionFormat);
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
    uint32_t getMipTailStartLODSurfaceState() override {
        return 0;
    }
    size_t pitch = 0;
    GMM_RESOURCE_FLAG flags = {};
};

HWTEST2_F(BlitTests, givenCompressionInfoWhenAppendImageCommandsThenCorrectPropertiesAreSet, IsXeHpgCore) {
    using COMPRESSION_ENABLE = typename FamilyType::XY_BLOCK_COPY_BLT::COMPRESSION_ENABLE;
    auto verifyCompressionFormat = [](bool mediaCompressed, bool renderCompressed, uint32_t compressionFormat) {
        if (mediaCompressed || renderCompressed) {
            EXPECT_GT(compressionFormat, 0u);
        } else {
            EXPECT_EQ(compressionFormat, 0u);
        }
    };

    auto verifyControlSurfaceType = [](bool mediaCompressed, bool renderCompressed, uint32_t controlSurfaceType) {
        using CONTROL_SURFACE_TYPE = typename FamilyType::XY_BLOCK_COPY_BLT::CONTROL_SURFACE_TYPE;
        constexpr auto mediaControlSurfaceType = static_cast<uint32_t>(CONTROL_SURFACE_TYPE::CONTROL_SURFACE_TYPE_MEDIA);
        constexpr auto renderControlSurfaceType = static_cast<uint32_t>(CONTROL_SURFACE_TYPE::CONTROL_SURFACE_TYPE_3D);
        if (mediaCompressed) {
            EXPECT_EQ(controlSurfaceType, mediaControlSurfaceType);
        } else if (renderCompressed) {
            EXPECT_EQ(controlSurfaceType, renderControlSurfaceType);
        } else {
            EXPECT_EQ(controlSurfaceType, 0u);
        }
    };

    BlitProperties properties = {};
    properties.srcSize = {1, 1, 1};
    properties.dstSize = {1, 1, 1};
    properties.dstRowPitch = 0x100;
    properties.srcRowPitch = 0x100;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);

    std::tuple<bool, bool> params[]{
        {false, false},
        {false, true},
        {true, false}};

    for (auto &[mediaCompressedSrc, renderCompressedSrc] : params) {
        auto gmmSrc = std::make_unique<MockGmm>(pDevice->getGmmHelper());
        auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
        resourceInfoSrc->getResourceFlags()->Info.MediaCompressed = mediaCompressedSrc;
        resourceInfoSrc->getResourceFlags()->Info.RenderCompressed = renderCompressedSrc;
        MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                                 reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                 MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
        mockAllocationSrc.setGmm(gmmSrc.get(), 0);
        properties.srcAllocation = &mockAllocationSrc;

        for (auto &[mediaCompressedDst, renderCompressedDst] : params) {
            auto gmmDst = std::make_unique<MockGmm>(pDevice->getGmmHelper());
            auto resourceInfoDst = static_cast<MockGmmResourceInfo *>(gmmDst->gmmResourceInfo.get());
            resourceInfoDst->getResourceFlags()->Info.MediaCompressed = mediaCompressedDst;
            resourceInfoDst->getResourceFlags()->Info.RenderCompressed = renderCompressedDst;
            MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                                     reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                                     MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
            mockAllocationDst.setGmm(gmmDst.get(), 0);
            properties.dstAllocation = &mockAllocationDst;
            auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
            bltCmd.setDestinationCompressionEnable(COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
            bltCmd.setSourceCompressionEnable(COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
            BlitCommandsHelper<FamilyType>::appendBlitCommandsForImages(properties, bltCmd, pDevice->getRootDeviceEnvironment(),
                                                                        srcSlicePitch, dstSlicePitch);

            verifyCompressionFormat(mediaCompressedSrc, renderCompressedSrc, bltCmd.getSourceCompressionFormat());
            verifyCompressionFormat(mediaCompressedDst, renderCompressedDst, bltCmd.getDestinationCompressionFormat());

            verifyControlSurfaceType(mediaCompressedSrc, renderCompressedSrc, bltCmd.getSourceControlSurfaceType());
            verifyControlSurfaceType(mediaCompressedDst, renderCompressedDst, bltCmd.getDestinationControlSurfaceType());
            EXPECT_EQ(bltCmd.getSourceCompressionEnable(), COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
            if (mediaCompressedDst) {
                EXPECT_EQ(bltCmd.getDestinationCompressionEnable(), COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_DISABLE);
            } else {
                EXPECT_EQ(bltCmd.getDestinationCompressionEnable(), COMPRESSION_ENABLE::COMPRESSION_ENABLE_COMPRESSION_ENABLE);
            }
        }
    }
}

using IsXeHPOrAbove = IsAtLeastProduct<IGFX_XE_HP_SDV>;

HWTEST2_F(BlitTests, givenEnabledGlobalCacheInvalidationWhenProgrammingGlobalSequencerFlushThenCommandsAreProgrammed, IsXeHPOrAbove) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.GlobalSequencerFlushOnCopyEngine.set(true);

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    size_t expectedSize = sizeof(MI_LOAD_REGISTER_IMM) + NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait();
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
    debugManager.flags.GlobalSequencerFlushOnCopyEngine.set(false);

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

HWTEST2_F(BlitTests, givenDebugVariableWhenDispatchBlitCommandsForImageRegionIsCalledThenCmdDetailsArePrintedToStdOutput, IsXeHPOrAbove) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintImageBlitBlockCopyCmdDetails.set(true);

    uint32_t streamBuffer[100]{};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));
    MockGraphicsAllocation srcAlloc;
    MockGraphicsAllocation dstAlloc;
    MockGraphicsAllocation clearColorAllocation;
    Vec3<size_t> dstOffsets = {0, 0, 0};
    Vec3<size_t> srcOffsets = {0, 0, 0};
    Vec3<size_t> copySize = {0x1, 0x1, 0x1};
    size_t srcRowPitch = 1;
    size_t srcSlicePitch = 1;
    size_t dstRowPitch = 1;
    size_t dstSlicePitch = 1;
    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(&dstAlloc, &srcAlloc,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, &clearColorAllocation);
    blitProperties.srcSize = {1, 1, 1};
    blitProperties.dstSize = {1, 1, 1};

    StreamCapture capture;
    capture.captureStdout();
    BlitCommandsHelper<FamilyType>::dispatchBlitCommandsForImageRegion(blitProperties, stream, pDevice->getRootDeviceEnvironmentRef());

    std::string output = capture.getCapturedStdout();
    std::stringstream expectedOutput;
    expectedOutput << "Slice index: 0\n"
                   << "ColorDepth: 0\n"
                   << "SourcePitch: 1\n"
                   << "SourceTiling: 0\n"
                   << "SourceX1Coordinate_Left: 0\n"
                   << "SourceY1Coordinate_Top: 0\n"
                   << "SourceBaseAddress: 0\n"
                   << "SourceXOffset: 0\n"
                   << "SourceYOffset: 0\n"
                   << "SourceTargetMemory: 0\n"
                   << "SourceCompressionFormat: 0\n"
                   << "SourceSurfaceHeight: 1\n"
                   << "SourceSurfaceWidth: 1\n"
                   << "SourceSurfaceType: 0\n"
                   << "SourceSurfaceQpitch: 0\n"
                   << "SourceSurfaceDepth: 1\n"
                   << "SourceHorizontalAlign: 0\n"
                   << "SourceVerticalAlign: 0\n"
                   << "SourceArrayIndex: 1\n"
                   << "DestinationPitch: 1\n"
                   << "DestinationTiling: 0\n"
                   << "DestinationX1Coordinate_Left: 0\n"
                   << "DestinationY1Coordinate_Top: 0\n"
                   << "DestinationX2Coordinate_Right: 1\n"
                   << "DestinationY2Coordinate_Bottom: 1\n"
                   << "DestinationBaseAddress: 0\n"
                   << "DestinationXOffset: 0\n"
                   << "DestinationYOffset: 0\n"
                   << "DestinationTargetMemory: 0\n"
                   << "DestinationCompressionFormat: 0\n"
                   << "DestinationSurfaceHeight: 1\n"
                   << "DestinationSurfaceWidth: 1\n"
                   << "DestinationSurfaceType: 0\n"
                   << "DestinationSurfaceQpitch: 0\n"
                   << "DestinationSurfaceDepth: 1\n"
                   << "DestinationHorizontalAlign: 0\n"
                   << "DestinationVerticalAlign: 0\n"
                   << "DestinationArrayIndex: 1\n\n";
    EXPECT_EQ(expectedOutput.str(), output);
}

HWTEST2_F(BlitTests, givenDispatchDummyBlitWhenDummyBlitWaRequiredThenDummyBlitIsProgrammedCorrectly, IsXeHPOrAbove) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceDummyBlitWa.set(-1);

    auto &rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment &>(pDevice->getRootDeviceEnvironmentRef());
    auto releaseHelper = new MockReleaseHelper();
    releaseHelper->isDummyBlitWaRequiredResult = true;
    rootDeviceEnvironment.releaseHelper.reset(releaseHelper);

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    size_t expectedSize = 0u;
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenDefault{};
    auto val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenDefault);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenDefault);
    EXPECT_EQ(0u, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenNotBcs{false, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenNotBcs);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenNotBcs);
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());
    EXPECT_EQ(0u, stream.getUsed());

    EncodeDummyBlitWaArgs waArgsWhenBcs{true, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenBcs);
    EXPECT_NE(0u, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenBcs);
    EXPECT_NE(nullptr, rootDeviceEnvironment.getDummyAllocation());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);

    auto cmdIterator = hwParser.cmdList.begin();
    UnitTestHelper<FamilyType>::verifyDummyBlitWa(&rootDeviceEnvironment, cmdIterator);
}

HWTEST2_F(BlitTests, givenDispatchDummyBlitWhenForceDummyBlitWaSetThenDummyBlitProgrammedCorrectly, IsXeHPOrAbove) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceDummyBlitWa.set(1);

    auto &rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment &>(pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    size_t expectedSize = 0u;
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenDefault{};
    auto val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenDefault);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenDefault);
    EXPECT_EQ(0u, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenNotBcs{false, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenNotBcs);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenNotBcs);
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());
    EXPECT_EQ(0u, stream.getUsed());

    EncodeDummyBlitWaArgs waArgsWhenBcs{true, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenBcs);
    EXPECT_NE(0u, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenBcs);
    EXPECT_NE(nullptr, rootDeviceEnvironment.getDummyAllocation());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);

    auto cmdIterator = hwParser.cmdList.begin();
    UnitTestHelper<FamilyType>::verifyDummyBlitWa(&rootDeviceEnvironment, cmdIterator);
}

struct DummyBlitWithColorBlt {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsXeHPOrAbove::isMatched<productFamily>() && IsNotXeHpcCore::isMatched<productFamily>();
    }
};
using DummyBlitWithMemSet = IsXeHpcCore;

HWTEST2_F(BlitTests, whenGettingSizeForDummyBlitThenColorBltSizeIsReturned, DummyBlitWithColorBlt) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceDummyBlitWa.set(1);
    EncodeDummyBlitWaArgs waArgs{true, &pDevice->getRootDeviceEnvironmentRef()};
    EXPECT_EQ(sizeof(typename FamilyType::XY_COLOR_BLT), BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs));
}

HWTEST2_F(BlitTests, whenGettingSizeForDummyBlitThenMemSetSizeIsReturned, DummyBlitWithMemSet) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceDummyBlitWa.set(1);
    EncodeDummyBlitWaArgs waArgs{true, &pDevice->getRootDeviceEnvironmentRef()};
    EXPECT_EQ(sizeof(typename FamilyType::MEM_SET), BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgs));
}

HWTEST2_F(BlitTests, givenDispatchDummyBlitWhenDummyBlitWaNotRequiredThenAdditionalCommandsAreNotProgrammed, IsXeHPOrAbove) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceDummyBlitWa.set(-1);
    auto &rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment &>(pDevice->getRootDeviceEnvironmentRef());
    rootDeviceEnvironment.releaseHelper = std::make_unique<MockReleaseHelper>();

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    size_t expectedSize = 0u;

    EncodeDummyBlitWaArgs waArgsWhenDefault{};
    auto val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenDefault);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenDefault);
    EXPECT_EQ(expectedSize, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenNotBcs{false, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenNotBcs);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenNotBcs);
    EXPECT_EQ(expectedSize, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenBcs{true, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenBcs);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenBcs);
    EXPECT_EQ(expectedSize, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());
}

HWTEST2_F(BlitTests, givenDispatchDummyBlitWhenForceDummyBlitWaDisabledThenAdditionalCommandsAreNotProgrammed, IsXeHPOrAbove) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceDummyBlitWa.set(0);
    auto &rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment &>(pDevice->getRootDeviceEnvironmentRef());

    uint32_t streamBuffer[100] = {};
    LinearStream stream(streamBuffer, sizeof(streamBuffer));

    size_t expectedSize = 0u;

    EncodeDummyBlitWaArgs waArgsWhenDefault{};
    auto val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenDefault);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenDefault);
    EXPECT_EQ(expectedSize, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenNotBcs{false, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenNotBcs);
    EXPECT_EQ(expectedSize, val);
    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenNotBcs);
    EXPECT_EQ(expectedSize, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());

    EncodeDummyBlitWaArgs waArgsWhenBcs{true, &rootDeviceEnvironment};
    val = BlitCommandsHelper<FamilyType>::getDummyBlitSize(waArgsWhenBcs);
    EXPECT_EQ(expectedSize, val);

    BlitCommandsHelper<FamilyType>::dispatchDummyBlit(stream, waArgsWhenBcs);
    EXPECT_EQ(expectedSize, stream.getUsed());
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());
}

struct IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastGfxCore<IGFX_XE_HP_CORE>::isMatched<productFamily>() && !IsXe2HpgCore::isMatched<productFamily>() && NEO::HwMapper<productFamily>::GfxProduct::supportsSampler;
    }
};

HWTEST2_F(BlitTests, givenXeHPOrAboveTiledResourcesWhenAppendSliceOffsetsIsCalledThenIndexesAreSet, IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;

    auto blitCmd = FamilyType::cmdInitXyBlockCopyBlt;
    blitCmd.setSourceTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE64);
    blitCmd.setDestinationTiling(XY_BLOCK_COPY_BLT::TILING::TILING_TILE64);
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitProperties properties{};
    uint32_t sliceIndex = 1;
    auto srcSlicePitch = static_cast<uint32_t>(properties.srcSlicePitch);
    auto dstSlicePitch = static_cast<uint32_t>(properties.dstSlicePitch);

    BlitCommandsHelper<FamilyType>::appendSliceOffsets(properties, blitCmd, sliceIndex, pDevice->getRootDeviceEnvironment(), srcSlicePitch, dstSlicePitch);

    EXPECT_EQ(blitCmd.getDestinationArrayIndex(), sliceIndex + 1);
    EXPECT_EQ(blitCmd.getSourceArrayIndex(), sliceIndex + 1);
}

HWTEST2_F(BlitTests, givenBltCmdWhenSrcAndDstImage1DTiled4ThenSrcAndDstTypeIs2D, IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;

    auto gmmSrc = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
    resourceInfoSrc->getResourceFlags()->Info.Tile4 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitProperties properties{};
    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmSrc.get(), 0);
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;

    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    NEO::BlitCommandsHelper<FamilyType>::appendSurfaceType(properties, bltCmd);
    EXPECT_EQ(bltCmd.getSourceSurfaceType(), XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
    EXPECT_EQ(bltCmd.getDestinationSurfaceType(), XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
}

HWTEST2_F(BlitTests, givenBltCmdWhenSrcAndDstImage1DTiled64ThenSrcAndDstTypeIs2D, IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;

    auto gmmSrc = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 1;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitProperties properties{};
    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmSrc.get(), 0);
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;

    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    NEO::BlitCommandsHelper<FamilyType>::appendSurfaceType(properties, bltCmd);
    EXPECT_EQ(bltCmd.getSourceSurfaceType(), XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
    EXPECT_EQ(bltCmd.getDestinationSurfaceType(), XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
}

HWTEST2_F(BlitTests, givenBltCmdWhenSrcAndDstImage1DNotTiledThenSrcAndDstTypeIs1D, IsAtLeastXeCoreAndNotXe2HpgCoreWith2DArrayImageSupport) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;

    auto gmmSrc = std::make_unique<MockGmm>(pDevice->getGmmHelper());
    auto resourceInfoSrc = static_cast<MockGmmResourceInfo *>(gmmSrc->gmmResourceInfo.get());
    resourceInfoSrc->getResourceFlags()->Info.Tile64 = 0;
    resourceInfoSrc->getResourceFlags()->Info.Tile4 = 0;
    resourceInfoSrc->mockResourceCreateParams.Type = GMM_RESOURCE_TYPE::RESOURCE_1D;
    MockGraphicsAllocation mockAllocationSrc(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    MockGraphicsAllocation mockAllocationDst(0, 1u /*num gmms*/, AllocationType::internalHostMemory,
                                             reinterpret_cast<void *>(0x1234), 0x1000, 0, sizeof(uint32_t),
                                             MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    BlitProperties properties{};
    mockAllocationSrc.setGmm(gmmSrc.get(), 0);
    mockAllocationDst.setGmm(gmmSrc.get(), 0);
    properties.srcAllocation = &mockAllocationSrc;
    properties.dstAllocation = &mockAllocationDst;

    auto bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    NEO::BlitCommandsHelper<FamilyType>::appendSurfaceType(properties, bltCmd);
    EXPECT_EQ(bltCmd.getSourceSurfaceType(), XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
    EXPECT_EQ(bltCmd.getDestinationSurfaceType(), XY_BLOCK_COPY_BLT::SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_1D);
}
