/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_ELKHARTLAKE);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_TIGERLAKE_LP);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_LAKEFIELD);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_ROCKETLAKE);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_ICELAKE_LP);

using CommandEncoderTest = Test<DeviceFixture>;

using Platforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_ROCKETLAKE>;
HWTEST2_F(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, Platforms) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
    EXPECT_EQ(size, 76ul);
}

HWTEST2_F(CommandEncoderTest, givenTglLpUsingRcsWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsTGLLP) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, true);
    EXPECT_EQ(size, 200ul);
}

HWTEST2_F(CommandEncoderTest, givenTglLpNotUsingRcsWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsTGLLP) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
    EXPECT_EQ(size, 88ul);
}

HWTEST2_F(CommandEncoderTest, givenDg1UsingRcsWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsDG1) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, true);
    EXPECT_EQ(size, 200ul);
}

HWTEST2_F(CommandEncoderTest, givenDg1NotUsingRcsWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsDG1) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
    EXPECT_EQ(size, 88ul);
}

HWTEST2_F(CommandEncoderTest, givenRklUsingRcsWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsRKL) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, true);
    EXPECT_EQ(size, 104ul);
}

HWTEST2_F(CommandEncoderTest, givenRklNotUsingRcsWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsRKL) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
    EXPECT_EQ(size, 88ul);
}

template <typename Family>
struct L0DebuggerSbaAddressSetter : public EncodeStateBaseAddress<Family> {
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;

    void proxySetSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd) {
        EncodeStateBaseAddress<Family>::setSbaAddressesForDebugger(sbaAddress, sbaCmd);
    }
};

HWTEST2_F(CommandEncoderTest, givenSbaCommandWhenGettingSbaAddressesForDebuggerThenCorrectValuesAreReturned, IsAtMostXeHpgCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    STATE_BASE_ADDRESS cmd = FamilyType::cmdInitStateBaseAddress;
    cmd.setInstructionBaseAddress(0x1234000);
    cmd.setSurfaceStateBaseAddress(0x1235000);
    cmd.setGeneralStateBaseAddress(0x1236000);

    NEO::Debugger::SbaAddresses sbaAddress = {};
    auto setter = L0DebuggerSbaAddressSetter<FamilyType>{};
    setter.proxySetSbaAddressesForDebugger(sbaAddress, cmd);
    EXPECT_EQ(0x1234000u, sbaAddress.instructionBaseAddress);
    EXPECT_EQ(0x1235000u, sbaAddress.surfaceStateBaseAddress);
    EXPECT_EQ(0x1236000u, sbaAddress.generalStateBaseAddress);
    EXPECT_EQ(0x0u, sbaAddress.bindlessSurfaceStateBaseAddress);
    EXPECT_EQ(0x0u, sbaAddress.bindlessSamplerStateBaseAddress);
}

HWTEST_F(CommandEncoderTest, GivenDwordStoreWhenAddingStoreDataImmThenExpectDwordProgramming) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    size_t size = EncodeStoreMemory<FamilyType>::getStoreDataImmSize();
    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM), size);

    uint64_t gpuAddress = 0xFF0000;
    uint32_t dword0 = 0x123;
    uint32_t dword1 = 0x456;

    constexpr size_t bufferSize = 64;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    void *outSdiCmd = nullptr;

    EncodeStoreMemory<FamilyType>::programStoreDataImm(cmdStream,
                                                       gpuAddress,
                                                       dword0,
                                                       dword1,
                                                       false,
                                                       false,
                                                       &outSdiCmd);
    size_t usedAfter = cmdStream.getUsed();
    EXPECT_EQ(size, usedAfter);
    ASSERT_NE(nullptr, outSdiCmd);

    auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(buffer);
    ASSERT_NE(nullptr, storeDataImm);
    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_EQ(dword0, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD, storeDataImm->getDwordLength());

    EXPECT_EQ(storeDataImm, outSdiCmd);
}

HWTEST_F(CommandEncoderTest, GivenQwordStoreWhenAddingStoreDataImmThenExpectQwordProgramming) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    size_t size = EncodeStoreMemory<FamilyType>::getStoreDataImmSize();
    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM), size);

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
                                                       true,
                                                       false,
                                                       nullptr);
    size_t usedAfter = cmdStream.getUsed();
    EXPECT_EQ(size, usedAfter);

    auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(buffer);
    ASSERT_NE(nullptr, storeDataImm);
    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_EQ(dword0, storeDataImm->getDataDword0());
    EXPECT_EQ(dword1, storeDataImm->getDataDword1());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_QWORD, storeDataImm->getDwordLength());
}

