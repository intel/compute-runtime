/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct PerContextAddressSpaceFixture : public Test<DeviceFixture> {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(0);
        Test<DeviceFixture>::SetUp();
    }

    void TearDown() override {
        Test<DeviceFixture>::TearDown();
    }

    DebugManagerStateRestore restorer;
};

struct L0DebuggerSimpleParameterizedTest : public ::testing::TestWithParam<int>, DeviceFixture {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(GetParam());
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    DebugManagerStateRestore restorer;
};

using L0DebuggerPerContextAddressSpaceTest = Test<L0DebuggerPerContextAddressSpaceFixture>;

using L0DebuggerTest = Test<L0DebuggerHwFixture>;

using L0DebuggerParameterizedTests = L0DebuggerHwParameterizedFixture;

HWTEST_P(L0DebuggerParameterizedTests, givenL0DebuggerWhenCreatedThenPerContextSbaTrackingBuffersAreAllocated) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    if (!debugger->getSingleAddressSpaceSbaTracking()) {
        EXPECT_NE(0u, debugger->getSbaTrackingGpuVa());
    }
    std::vector<NEO::GraphicsAllocation *> allocations;

    auto &allEngines = neoDevice->getMemoryManager()->getRegisteredEngines();

    for (auto &engine : allEngines) {
        auto sbaAllocation = debugger->getSbaTrackingBuffer(engine.osContext->getContextId());
        ASSERT_NE(nullptr, sbaAllocation);
        allocations.push_back(sbaAllocation);

        EXPECT_EQ(NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER, sbaAllocation->getAllocationType());
        EXPECT_EQ(MemoryPool::System4KBPages, sbaAllocation->getMemoryPool());
    }

    for (uint32_t i = 0; i < allocations.size() - 1; i++) {
        EXPECT_NE(allocations[i], allocations[i + 1]);
    }

    EXPECT_EQ(allEngines.size(), getMockDebuggerL0Hw<FamilyType>()->perContextSbaAllocations.size());
}

HWTEST_F(L0DebuggerTest, givenCreatedL0DebuggerThenSbaTrackingBuffersContainValidHeader) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    for (auto &sbaBuffer : getMockDebuggerL0Hw<FamilyType>()->perContextSbaAllocations) {
        auto sbaAllocation = sbaBuffer.second;
        ASSERT_NE(nullptr, sbaAllocation);

        auto sbaHeader = reinterpret_cast<NEO::SbaTrackedAddresses *>(sbaAllocation->getUnderlyingBuffer());

        EXPECT_STREQ("sbaarea", sbaHeader->magic);
        EXPECT_EQ(0u, sbaHeader->BindlessSamplerStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->BindlessSurfaceStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->DynamicStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->GeneralStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->IndirectObjectBaseAddress);
        EXPECT_EQ(0u, sbaHeader->InstructionBaseAddress);
        EXPECT_EQ(0u, sbaHeader->SurfaceStateBaseAddress);
        EXPECT_EQ(0u, sbaHeader->Version);
    }
}

HWTEST_P(L0DebuggerParameterizedTests, givenL0DebuggerWhenCreatedThenPerContextSbaTrackingBuffersAreAllocatedWithProperStorageInfo) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    if (!debugger->getSingleAddressSpaceSbaTracking()) {
        EXPECT_NE(0u, debugger->getSbaTrackingGpuVa());
    }
    std::vector<NEO::GraphicsAllocation *> allocations;

    for (auto &engine : device->getNEODevice()->getAllEngines()) {
        auto sbaAllocation = debugger->getSbaTrackingBuffer(engine.osContext->getContextId());
        ASSERT_NE(nullptr, sbaAllocation);
        allocations.push_back(sbaAllocation);

        EXPECT_EQ(NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER, sbaAllocation->getAllocationType());
        if (sbaAllocation->isAllocatedInLocalMemoryPool()) {
            EXPECT_EQ(neoDevice->getDeviceBitfield(), sbaAllocation->storageInfo.pageTablesVisibility);
        }
        EXPECT_FALSE(sbaAllocation->storageInfo.cloningOfPageTables);
        EXPECT_FALSE(sbaAllocation->storageInfo.multiStorage);
        EXPECT_FALSE(sbaAllocation->storageInfo.tileInstanced);
        EXPECT_GE(neoDevice->getDeviceBitfield().to_ullong(), sbaAllocation->storageInfo.getMemoryBanks());
        EXPECT_EQ(1u, sbaAllocation->storageInfo.getNumBanks());
    }
}

