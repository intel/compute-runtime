/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/gen10/reg_configs.h"
#include "runtime/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_device.h"

using namespace OCLRT;

struct Gen10CoherencyRequirements : public ::testing::Test {
    typedef typename CNLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    struct myCsr : public CommandStreamReceiverHw<CNLFamily> {
        using CommandStreamReceiver::commandStream;
        myCsr(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<CNLFamily>(*platformDevices[0], executionEnvironment){};
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

GEN10TEST_F(Gen10CoherencyRequirements, coherencyCmdSize) {
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

GEN10TEST_F(Gen10CoherencyRequirements, coherencyCmdValues) {
    auto lriSize = sizeof(MI_LOAD_REGISTER_IMM);
    char buff[MemoryConstants::pageSize];
    LinearStream stream(buff, MemoryConstants::pageSize);

    auto expectedCmd = CNLFamily::cmdInitLoadRegisterImm;
    expectedCmd.setRegisterOffset(gen10HdcModeRegisterAddresss);
    expectedCmd.setDataDword(DwordBuilder::build(4, true));

    overrideCoherencyRequest(true, false);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(lriSize, stream.getUsed());

    auto cmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedCmd, cmd, lriSize) == 0);

    overrideCoherencyRequest(true, true);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(lriSize * 2, stream.getUsed());

    cmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(stream.getCpuBase(), lriSize));
    expectedCmd.setDataDword(DwordBuilder::build(4, true, false));
    EXPECT_TRUE(memcmp(&expectedCmd, cmd, lriSize) == 0);
}

struct Gen10CoherencyProgramingTest : public Gen10CoherencyRequirements {

    void SetUp() override {
        Gen10CoherencyRequirements::SetUp();
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

    void findMmio(bool expectToBeProgrammed) {
        HardwareParse hwParser;
        hwParser.parseCommands<CNLFamily>(csr->commandStream, startOffset);
        bool foundOne = false;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
            if (cmd && cmd->getRegisterOffset() == gen10HdcModeRegisterAddresss) {
                EXPECT_FALSE(foundOne);
                foundOne = true;
            }
        }
        EXPECT_EQ(expectToBeProgrammed, foundOne);
    };

    size_t startOffset;
};

GEN10TEST_F(Gen10CoherencyProgramingTest, givenCsrWhenFlushFirstTaskWithoutCoherencyRequiredThenProgramMmio) {
    flushTask(false);
    findMmio(true);
}

GEN10TEST_F(Gen10CoherencyProgramingTest, givenCsrWhenFlushFirstTaskWithCoherencyRequiredThenProgramMmio) {
    flushTask(true);
    findMmio(true);
}

GEN10TEST_F(Gen10CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithCoherencyRequiredWhenFlushNextTaskWithoutChangingCoherencyRequirementThenDoNotProgramMmio) {
    flushTask(true);
    flushTask(true);
    findMmio(false);
}

GEN10TEST_F(Gen10CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithoutCoherencyRequiredWhenFlushNextTaskWithoutChangingCoherencyRequirementThenDoNotProgramMmio) {
    flushTask(false);
    flushTask(false);
    findMmio(false);
}

GEN10TEST_F(Gen10CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithCoherencyRequiredWhenFlushNextTaskWithChangingCoherencyRequirementThenProgramMmio) {
    flushTask(true);
    flushTask(false);
    findMmio(true);
}

GEN10TEST_F(Gen10CoherencyProgramingTest, givenCsrWithFlushedFirstTaskWithoutCoherencyRequiredWhenFlushNextTaskWithChangingCoherencyRequirementThenProgramMmio) {
    flushTask(false);
    flushTask(true);
    findMmio(true);
}
