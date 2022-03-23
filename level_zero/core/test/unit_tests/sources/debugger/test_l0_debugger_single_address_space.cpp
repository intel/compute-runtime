/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct SingleAddressSpaceFixture : public Test<DeviceFixture> {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        Test<DeviceFixture>::SetUp();
    }

    void TearDown() override {
        Test<DeviceFixture>::TearDown();
    }

    DebugManagerStateRestore restorer;
};

struct L0DebuggerSingleAddressSpace : public Test<L0DebuggerHwFixture> {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        Test<L0DebuggerHwFixture>::SetUp();
    }

    void TearDown() override {
        Test<L0DebuggerHwFixture>::TearDown();
    }

    DebugManagerStateRestore restorer;
};

HWTEST_F(SingleAddressSpaceFixture, givenDebugFlagForceSbaTrackingModeSetWhenDebuggerIsCreatedThenItHasCorrectSingleAddressSpaceValue) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(1);
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    EXPECT_TRUE(debugger->singleAddressSpaceSbaTracking);

    NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(0);
    debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    EXPECT_FALSE(debugger->singleAddressSpaceSbaTracking);
}

HWTEST_F(SingleAddressSpaceFixture, givenSingleAddressSpaceWhenDebuggerIsCreatedThenSbaTrackingGpuVaIsNotReserved) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);

    EXPECT_EQ(0u, debugger->sbaTrackingGpuVa.address);
    EXPECT_EQ(0u, debugger->sbaTrackingGpuVa.size);

    EXPECT_EQ(0u, debugger->getSbaTrackingGpuVa());
    std::vector<NEO::GraphicsAllocation *> allocations;

    auto &allEngines = device->getNEODevice()->getMemoryManager()->getRegisteredEngines();

    for (auto &engine : allEngines) {
        auto sbaAllocation = debugger->getSbaTrackingBuffer(engine.osContext->getContextId());
        ASSERT_NE(nullptr, sbaAllocation);
        allocations.push_back(sbaAllocation);
        EXPECT_EQ(NEO::AllocationType::DEBUG_SBA_TRACKING_BUFFER, sbaAllocation->getAllocationType());
    }

    for (uint32_t i = 0; i < allocations.size() - 1; i++) {
        EXPECT_NE(allocations[i]->getGpuAddress(), allocations[i + 1]->getGpuAddress());
    }
}

HWTEST2_F(SingleAddressSpaceFixture, WhenProgrammingSbaTrackingCommandsForSingleAddressSpaceThenAbortIsCalledAndNoCommandsAreAddedToStream, IsAtMostGen11) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_MATH = typename FamilyType::MI_MATH;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());
    uint64_t gsba = 0x60000;
    uint64_t ssba = 0x1234567000;
    uint64_t iba = 0xfff80000;
    uint64_t ioba = 0x8100000;
    uint64_t dsba = 0xffff0000aaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    EXPECT_THROW(debugger->programSbaTrackingCommandsSingleAddressSpace(cmdStream, sbaAddresses), std::exception);

    EXPECT_EQ(0u, cmdStream.getUsed());
    EXPECT_THROW(debugger->getSbaTrackingCommandsSize(6), std::exception);
}