using L0DebuggerMultiSubDeviceTest = Test<SingleRootMultiSubDeviceFixture>;

HWTEST_F(L0DebuggerMultiSubDeviceTest, givenMultiSubDevicesWhenSbaTrackingBuffersAllocatedThenThereIsSeparatePhysicalStorageForEveryContext) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    const auto &engines = neoDevice->getAllEngines();
    EXPECT_LE(1u, engines.size());

    for (auto &engine : engines) {

        auto contextId = engine.osContext->getContextId();
        const auto &storageInfo = debugger->perContextSbaAllocations[contextId]->storageInfo;

        EXPECT_FALSE(storageInfo.cloningOfPageTables);
        EXPECT_EQ(DeviceBitfield{maxNBitValue(numSubDevices)}, storageInfo.memoryBanks);
        EXPECT_EQ(DeviceBitfield{maxNBitValue(numSubDevices)}, storageInfo.pageTablesVisibility);
        EXPECT_EQ(engine.osContext->getDeviceBitfield().to_ulong(), storageInfo.memoryBanks.to_ulong());
        EXPECT_TRUE(storageInfo.tileInstanced);

        for (uint32_t i = 0; i < numSubDevices; i++) {
            auto sbaHeader = reinterpret_cast<NEO::SbaTrackedAddresses *>(ptrOffset(debugger->perContextSbaAllocations[contextId]->getUnderlyingBuffer(),
                                                                                    debugger->perContextSbaAllocations[contextId]->getUnderlyingBufferSize() * i));

            EXPECT_STREQ("sbaarea", sbaHeader->magic);
            EXPECT_EQ(0u, sbaHeader->BindlessSamplerStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->BindlessSurfaceStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->DynamicStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->GeneralStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->IndirectObjectBaseAddress);
            EXPECT_EQ(0u, sbaHeader->InstructionBaseAddress);
            EXPECT_EQ(0u, sbaHeader->SurfaceStateBaseAddress);
            EXPECT_EQ(0u, sbaHeader->Version);
        }
        if (!debugger->singleAddressSpaceSbaTracking) {
            EXPECT_EQ(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
        } else {
            EXPECT_NE(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
        }
    }

    const auto &subDevice0Engines = neoDevice->getSubDevice(0)->getAllEngines();
    const auto &subDevice1Engines = neoDevice->getSubDevice(1)->getAllEngines();

    auto subDeviceEngineSets = {subDevice0Engines, subDevice1Engines};
    uint64_t subDeviceIndex = 0;
    for (const auto &subDeviceEngines : subDeviceEngineSets) {
        for (auto &engine : subDeviceEngines) {

            auto contextId = engine.osContext->getContextId();
            const auto &storageInfo = debugger->perContextSbaAllocations[contextId]->storageInfo;

            EXPECT_FALSE(storageInfo.cloningOfPageTables);
            EXPECT_EQ(DeviceBitfield{1llu << subDeviceIndex}, storageInfo.memoryBanks);
            EXPECT_EQ(DeviceBitfield{1llu << subDeviceIndex}, storageInfo.pageTablesVisibility);
            EXPECT_EQ(engine.osContext->getDeviceBitfield().to_ulong(), storageInfo.memoryBanks.to_ulong());
            EXPECT_FALSE(storageInfo.tileInstanced);

            if (!debugger->singleAddressSpaceSbaTracking) {
                EXPECT_EQ(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
            } else {
                EXPECT_NE(debugger->sbaTrackingGpuVa.address, debugger->perContextSbaAllocations[contextId]->getGpuAddress());
            }
        }
        subDeviceIndex++;
    }
}

using NotGen8Or11 = AreNotGfxCores<IGFX_GEN8_CORE, IGFX_GEN11_CORE>;
using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;

HWTEST2_F(L0DebuggerPerContextAddressSpaceTest, givenDebuggingEnabledAndRequiredGsbaWhenCommandListIsExecutedThenProgramGsbaWritesToSbaTrackingBuffer, NotGen8Or11) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto commandQueue = whiteboxCast(cmdQ);
    auto cmdQHw = static_cast<CommandQueueHw<gfxCoreFamily> *>(cmdQ);

    if (cmdQHw->estimateStateBaseAddressCmdSize() == 0) {
        commandQueue->destroy();
        GTEST_SKIP();
    }

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    CommandList::fromHandle(commandLists[0])->setCommandListPerThreadScratchSize(4096);

    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    auto gmmHelper = neoDevice->getGmmHelper();

    uint64_t gsbaGpuVa = gmmHelper->canonize(cmdSba->getGeneralStateBaseAddress());
    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsbaGpuVa >> 32), cmdSdi->getDataDword1());

    auto expectedGpuVa = gmmHelper->decanonize(device->getL0Debugger()->getSbaTrackingGpuVa()) + offsetof(NEO::SbaTrackedAddresses, GeneralStateBaseAddress);
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }
    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesArePrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("INFO: Debugger: SBA stored ssh");
    EXPECT_NE(std::string::npos, pos);

    pos = output.find("Debugger: SBA ssh");
    EXPECT_NE(std::string::npos, pos);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();

    commandQueue->destroy();
}

