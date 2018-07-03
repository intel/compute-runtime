/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/gen10/reg_configs.h"
#include "runtime/helpers/hw_helper.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/helpers/hw_parse.h"
#include "test.h"

using namespace OCLRT;

struct Gen10CoherencyRequirements : public ::testing::Test {
    typedef typename CNLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    struct myCsr : public CommandStreamReceiverHw<CNLFamily> {
        using CommandStreamReceiver::commandStream;
        myCsr() : CommandStreamReceiverHw<CNLFamily>(*platformDevices[0]){};
        CsrSizeRequestFlags *getCsrRequestFlags() { return &csrSizeRequestFlags; }
    };

    void overrideCoherencyRequest(bool requestChanged, bool requireCoherency) {
        csr->getCsrRequestFlags()->coherencyRequestChanged = requestChanged;
        flags.requiresCoherency = requireCoherency;
    }

    void SetUp() override {
        csr = new myCsr();
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        device->resetCommandStreamReceiver(csr);
    }

    myCsr *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags = {};
};

GEN10TEST_F(Gen10CoherencyRequirements, coherencyCmdSize) {
    auto lriSize = sizeof(MI_LOAD_REGISTER_IMM);
    overrideCoherencyRequest(false, false);
    auto retSize = csr->getCmdSizeForCoherency();
    EXPECT_EQ(0u, retSize);

    overrideCoherencyRequest(false, true);
    retSize = csr->getCmdSizeForCoherency();
    EXPECT_EQ(0u, retSize);

    overrideCoherencyRequest(true, true);
    retSize = csr->getCmdSizeForCoherency();
    EXPECT_EQ(lriSize, retSize);

    overrideCoherencyRequest(true, false);
    retSize = csr->getCmdSizeForCoherency();
    EXPECT_EQ(lriSize, retSize);
}

GEN10TEST_F(Gen10CoherencyRequirements, coherencyCmdValues) {
    auto lriSize = sizeof(MI_LOAD_REGISTER_IMM);
    char buff[MemoryConstants::pageSize];
    LinearStream stream(buff, MemoryConstants::pageSize);

    auto expectedCmd = MI_LOAD_REGISTER_IMM::sInit();
    expectedCmd.setRegisterOffset(gen10HdcModeRegisterAddresss);
    expectedCmd.setDataDword(DwordBuilder::build(4, true));

    overrideCoherencyRequest(true, false);
    csr->programCoherency(stream, flags);
    EXPECT_EQ(lriSize, stream.getUsed());

    auto cmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedCmd, cmd, lriSize) == 0);

    overrideCoherencyRequest(true, true);
    csr->programCoherency(stream, flags);
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

        auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemory(MemoryConstants::pageSize);
        IndirectHeap stream(graphicAlloc);

        startOffset = csr->commandStream.getUsed();
        csr->flushTask(stream, 0, stream, stream, stream, 0, flags);

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