HWTEST2_F(SingleAddressSpaceFixture, GivenNonZeroSbaAddressesWhenProgrammingSbaTrackingCommandsForSingleAddressSpaceThenCorrectSequenceOfCommandsAreAddedToStream, IsAtLeastGen12lp) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    AllocationProperties commandBufferProperties = {device->getRootDeviceIndex(),
                                                    true,
                                                    MemoryConstants::pageSize,
                                                    AllocationType::COMMAND_BUFFER,
                                                    false,
                                                    device->getNEODevice()->getDeviceBitfield()};
    auto streamAllocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandBufferProperties);
    ASSERT_NE(nullptr, streamAllocation);

    NEO::LinearStream cmdStream;
    cmdStream.replaceGraphicsAllocation(streamAllocation);
    cmdStream.replaceBuffer(streamAllocation->getUnderlyingBuffer(), streamAllocation->getUnderlyingBufferSize());

    uint64_t gsba = 0x60000;
    uint64_t ssba = 0x1234567000;
    uint64_t iba = 0xfff80000;
    uint64_t ioba = 0x8100000;
    uint64_t dsba = 0xffff0000aaaa0000;

    NEO::Debugger::SbaAddresses sbaAddresses = {};
    sbaAddresses.GeneralStateBaseAddress = gsba;
    sbaAddresses.SurfaceStateBaseAddress = ssba;
    sbaAddresses.InstructionBaseAddress = iba;
    sbaAddresses.IndirectObjectBaseAddress = ioba;
    sbaAddresses.DynamicStateBaseAddress = dsba;
    sbaAddresses.BindlessSurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommandsSingleAddressSpace(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    size_t sizeExpected = sizeof(MI_ARB_CHECK) + sizeof(MI_BATCH_BUFFER_START);

    for (int i = 0; i < 6; i++) {
        sizeExpected += NEO::EncodeSetMMIO<FamilyType>::sizeIMM;
        sizeExpected += NEO::EncodeMath<FamilyType>::streamCommandSize;
        sizeExpected += 2 * sizeof(MI_STORE_REGISTER_MEM);
        sizeExpected += 2 * sizeof(MI_STORE_DATA_IMM);
        sizeExpected += sizeof(MI_ARB_CHECK);
        sizeExpected += sizeof(MI_BATCH_BUFFER_START);
        sizeExpected += sizeof(MI_STORE_DATA_IMM);
    }

    sizeExpected += sizeof(MI_ARB_CHECK) + sizeof(MI_BATCH_BUFFER_START);

    EXPECT_EQ(sizeExpected, cmdStream.getUsed());

    EXPECT_EQ(sizeExpected, debugger->getSbaTrackingCommandsSize(6));

    auto itor = find<MI_ARB_CHECK *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    for (int i = 0; i < 6; i++) {
        itor = find<MI_LOAD_REGISTER_IMM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
        EXPECT_EQ(CS_GPR_R0, lri->getRegisterOffset());

        itor = find<MI_MATH *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<MI_ARB_CHECK *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);

        itor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
    }

    itor = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_ARB_CHECK *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto miArb = genCmdCast<MI_ARB_CHECK *>(*itor);

    EXPECT_FALSE(miArb->getPreParserDisable());

    device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(streamAllocation);
}

HWTEST2_F(SingleAddressSpaceFixture, GivenOneNonZeroSbaAddressesWhenProgrammingSbaTrackingCommandsForSingleAddressSpaceThenONlyPartOfCommandsAreAddedToStream, IsAtLeastGen12lp) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    AllocationProperties commandBufferProperties = {device->getRootDeviceIndex(),
                                                    true,
                                                    MemoryConstants::pageSize,
                                                    AllocationType::COMMAND_BUFFER,
                                                    false,
                                                    device->getNEODevice()->getDeviceBitfield()};
    auto streamAllocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandBufferProperties);
    ASSERT_NE(nullptr, streamAllocation);

    NEO::LinearStream cmdStream;
    cmdStream.replaceGraphicsAllocation(streamAllocation);
    cmdStream.replaceBuffer(streamAllocation->getUnderlyingBuffer(), streamAllocation->getUnderlyingBufferSize());

    uint64_t ssba = 0x1234567000;

    NEO::Debugger::SbaAddresses sbaAddresses = {0};
    sbaAddresses.SurfaceStateBaseAddress = ssba;

    debugger->programSbaTrackingCommandsSingleAddressSpace(cmdStream, sbaAddresses);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, cmdStream.getCpuBase(), cmdStream.getUsed()));

    size_t sizeExpected = sizeof(MI_ARB_CHECK) + sizeof(MI_BATCH_BUFFER_START);

    sizeExpected += NEO::EncodeSetMMIO<FamilyType>::sizeIMM;
    sizeExpected += NEO::EncodeMath<FamilyType>::streamCommandSize;
    sizeExpected += 2 * sizeof(MI_STORE_REGISTER_MEM);
    sizeExpected += 2 * sizeof(MI_STORE_DATA_IMM);
    sizeExpected += sizeof(MI_ARB_CHECK);
    sizeExpected += sizeof(MI_BATCH_BUFFER_START);
    sizeExpected += sizeof(MI_STORE_DATA_IMM);

    sizeExpected += sizeof(MI_ARB_CHECK) + sizeof(MI_BATCH_BUFFER_START);

    EXPECT_EQ(sizeExpected, cmdStream.getUsed());
    EXPECT_EQ(sizeExpected, debugger->getSbaTrackingCommandsSize(1));

    auto itor = find<MI_ARB_CHECK *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_LOAD_REGISTER_IMM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itor);
    EXPECT_EQ(CS_GPR_R0, lri->getRegisterOffset());

    itor = find<MI_MATH *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_REGISTER_MEM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_ARB_CHECK *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    itor = find<MI_ARB_CHECK *>(itor, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto miArb = genCmdCast<MI_ARB_CHECK *>(*itor);

    EXPECT_FALSE(miArb->getPreParserDisable());

    device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(streamAllocation);
}

