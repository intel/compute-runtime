/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/gen11/reg_configs.h"
#include "runtime/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device.h"

using namespace NEO;

struct Gen11CoherencyRequirements : public ::testing::Test {
    typedef typename ICLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    struct myCsr : public CommandStreamReceiverHw<ICLFamily> {
        using CommandStreamReceiver::commandStream;
        myCsr(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<ICLFamily>(executionEnvironment){};
        CsrSizeRequestFlags *getCsrRequestFlags() { return &csrSizeRequestFlags; }
    };

    void overrideCoherencyRequest(bool requestChanged, bool requireCoherency) {
        csr->getCsrRequestFlags()->coherencyRequestChanged = requestChanged;
        flags.requiresCoherency = requireCoherency;
    }

    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        csr = new myCsr(*device->executionEnvironment);
        device->resetCommandStreamReceiver(csr);
    }

    myCsr *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags = {};
};

GEN11TEST_F(Gen11CoherencyRequirements, coherencyCmdSize) {
    auto lriSize = sizeof(MI_LOAD_REGISTER_IMM);
    overrideCoherencyRequest(false, false);
    auto retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);

    overrideCoherencyRequest(false, true);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);

    overrideCoherencyRequest(true, true);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(lriSize, retSize);

    overrideCoherencyRequest(true, false);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(lriSize, retSize);
}

GEN11TEST_F(Gen11CoherencyRequirements, hdcModeCmdValues) {
    auto lriSize = sizeof(MI_LOAD_REGISTER_IMM);
    char buff[MemoryConstants::pageSize];
    LinearStream stream(buff, MemoryConstants::pageSize);

    auto expectedCmd = FamilyType::cmdInitLoadRegisterImm;
    expectedCmd.setRegisterOffset(gen11HdcModeRegister::address);
    expectedCmd.setDataDword(DwordBuilder::build(gen11HdcModeRegister::forceNonCoherentEnableBit, true));

    overrideCoherencyRequest(true, false);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(csr->getCmdSizeForComputeMode(), stream.getUsed());

    auto cmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedCmd, cmd, lriSize) == 0);

    overrideCoherencyRequest(true, true);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(csr->getCmdSizeForComputeMode() * 2, stream.getUsed());

    cmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(stream.getCpuBase(), lriSize));
    expectedCmd.setDataDword(DwordBuilder::build(gen11HdcModeRegister::forceNonCoherentEnableBit, true, false));
    EXPECT_TRUE(memcmp(&expectedCmd, cmd, lriSize) == 0);
}

struct Gen11CoherencyProgramingTest : public Gen11CoherencyRequirements {

    void SetUp() override {
        Gen11CoherencyRequirements::SetUp();
        startOffset = csr->commandStream.getUsed();
    }

    void flushTask(bool coherencyRequired) {
        flags.requiresCoherency = coherencyRequired;

        auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
        IndirectHeap stream(graphicAlloc);

        startOffset = csr->commandStream.getUsed();
        csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);

        csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
    };

    void findMmio(bool expectToBeProgrammed, uint32_t registerAddress) {
        HardwareParse hwParser;
        hwParser.parseCommands<ICLFamily>(csr->commandStream, startOffset);
        bool foundOne = false;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
            if (cmd && cmd->getRegisterOffset() == registerAddress) {
                EXPECT_FALSE(foundOne);
                foundOne = true;
            }
        }
        EXPECT_EQ(expectToBeProgrammed, foundOne);
    };

    void findMmio(bool expectToBeProgrammed) {
        findMmio(expectToBeProgrammed, gen11HdcModeRegister::address);
    }

    size_t startOffset;
};

GEN11TEST_F(Gen11CoherencyProgramingTest, givenCsrWhenFlushFirstTaskWithoutCoherencyRequiredThenProgramMmio) {
    flushTask(false);
    findMmio(true);
}

GEN11TEST_F(Gen11CoherencyProgramingTest, givenCsrWhenFlushFirstTaskWithCoherencyRequiredThenProgramMmio) {
    flushTask(true);
    findMmio(true);
}

GEN11TEST_F(Gen11CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithCoherencyRequiredWhenFlushNextTaskWithoutChangingCoherencyRequirementThenDoNotProgramMmio) {
    flushTask(true);
    flushTask(true);
    findMmio(false);
}

GEN11TEST_F(Gen11CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithoutCoherencyRequiredWhenFlushNextTaskWithoutChangingCoherencyRequirementThenDoNotProgramMmio) {
    flushTask(false);
    flushTask(false);
    findMmio(false);
}

GEN11TEST_F(Gen11CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithCoherencyRequiredWhenFlushNextTaskWithChangingCoherencyRequirementThenProgramMmio) {
    flushTask(true);
    flushTask(false);
    findMmio(true);
}

GEN11TEST_F(Gen11CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithoutCoherencyRequiredWhenFlushNextTaskWithChangingCoherencyRequirementThenProgramMmio) {
    flushTask(false);
    flushTask(true);
    findMmio(true);
}
