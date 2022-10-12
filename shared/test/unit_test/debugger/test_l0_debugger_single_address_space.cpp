/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

struct SingleAddressSpaceFixture : public Test<NEO::DeviceFixture> {
    void SetUp() override {
        NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(1);
        Test<NEO::DeviceFixture>::SetUp();
    }

    void TearDown() override {
        Test<NEO::DeviceFixture>::TearDown();
    }

    DebugManagerStateRestore restorer;
};

HWTEST_F(SingleAddressSpaceFixture, givenDebugFlagForceSbaTrackingModeSetWhenDebuggerIsCreatedThenItHasCorrectSingleAddressSpaceValue) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(1);
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    EXPECT_TRUE(debugger->singleAddressSpaceSbaTracking);

    NEO::DebugManager.flags.DebuggerForceSbaTrackingMode.set(0);
    debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    EXPECT_FALSE(debugger->singleAddressSpaceSbaTracking);
}

HWTEST_F(SingleAddressSpaceFixture, givenSingleAddressSpaceWhenDebuggerIsCreatedThenSbaTrackingGpuVaIsNotReserved) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);

    EXPECT_EQ(0u, debugger->sbaTrackingGpuVa.address);
    EXPECT_EQ(0u, debugger->sbaTrackingGpuVa.size);

    EXPECT_EQ(0u, debugger->getSbaTrackingGpuVa());
    std::vector<NEO::GraphicsAllocation *> allocations;

    auto &allEngines = pDevice->getMemoryManager()->getRegisteredEngines();

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
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
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

HWTEST2_F(SingleAddressSpaceFixture, GivenDebuggerDisableSingleAddressSbaTrackingThenNoCommandsProgrammed, IsAtLeastGen12lp) {
    NEO::DebugManager.flags.DebuggerDisableSingleAddressSbaTracking.set(true);
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);

    NEO::LinearStream cmdStream;
    NEO::Debugger::SbaAddresses sbaAddresses = {};

    size_t sizeExpected = 0;
    EXPECT_EQ(sizeExpected, cmdStream.getUsed());
    debugger->programSbaTrackingCommandsSingleAddressSpace(cmdStream, sbaAddresses);
    EXPECT_EQ(sizeExpected, cmdStream.getUsed());
    EXPECT_EQ(sizeExpected, debugger->getSbaTrackingCommandsSize(6));
}

HWTEST2_F(SingleAddressSpaceFixture, GivenNonZeroSbaAddressesWhenProgrammingSbaTrackingCommandsForSingleAddressSpaceThenCorrectSequenceOfCommandsAreAddedToStream, IsAtLeastGen12lp) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    AllocationProperties commandBufferProperties = {pDevice->getRootDeviceIndex(),
                                                    true,
                                                    MemoryConstants::pageSize,
                                                    AllocationType::COMMAND_BUFFER,
                                                    false,
                                                    pDevice->getDeviceBitfield()};
    auto streamAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandBufferProperties);
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
        auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
        EXPECT_LT(cmdSdi->TheStructure.Common.Address, 0xfffffffffffcuL);

        itor = find<MI_STORE_DATA_IMM *>(itor, cmdList.end());
        ASSERT_NE(cmdList.end(), itor);
        cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*itor);
        EXPECT_LT(cmdSdi->TheStructure.Common.Address, 0xfffffffffffcuL);

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

    pDevice->getMemoryManager()->freeGraphicsMemory(streamAllocation);
}

HWTEST2_F(SingleAddressSpaceFixture, GivenOneNonZeroSbaAddressesWhenProgrammingSbaTrackingCommandsForSingleAddressSpaceThenONlyPartOfCommandsAreAddedToStream, IsAtLeastGen12lp) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    using MI_MATH = typename FamilyType::MI_MATH;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    AllocationProperties commandBufferProperties = {pDevice->getRootDeviceIndex(),
                                                    true,
                                                    MemoryConstants::pageSize,
                                                    AllocationType::COMMAND_BUFFER,
                                                    false,
                                                    pDevice->getDeviceBitfield()};
    auto streamAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandBufferProperties);
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

    pDevice->getMemoryManager()->freeGraphicsMemory(streamAllocation);
}

HWTEST2_F(SingleAddressSpaceFixture, GivenAllZeroSbaAddressesWhenProgrammingSbaTrackingCommandsForSingleAddressSpaceThenNoCommandsAreAddedToStream, IsAtLeastGen12lp) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    AllocationProperties commandBufferProperties = {pDevice->getRootDeviceIndex(),
                                                    true,
                                                    MemoryConstants::pageSize,
                                                    AllocationType::COMMAND_BUFFER,
                                                    false,
                                                    pDevice->getDeviceBitfield()};
    auto streamAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandBufferProperties);
    ASSERT_NE(nullptr, streamAllocation);

    NEO::LinearStream cmdStream;
    cmdStream.replaceGraphicsAllocation(streamAllocation);
    cmdStream.replaceBuffer(streamAllocation->getUnderlyingBuffer(), streamAllocation->getUnderlyingBufferSize());
    NEO::Debugger::SbaAddresses sbaAddresses = {0};

    debugger->programSbaTrackingCommandsSingleAddressSpace(cmdStream, sbaAddresses);
    size_t sizeExpected = 0;
    EXPECT_EQ(sizeExpected, cmdStream.getUsed());

    pDevice->getMemoryManager()->freeGraphicsMemory(streamAllocation);
}

} // namespace NEO