HWTEST2_F(SingleAddressSpaceFixture, GivenAllZeroSbaAddressesWhenProgrammingSbaTrackingCommandsForSingleAddressSpaceThenNoCommandsAreAddedToStream, IsAtLeastGen12lp) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(neoDevice);
    AllocationProperties commandBufferProperties = {device->getRootDeviceIndex(),
                                                    true,
                                                    MemoryConstants::pageSize,
                                                    AllocationType::COMMAND_BUFFER,
                                                    false,
                                                    device->getNEODevice()->getDeviceBitfield()};
    auto streamAllocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandBufferProperties);
    ASSERT_NE(nullptr, streamAllocation);

    NEO::LinearStream cmdStream;
    cmdStream.replaceGraphicsAllocation(streamAllocation);
    cmdStream.replaceBuffer(streamAllocation->getUnderlyingBuffer(), streamAllocation->getUnderlyingBufferSize());
    NEO::Debugger::SbaAddresses sbaAddresses = {0};

    debugger->programSbaTrackingCommandsSingleAddressSpace(cmdStream, sbaAddresses);
    size_t sizeExpected = 0;
    EXPECT_EQ(sizeExpected, cmdStream.getUsed());

    device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(streamAllocation);
}

HWTEST2_F(L0DebuggerSingleAddressSpace, givenDebuggingEnabledWhenCommandListIsExecutedThenValidKernelDebugCommandsAreAdded, IsAtLeastGen12lp) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

    size_t gpr15RegisterCount = 0;

    size_t gprMiLoadindex = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == CS_GPR_R15) {
            gpr15RegisterCount++;
            gprMiLoadindex = i;
        }
        if (miLoad->getRegisterOffset() == CS_GPR_R15 + 4) {
            gpr15RegisterCount++;
        }
    }

    // 2 LRI commands to store SBA buffer address
    EXPECT_EQ(2u, gpr15RegisterCount);

    auto sbaGpuVa = getMockDebuggerL0Hw<FamilyType>()->getSbaTrackingBuffer(commandQueue->getCsr()->getOsContext().getContextId())->getGpuAddress();
    uint32_t low = sbaGpuVa & 0xffffffff;
    uint32_t high = (sbaGpuVa >> 32) & 0xffffffff;

    MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[gprMiLoadindex]);
    EXPECT_EQ(CS_GPR_R15, miLoad->getRegisterOffset());
    EXPECT_EQ(low, miLoad->getDataDword());

    miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[gprMiLoadindex + 1]);
    EXPECT_EQ(CS_GPR_R15 + 4, miLoad->getRegisterOffset());
    EXPECT_EQ(high, miLoad->getDataDword());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