using L0DebuggerSimpleTest = Test<DeviceFixture>;

HWTEST2_F(L0DebuggerSimpleTest, givenNullL0DebuggerAndDebuggerLogsWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(255);

    EXPECT_EQ(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenL0DebuggerAndDebuggerLogsDisabledWhenCommandQueueIsSynchronizedThenSbaAddressesAreNotPrinted, Gen12Plus) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenNonCopyCommandListIsInititalizedOrResetThenSSHAddressIsTracked, Gen12Plus) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    size_t usedSpaceBefore = 0;
    ze_result_t returnValue;
    ze_command_list_handle_t commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
    auto commandList = CommandList::fromHandle(commandListHandle);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);
    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

    uint64_t sshGpuVa = cmdSba->getSurfaceStateBaseAddress();
    auto expectedGpuVa = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE)->getHeapGpuBase();
    EXPECT_EQ(expectedGpuVa, sshGpuVa);
    EXPECT_EQ(1u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->reset();
    EXPECT_EQ(2u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenSbaBufferIsPushedToResidencyContainer, Gen12Plus) {
    ze_command_queue_desc_t queueDesc = {};

    std::unique_ptr<MockCommandQueueHw<gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, false);

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().getContextId());
    bool sbaFound = false;

    for (auto iter : commandQueue->residencyContainerSnapshot) {
        if (iter == sbaBuffer) {
            sbaFound = true;
        }
    }
    EXPECT_TRUE(sbaFound);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();
}

