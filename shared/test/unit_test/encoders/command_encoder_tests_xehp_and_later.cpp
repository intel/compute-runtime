/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"

#include "encode_surface_state_args.h"
#include "test_traits_common.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

using XeHPAndLaterHardwareCommandsTest = testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterHardwareCommandsTest, givenXeHPAndLaterPlatformWhenDoBindingTablePrefetchIsCalledThenReturnsFalse) {
    EXPECT_FALSE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}

HWTEST2_F(XeHPAndLaterHardwareCommandsTest, GivenXeHPAndLaterPlatformWhenSetCoherencyTypeIsCalledThenOnlyEncodingSupportedIsSingleGpuCoherent, IsXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using COHERENCY_TYPE = typename RENDER_SURFACE_STATE::COHERENCY_TYPE;

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    for (COHERENCY_TYPE coherencyType : {RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT}) {
        EncodeSurfaceState<FamilyType>::setCoherencyType(&surfaceState, coherencyType);
        EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, surfaceState.getCoherencyType());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterHardwareCommandsTest, givenXeHPAndLaterPlatformWhenGetAdditionalPipelineSelectSizeIsCalledThenZeroIsReturned) {
    MockDevice device;
    EXPECT_EQ(0u, EncodeWA<FamilyType>::getAdditionalPipelineSelectSize(device, false));
}

using XeHPAndLaterCommandEncoderTest = Test<DeviceFixture>;

HWTEST2_F(XeHPAndLaterCommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsAtLeastXeCore) {
    auto container = CommandContainer();
    if constexpr (FamilyType::isHeaplessRequired()) {
        EXPECT_ANY_THROW(EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false));
        return;
    }
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
    EXPECT_EQ(size, 104ul);
}

