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
#include "shared/test/common/cmd_parse/hw_parse.h"
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

HWTEST_F(CommandEncoderTest, givenEncodeDataInMemoryWhenInvalidSizeThenExpectUnrecoverable) {
    size_t size = 3;

    constexpr size_t bufferSize = 64;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    uint32_t data[2] = {0x0, 0x0};

    EXPECT_ANY_THROW(EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, 0x1000, data, size));
    EXPECT_ANY_THROW(EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size));
}

HWTEST_F(CommandEncoderTest, givenEncodeDataInMemoryWhenCorrectSizesProvidedThenProgrammedMemoryIsEncodedInCommands) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr size_t bufferSize = 256;
    alignas(8) uint8_t buffer[bufferSize] = {};
    LinearStream cmdStream(buffer, bufferSize);

    uint32_t data[7] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};

    uint64_t gpuAddress = 0x1000;

    uint32_t dwordCount = 1;
    size_t size = dwordCount * sizeof(uint32_t);
    size_t commandSize = 0;

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM), commandSize);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(1u, storeDataImmItList.size());

    auto storeDataImmIt = storeDataImmItList[0];
    MI_STORE_DATA_IMM *storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    dwordCount = 2;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    // expected is for 2 SDI dword commands - worst-case scenario
    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM) * 2, commandSize);

    // since gpu address is qword aligned, we can use 1 SDI qword command
    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(1u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(data[1], storeDataImm->getDataDword1());

    dwordCount = 3;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(2 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(2u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(data[1], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[2], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    dwordCount = 4;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(3 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(2u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(data[1], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[2], storeDataImm->getDataDword0());
    EXPECT_EQ(data[3], storeDataImm->getDataDword1());

    dwordCount = 5;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(3 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(3u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(data[1], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[2], storeDataImm->getDataDword0());
    EXPECT_EQ(data[3], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[2];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + 2 * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[4], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    dwordCount = 6;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(4 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(3u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(data[1], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[2], storeDataImm->getDataDword0());
    EXPECT_EQ(data[3], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[2];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + 2 * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[4], storeDataImm->getDataDword0());
    EXPECT_EQ(data[5], storeDataImm->getDataDword1());

    dwordCount = 7;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(4 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(4u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(data[1], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[2], storeDataImm->getDataDword0());
    EXPECT_EQ(data[3], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[2];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + 2 * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[4], storeDataImm->getDataDword0());
    EXPECT_EQ(data[5], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[3];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + 3 * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[6], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());
}

HWTEST_F(CommandEncoderTest, givenEncodeDataInMemoryWithMisalignedDstGpuAddressWhenCorrectSizesProvidedThenProgrammedMemoryIsEncodedInCommands) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr size_t bufferSize = 256;
    alignas(8) uint8_t buffer[bufferSize] = {};
    LinearStream cmdStream(buffer, bufferSize);

    uint32_t data[7] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};

    uint64_t gpuAddress = 0x1004;

    uint32_t dwordCount = 1;
    size_t size = dwordCount * sizeof(uint32_t);
    size_t commandSize = 0;

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM), commandSize);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(1u, storeDataImmItList.size());

    auto storeDataImmIt = storeDataImmItList[0];
    MI_STORE_DATA_IMM *storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    dwordCount = 2;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    // expected is for 2 SDI dword commands - worst-case scenario
    EXPECT_EQ(sizeof(MI_STORE_DATA_IMM) * 2, commandSize);

    // since gpu address is qword misaligned, we must use 2 SDI dword commands
    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(2u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[1], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    dwordCount = 3;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(2 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(2u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[1], storeDataImm->getDataDword0());
    EXPECT_EQ(data[2], storeDataImm->getDataDword1());

    dwordCount = 4;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(3 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(3u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[1], storeDataImm->getDataDword0());
    EXPECT_EQ(data[2], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[2];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t) + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[3], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    dwordCount = 5;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(3 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(3u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[1], storeDataImm->getDataDword0());
    EXPECT_EQ(data[2], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[2];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t) + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[3], storeDataImm->getDataDword0());
    EXPECT_EQ(data[4], storeDataImm->getDataDword1());

    dwordCount = 6;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(4 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(4u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[1], storeDataImm->getDataDword0());
    EXPECT_EQ(data[2], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[2];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t) + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[3], storeDataImm->getDataDword0());
    EXPECT_EQ(data[4], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[3];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t) + 2 * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[5], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    dwordCount = 7;
    size = dwordCount * sizeof(uint32_t);
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();

    EncodeDataMemory<FamilyType>::programDataMemory(cmdStream, gpuAddress, data, size);
    commandSize = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(size);

    EXPECT_EQ(4 * sizeof(MI_STORE_DATA_IMM), commandSize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(4u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[0], storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[1], storeDataImm->getDataDword0());
    EXPECT_EQ(data[2], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[2];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t) + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[3], storeDataImm->getDataDword0());
    EXPECT_EQ(data[4], storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[3];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

    EXPECT_EQ(gpuAddress + sizeof(uint32_t) + 2 * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(data[5], storeDataImm->getDataDword0());
    EXPECT_EQ(data[6], storeDataImm->getDataDword1());
}

HWTEST_F(CommandEncoderTest, givenEncodeDataInMemoryWhenProgrammingNoopThenExpectNoopDataInDispatchedCommand) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr size_t sdiCommandSize = sizeof(MI_STORE_DATA_IMM);

    uint32_t expectedNoop[8] = {0};
    uint32_t dispatchedData[8];
    memset(dispatchedData, 0xFF, sizeof(dispatchedData));

    constexpr size_t bufferSize = 256;
    alignas(8) uint8_t buffer[bufferSize] = {0x0};
    alignas(8) uint8_t memory[bufferSize] = {0x0};
    void *memoryPtr = memory;
    void *baseMemoryPtr = memoryPtr;
    LinearStream cmdStream(buffer, bufferSize);

    uint64_t dstGpuAddress = 0x1000;

    // noop 1 dword - 1 dword SDI
    size_t requiredMemorySize = sizeof(uint32_t);
    size_t offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(1u, storeDataImmItList.size());

    auto storeDataImmIt = storeDataImmItList[0];
    MI_STORE_DATA_IMM *storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());

    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 2 dword - 1 qword SDI for aligned address, estimate for misaligned address
    requiredMemorySize = 2 * sizeof(uint32_t);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EXPECT_EQ(2 * sdiCommandSize, offset);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, sdiCommandSize), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(1u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 3 dword - 1 qword SDI + 1 dword SDI for aligned address, estimate for misaligned address - 2 SDI commands
    requiredMemorySize = 3 * sizeof(uint32_t);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EXPECT_EQ(2 * sdiCommandSize, offset);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(2u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress + sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 7 dwords - 3x qword SDI + 1 dword SDI, estimate for misaligned address - 4 SDI commands
    requiredMemorySize = sizeof(expectedNoop) - sizeof(uint32_t);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EXPECT_EQ(4 * sdiCommandSize, offset);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(4u, storeDataImmItList.size());

    // verify qwords
    size_t i = 0;
    for (; i < 3; i++) {
        storeDataImmIt = storeDataImmItList[i];
        storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

        EXPECT_EQ(dstGpuAddress + i * sizeof(uint64_t), storeDataImm->getAddress());
        EXPECT_TRUE(storeDataImm->getStoreQword());
        dispatchedData[i * 2] = storeDataImm->getDataDword0();
        dispatchedData[i * 2 + 1] = storeDataImm->getDataDword1();
    }
    storeDataImmIt = storeDataImmItList[i];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress + i * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    dispatchedData[i * 2] = storeDataImm->getDataDword0();

    EXPECT_EQ(0, memcmp(expectedNoop, dispatchedData, requiredMemorySize));

    memset(dispatchedData, 0xFF, sizeof(dispatchedData));
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    i = 0;
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 8 dwords - 4x qword SDI, estimate for misaligned address - 5 SDI commands
    requiredMemorySize = sizeof(expectedNoop);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EXPECT_EQ(5 * sdiCommandSize, offset);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, 4 * sdiCommandSize), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(4u, storeDataImmItList.size());
    for (; i < 4; i++) {
        storeDataImmIt = storeDataImmItList[i];
        storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

        EXPECT_EQ(dstGpuAddress + i * sizeof(uint64_t), storeDataImm->getAddress());
        EXPECT_TRUE(storeDataImm->getStoreQword());
        dispatchedData[i * 2] = storeDataImm->getDataDword0();
        dispatchedData[i * 2 + 1] = storeDataImm->getDataDword1();
    }
    EXPECT_EQ(0, memcmp(expectedNoop, dispatchedData, requiredMemorySize));
}

HWTEST_F(CommandEncoderTest, givenEncodeDataInMemoryWithMisalignedDstGpuAddressWhenProgrammingNoopThenExpectNoopDataInDispatchedCommand) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    uint32_t expectedNoop[8] = {0};
    uint32_t dispatchedData[8];
    memset(dispatchedData, 0xFF, sizeof(dispatchedData));

    constexpr size_t bufferSize = 256;
    alignas(8) uint8_t buffer[bufferSize] = {0x0};
    alignas(8) uint8_t memory[bufferSize] = {0x0};
    void *memoryPtr = memory;
    void *baseMemoryPtr = memoryPtr;
    LinearStream cmdStream(buffer, bufferSize);

    uint64_t dstGpuAddress = 0x1004;

    // noop 1 dword - 1 dword SDI
    size_t requiredMemorySize = sizeof(uint32_t);
    size_t offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(1u, storeDataImmItList.size());

    auto storeDataImmIt = storeDataImmItList[0];
    MI_STORE_DATA_IMM *storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());

    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 2 dword - 2 dword SDI for misaligned address
    requiredMemorySize = 2 * sizeof(uint32_t);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(2u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 3 dword - 1 dword SDI for misaligned + 1 qword SDI for aligned address
    requiredMemorySize = 3 * sizeof(uint32_t);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(2u, storeDataImmItList.size());

    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    storeDataImmIt = storeDataImmItList[1];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress + sizeof(uint32_t), storeDataImm->getAddress());
    EXPECT_TRUE(storeDataImm->getStoreQword());
    EXPECT_EQ(0u, storeDataImm->getDataDword0());
    EXPECT_EQ(0u, storeDataImm->getDataDword1());

    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 7 dwords - 1 dword SDI + 3x qword SDI
    requiredMemorySize = sizeof(expectedNoop) - sizeof(uint32_t);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(4u, storeDataImmItList.size());

    // initial dword SDI
    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    dispatchedData[0] = storeDataImm->getDataDword0();

    // verify qwords
    size_t i = 1;
    for (; i < 4; i++) {
        storeDataImmIt = storeDataImmItList[i];
        storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

        EXPECT_EQ(dstGpuAddress + sizeof(uint32_t) + (i - 1) * sizeof(uint64_t), storeDataImm->getAddress());
        EXPECT_TRUE(storeDataImm->getStoreQword());
        dispatchedData[i * 2 - 1] = storeDataImm->getDataDword0();
        dispatchedData[i * 2] = storeDataImm->getDataDword1();
    }

    EXPECT_EQ(0, memcmp(expectedNoop, dispatchedData, requiredMemorySize));

    memset(dispatchedData, 0xFF, sizeof(dispatchedData));
    memset(buffer, 0x0, bufferSize);
    cmdStream.replaceBuffer(buffer, bufferSize);
    i = 1;
    hwParser.tearDown();
    memoryPtr = baseMemoryPtr;

    // noop 8 dwords - 1 dword SDI for misaligned 3x qword SDI, 1 dword SDI for reminder
    requiredMemorySize = sizeof(expectedNoop);
    offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(requiredMemorySize);
    EncodeDataMemory<FamilyType>::programNoop(memoryPtr, dstGpuAddress, requiredMemorySize);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);

    EncodeDataMemory<FamilyType>::programNoop(cmdStream, dstGpuAddress, requiredMemorySize);

    hwParser.parseCommands<FamilyType>(cmdStream);
    storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_EQ(5u, storeDataImmItList.size());

    // initial dword SDI
    storeDataImmIt = storeDataImmItList[0];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress, storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    dispatchedData[0] = storeDataImm->getDataDword0();

    // verify qwords
    for (; i < 4; i++) {
        storeDataImmIt = storeDataImmItList[i];
        storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);

        EXPECT_EQ(dstGpuAddress + sizeof(uint32_t) + (i - 1) * sizeof(uint64_t), storeDataImm->getAddress());
        EXPECT_TRUE(storeDataImm->getStoreQword());
        dispatchedData[i * 2 - 1] = storeDataImm->getDataDword0();
        dispatchedData[i * 2] = storeDataImm->getDataDword1();
    }

    // reminder dword SDI
    storeDataImmIt = storeDataImmItList[i];
    storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
    EXPECT_EQ(dstGpuAddress + sizeof(uint32_t) + (i - 1) * sizeof(uint64_t), storeDataImm->getAddress());
    EXPECT_FALSE(storeDataImm->getStoreQword());
    dispatchedData[i * 2 - 1] = storeDataImm->getDataDword0();

    EXPECT_EQ(0, memcmp(expectedNoop, dispatchedData, requiredMemorySize));
}

HWTEST_F(CommandEncoderTest, givenEncodeDataInMemoryWhenProgrammingBbStartThenExpectBbStartCmdDataInDispatchedCommand) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    constexpr size_t bbStartSizeDwordUnits = sizeof(MI_BATCH_BUFFER_START) / sizeof(uint32_t);
    uint32_t bbStartCmdBuffer[bbStartSizeDwordUnits];
    memset(bbStartCmdBuffer, 0x0, sizeof(MI_BATCH_BUFFER_START));

    constexpr size_t bufferSize = 256;
    alignas(8) uint8_t buffer[bufferSize] = {};
    alignas(8) uint8_t memory[bufferSize] = {};
    void *memoryPtr = memory;
    LinearStream cmdStream(buffer, bufferSize);

    uint64_t dstGpuAddress = 0x1000;

    uint64_t bbStartAddress = 0x2000;

    EncodeDataMemory<FamilyType>::programBbStart(cmdStream, dstGpuAddress, bbStartAddress, false, false, false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto storeDataImmItList = findAll<MI_STORE_DATA_IMM *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

    size_t i = 0;
    for (auto storeDataImmIt : storeDataImmItList) {
        auto storeDataImm = reinterpret_cast<MI_STORE_DATA_IMM *>(*storeDataImmIt);
        EXPECT_EQ(dstGpuAddress + i * sizeof(uint64_t), storeDataImm->getAddress());

        bbStartCmdBuffer[2 * i] = storeDataImm->getDataDword0();
        if (storeDataImm->getStoreQword()) {
            ASSERT_TRUE(bbStartSizeDwordUnits > (2 * i + 1));
            bbStartCmdBuffer[2 * i + 1] = storeDataImm->getDataDword1();
        }
        i++;
    }

    auto bbStartCmd = genCmdCast<MI_BATCH_BUFFER_START *>(bbStartCmdBuffer);
    ASSERT_NE(nullptr, bbStartCmd);

    EXPECT_EQ(bbStartAddress, bbStartCmd->getBatchBufferStartAddress());
    EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, bbStartCmd->getSecondLevelBatchBuffer());

    void *baseMemoryPtr = memoryPtr;
    size_t offset = EncodeDataMemory<FamilyType>::getCommandSizeForEncode(sizeof(MI_BATCH_BUFFER_START));
    EncodeDataMemory<FamilyType>::programBbStart(memoryPtr, dstGpuAddress, bbStartAddress, false, false, false);
    EXPECT_EQ(ptrOffset(baseMemoryPtr, offset), memoryPtr);
}