HWTEST_F(PerContextAddressSpaceFixture, givenNonZeroGpuVasWhenProgrammingSbaTrackingThenCorrectCmdsAreAddedToStream) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    debugger->singleAddressSpaceSbaTracking = 0;
    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, GeneralStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0x60000;
    uint64_t ssba = 0x1234567000;
    uint64_t iba = 0xfff80000;
    uint64_t ioba = 0x8100000;
    uint64_t dsba = 0xffffffffaaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    EXPECT_EQ(6 * sizeof(MI_STORE_DATA_IMM), cmdStream.getUsed());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(static_cast<uint32_t>(gsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, SurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, DynamicStateBaseAddress);

    EXPECT_EQ(static_cast<uint32_t>(dsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(dsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, IndirectObjectBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ioba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ioba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, InstructionBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(iba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(iba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());
}

HWTEST_F(PerContextAddressSpaceFixture, givenCanonizedGpuVasWhenProgrammingSbaTrackingThenNonCanonicalAddressesAreStored) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    auto expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, GeneralStateBaseAddress);

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0xffff800000060000;
    uint64_t ssba = 0xffff801234567000;
    uint64_t iba = 0xffff8000fff80000;
    uint64_t ioba = 0xffff800008100000;
    uint64_t dsba = 0xffff8000aaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    EXPECT_EQ(6 * sizeof(MI_STORE_DATA_IMM), cmdStream.getUsed());

    auto sdiItor = find<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sdiItor);
    auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    EXPECT_EQ(static_cast<uint32_t>(gsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(gsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, SurfaceStateBaseAddress);

    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, DynamicStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(dsba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(dsba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, IndirectObjectBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ioba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ioba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, InstructionBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(iba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(iba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());

    sdiItor++;
    cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

    expectedGpuVa = debugger->sbaTrackingGpuVa.address + offsetof(NEO::SbaTrackedAddresses, BindlessSurfaceStateBaseAddress);
    EXPECT_EQ(static_cast<uint32_t>(ssba & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
    EXPECT_EQ(static_cast<uint32_t>(ssba >> 32), cmdSdi->getDataDword1());
    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
    EXPECT_TRUE(cmdSdi->getStoreQword());
}

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenZeroGpuVasWhenProgrammingSbaTrackingThenStreamIsNotUsed, Gen12Plus) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0;
    uint64_t ssba = 0;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommands(cmdStream, sbaAddresses);

    EXPECT_EQ(0u, cmdStream.getUsed());
}

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenNotChangedSurfaceStateWhenCapturingSBAThenNoTrackingCmdsAreAdded, Gen12Plus) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;

    NEO::CommandContainer container;
    container.initialize(neoDevice, nullptr, true);

    NEO::Debugger::SbaAddresses sba = {};
    sba.SurfaceStateBaseAddress = 0x123456000;

    debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
    auto sizeUsed = container.getCommandStream()->getUsed();

    EXPECT_NE(0u, sizeUsed);
    sba.SurfaceStateBaseAddress = 0;

    debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
    auto sizeUsed2 = container.getCommandStream()->getUsed();

    EXPECT_EQ(sizeUsed, sizeUsed2);
}

HWTEST2_P(L0DebuggerSimpleParameterizedTest, givenChangedBaseAddressesWhenCapturingSBAThenTrackingCmdsAreAdded, Gen12Plus) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    debugger->sbaTrackingGpuVa.address = 0x45670000;
    {
        NEO::CommandContainer container;
        container.initialize(neoDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.SurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(neoDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.GeneralStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }

    {
        NEO::CommandContainer container;
        container.initialize(neoDevice, nullptr, true);

        NEO::Debugger::SbaAddresses sba = {};
        sba.BindlessSurfaceStateBaseAddress = 0x123456000;

        debugger->captureStateBaseAddress(*container.getCommandStream(), sba);
        auto sizeUsed = container.getCommandStream()->getUsed();

        EXPECT_NE(0u, sizeUsed);
    }
}

INSTANTIATE_TEST_CASE_P(SBAModesForDebugger, L0DebuggerParameterizedTests, ::testing::Values(0, 1));
INSTANTIATE_TEST_CASE_P(SBAModesForDebugger, L0DebuggerSimpleParameterizedTest, ::testing::Values(0, 1));

} // namespace ult
} // namespace L0