HWTEST2_F(XeHPAndLaterCommandEncoderTest, givenCommandContainerWithDirtyHeapWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsHeapfulSupportedAndAtLeastXeCore) {
    auto container = CommandContainer();
    container.setHeapDirty(HeapType::surfaceState);
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
    EXPECT_EQ(size, 104ul);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterHardwareCommandsTest, givenPartitionArgumentFalseWhenAddingStoreDataImmThenExpectCommandFlagFalse) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint64_t gpuAddress = 0xFF0000;
    uint32_t dword0 = 0x123;
    uint32_t dword1 = 0x456;

    constexpr size_t bufferSize = 64;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    EncodeStoreMemory<FamilyType>::programStoreDataImm(cmdStream,
                                                       gpuAddress,
                                                       dword0,
                                                       dword1,
                                                       false,
                                                       false,
                                                       nullptr);

    auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(buffer);
    ASSERT_NE(nullptr, storeDataImm);
    EXPECT_FALSE(storeDataImm->getWorkloadPartitionIdOffsetEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterHardwareCommandsTest, givenPartitionArgumentTrueWhenAddingStoreDataImmThenExpectCommandFlagTrue) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint64_t gpuAddress = 0xFF0000;
    uint32_t dword0 = 0x123;
    uint32_t dword1 = 0x456;

    constexpr size_t bufferSize = 64;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    EncodeStoreMemory<FamilyType>::programStoreDataImm(cmdStream,
                                                       gpuAddress,
                                                       dword0,
                                                       dword1,
                                                       false,
                                                       true,
                                                       nullptr);

    auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(buffer);
    ASSERT_NE(nullptr, storeDataImm);
    EXPECT_TRUE(storeDataImm->getWorkloadPartitionIdOffsetEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterHardwareCommandsTest, givenWorkloadPartitionArgumentTrueWhenAddingStoreRegisterMemThenExpectCommandFlagTrue) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    uint64_t gpuAddress = 0xFFA000;
    uint32_t offset = 0x123;

    constexpr size_t bufferSize = 256;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    EncodeStoreMMIO<FamilyType>::encode(cmdStream,
                                        offset,
                                        gpuAddress,
                                        true,
                                        nullptr,
                                        false);

    auto storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(buffer);
    ASSERT_NE(nullptr, storeRegMem);
    EXPECT_TRUE(storeRegMem->getWorkloadPartitionIdOffsetEnable());

    void *outCmdBuffer = nullptr;
    size_t beforeEncode = cmdStream.getUsed();
    EncodeStoreMMIO<FamilyType>::encode(cmdStream,
                                        offset,
                                        gpuAddress,
                                        true,
                                        &outCmdBuffer,
                                        false);

    storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(ptrOffset(buffer, beforeEncode));
    ASSERT_NE(nullptr, storeRegMem);
    EXPECT_TRUE(storeRegMem->getWorkloadPartitionIdOffsetEnable());
    EXPECT_EQ(storeRegMem, outCmdBuffer);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterCommandEncoderTest, givenOffsetAndValueAndWorkloadPartitionWhenEncodeBitwiseAndValIsCalledThenContainerHasCorrectMathCommands) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    GenCmdList commands;
    CommandContainer cmdContainer;
    cmdContainer.initialize(pDevice, nullptr, HeapSize::defaultHeapSize, true, false);
    constexpr uint32_t regOffset = 0x2000u;
    constexpr uint32_t immVal = 0xbaau;
    constexpr uint64_t dstAddress = 0xDEADCAF0u;
    void *storeRegMem = nullptr;
    EncodeMathMMIO<FamilyType>::encodeBitwiseAndVal(cmdContainer, regOffset, immVal, dstAddress, true, &storeRegMem, false);

    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             ptrOffset(cmdContainer.getCommandStream()->getCpuBase(), 0),
                                             cmdContainer.getCommandStream()->getUsed());

    auto itor = find<MI_LOAD_REGISTER_REG *>(commands.begin(), commands.end());

    // load regOffset to R13
    ASSERT_NE(commands.end(), itor);
    auto cmdLoadReg = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(regOffset, cmdLoadReg->getSourceRegisterAddress());
    EXPECT_EQ(RegisterOffsets::csGprR13, cmdLoadReg->getDestinationRegisterAddress());

    // load immVal to R14
    itor++;
    ASSERT_NE(commands.end(), itor);
    auto cmdLoadImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    EXPECT_EQ(RegisterOffsets::csGprR14, cmdLoadImm->getRegisterOffset());
    EXPECT_EQ(immVal, cmdLoadImm->getDataDword());

    // encodeAluAnd should have its own unit tests, so we only check
    // that the MI_MATH exists and length is set to 3u
    itor++;
    ASSERT_NE(commands.end(), itor);
    auto cmdMath = genCmdCast<MI_MATH *>(*itor);
    EXPECT_EQ(3u, cmdMath->DW0.BitField.DwordLength);

    // store R15 to address
    itor++;
    ASSERT_NE(commands.end(), itor);
    auto cmdMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmdMem, storeRegMem);
    EXPECT_EQ(RegisterOffsets::csGprR12, cmdMem->getRegisterAddress());
    EXPECT_EQ(dstAddress, cmdMem->getMemoryAddress());
    EXPECT_TRUE(cmdMem->getWorkloadPartitionIdOffsetEnable());
}

struct CompressionParamsSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::surfaceStateCompressionParamsSupported;
        }
        return false;
    }
};

using XeHpAndLaterSbaTest = SbaTest;

HWTEST2_F(XeHpAndLaterSbaTest, givenMemoryCompressionEnabledWhenAppendingSbaThenEnableStatelessCompressionForAllStatelessAccesses, CompressionParamsSupportedMatcher) {
    for (auto memoryCompressionState : {MemoryCompressionState::notApplicable, MemoryCompressionState::disabled, MemoryCompressionState::enabled}) {
        auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
        StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
        args.setGeneralStateBaseAddress = true;
        args.memoryCompressionState = memoryCompressionState;

        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
        if (memoryCompressionState == MemoryCompressionState::enabled) {
            EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED, sbaCmd.getEnableMemoryCompressionForAllStatelessAccesses());
        } else {
            EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED, sbaCmd.getEnableMemoryCompressionForAllStatelessAccesses());
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpAndLaterSbaTest, givenNonZeroInternalHeapBaseAddressWhenSettingIsDisabledThenExpectCommandValueZero) {
    constexpr uint64_t ihba = 0x80010000ull;

    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.indirectObjectHeapBaseAddress = ihba;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(0ull, sbaCmd.getGeneralStateBaseAddress());
}