HWTEST_F(CommandEncoderTest, givenPlatformSupportingMiMemFenceWhenEncodingThenProgramSystemMemoryFence) {
    uint64_t gpuAddress = 0x12340000;
    constexpr size_t bufferSize = 64;

    NEO::MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1234000), gpuAddress, 0x123);

    uint8_t buffer[bufferSize] = {};
    LinearStream cmdStream(buffer, bufferSize);

    size_t size = EncodeMemoryFence<FamilyType>::getSystemMemoryFenceSize();

    EncodeMemoryFence<FamilyType>::encodeSystemMemoryFence(cmdStream, &allocation);

    if constexpr (FamilyType::isUsingMiMemFence) {
        using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

        STATE_SYSTEM_MEM_FENCE_ADDRESS expectedCmd = FamilyType::cmdInitStateSystemMemFenceAddress;
        expectedCmd.setSystemMemoryFenceAddress(gpuAddress);

        EXPECT_EQ(sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS), size);
        EXPECT_EQ(sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS), cmdStream.getUsed());

        EXPECT_EQ(0, memcmp(buffer, &expectedCmd, sizeof(STATE_SYSTEM_MEM_FENCE_ADDRESS)));
    } else {
        EXPECT_EQ(0u, size);
        EXPECT_EQ(0u, cmdStream.getUsed());
    }
}

HWTEST2_F(CommandEncoderTest, whenAdjustCompressionFormatForPlanarImageThenNothingHappens, IsGen12LP) {
    for (auto plane : {GMM_NO_PLANE, GMM_PLANE_Y, GMM_PLANE_U, GMM_PLANE_V}) {
        uint32_t compressionFormat = 0u;
        EncodeWA<FamilyType>::adjustCompressionFormatForPlanarImage(compressionFormat, plane);
        EXPECT_EQ(0u, compressionFormat);

        compressionFormat = 0xFFu;
        EncodeWA<FamilyType>::adjustCompressionFormatForPlanarImage(compressionFormat, plane);
        EXPECT_EQ(0xFFu, compressionFormat);
    }
}

HWTEST2_F(CommandEncoderTest, givenPredicateBitSetWhenProgrammingBbStartThenSetCorrectBit, MatchAny) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START cmd = {};
    LinearStream cmdStream(&cmd, sizeof(MI_BATCH_BUFFER_START));

    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(&cmdStream, 0, false, false, true);
    EXPECT_EQ(1u, cmd.getPredicationEnable());
}

HWTEST_F(CommandEncoderTest, givenEncodePostSyncArgsWhenCallingRequiresSystemMemoryFenceThenCorrectValuesAreReturned) {
    EncodePostSyncArgs args{};
    for (bool hostScopeSignalEvent : {true, false}) {
        for (bool kernelUsingSystemAllocation : {true, false}) {
            args.device = pDevice;
            args.isHostScopeSignalEvent = hostScopeSignalEvent;
            args.isUsingSystemAllocation = kernelUsingSystemAllocation;

            if (hostScopeSignalEvent && kernelUsingSystemAllocation && pDevice->getProductHelper().isGlobalFenceInPostSyncRequired(pDevice->getHardwareInfo())) {
                EXPECT_TRUE(args.requiresSystemMemoryFence());
            } else {
                EXPECT_FALSE(args.requiresSystemMemoryFence());
            }
        }
    }
}

HWTEST_F(CommandEncoderTest, givenEncodePostSyncArgsWhenCallingIsRegularEventThenCorrectValuesAreReturned) {
    EncodePostSyncArgs args{};
    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, pDevice->getMemoryManager());
    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), deviceTagAllocator.getTag(), *pDevice, 1, false); // setting duplicateStorage = true;
    for (bool inOrderExec : {true, false}) {
        for (uint64_t eventAddress : {0, 0x1010}) {
            args.device = pDevice;
            args.inOrderExecInfo = (inOrderExec) ? reinterpret_cast<InOrderExecInfo *>(0x1234) : nullptr;
            args.eventAddress = eventAddress;
            bool expectedRegularEvent = (eventAddress != 0 && !inOrderExec);
            EXPECT_EQ(expectedRegularEvent, args.isRegularEvent());
        }
    }
}
