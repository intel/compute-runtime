/*
 * Copyright (c) 2018, Intel Corporation
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

#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "runtime/helpers/dispatch_info.h"

#include "test.h"

using namespace OCLRT;

struct MyMockCsr : UltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> {
    MyMockCsr(const HardwareInfo &hwInfoIn, void *ptr) : UltCommandStreamReceiver(hwInfoIn) {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer *allocationsForResidency) override {
        flushParametrization.wasCalled = true;
        flushParametrization.receivedBatchBuffer = &batchBuffer;
        flushParametrization.receivedEngine = engineOrdinal;
        flushParametrization.receivedAllocationsForResidency = allocationsForResidency;
        return flushParametrization.flushStampToReturn;
    }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        makeResidentParameterization.wasCalled = true;
        makeResidentParameterization.receivedGfxAllocation = &gfxAllocation;
    }

    void processResidency(ResidencyContainer *allocationsForResidency) override {
        processResidencyParameterization.wasCalled = true;
        processResidencyParameterization.receivedAllocationsForResidency = allocationsForResidency;
    }

    void makeNonResident(GraphicsAllocation &gfxAllocation) override {
        makeNonResidentParameterization.wasCalled = true;
        makeNonResidentParameterization.receivedGfxAllocation = &gfxAllocation;
    }

    void activateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override {
        activateAubSubCaptureParameterization.wasCalled = true;
        activateAubSubCaptureParameterization.receivedDispatchInfo = &dispatchInfo;
    }

    struct FlushParameterization {
        bool wasCalled = false;
        FlushStamp flushStampToReturn = 1;
        BatchBuffer *receivedBatchBuffer = nullptr;
        EngineType receivedEngine = EngineType::ENGINE_RCS;
        ResidencyContainer *receivedAllocationsForResidency = nullptr;
    } flushParametrization;

    struct MakeResidentParameterization {
        bool wasCalled = false;
        GraphicsAllocation *receivedGfxAllocation = nullptr;
    } makeResidentParameterization;

    struct ProcessResidencyParameterization {
        bool wasCalled = false;
        ResidencyContainer *receivedAllocationsForResidency = nullptr;
    } processResidencyParameterization;

    struct MakeNonResidentParameterization {
        bool wasCalled = false;
        GraphicsAllocation *receivedGfxAllocation = nullptr;
    } makeNonResidentParameterization;

    struct ActivateAubSubCaptureParameterization {
        bool wasCalled = false;
        const MultiDispatchInfo *receivedDispatchInfo = nullptr;
    } activateAubSubCaptureParameterization;
};

template <typename BaseCSR>
struct MyMockCsrWithAubDump : CommandStreamReceiverWithAUBDump<BaseCSR> {
    MyMockCsrWithAubDump<BaseCSR>(const HardwareInfo &hwInfoIn, bool createAubCSR) : CommandStreamReceiverWithAUBDump<BaseCSR>(hwInfoIn) {
        if (this->aubCSR != nullptr) {
            delete this->aubCSR;
            this->aubCSR = nullptr;
        }
        if (createAubCSR) {
            // overwrite with mock
            this->aubCSR = new MyMockCsr(hwInfoIn, nullptr);
        }
    }

    MyMockCsr &getAubMockCsr() {
        return static_cast<MyMockCsr &>(*this->aubCSR);
    }
};

struct CommandStreamReceiverWithAubDumpTest : public ::testing::TestWithParam<bool /*createAubCSR*/> {
    void SetUp() override {
        createAubCSR = GetParam();
        csrWithAubDump = new MyMockCsrWithAubDump<MyMockCsr>(DEFAULT_TEST_PLATFORM::hwInfo, createAubCSR);
        ASSERT_NE(nullptr, csrWithAubDump);

        memoryManager = csrWithAubDump->createMemoryManager(false);
        ASSERT_NE(nullptr, memoryManager);
    }

    void TearDown() override {
        delete csrWithAubDump;
        delete memoryManager;
    }

    MyMockCsrWithAubDump<MyMockCsr> *csrWithAubDump;
    MemoryManager *memoryManager;
    bool createAubCSR;
};

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenCtorIsCalledThenAubCsrIsInitialized) {
    if (createAubCSR) {
        EXPECT_NE(nullptr, csrWithAubDump->aubCSR);
    } else {
        EXPECT_EQ(nullptr, csrWithAubDump->aubCSR);
    }
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenFlushIsCalledThenBaseCsrFlushStampIsReturned) {
    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemory(4096);
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = OCLRT::ENGINE_RCS;

    ResidencyContainer allocationsForResidency;
    FlushStamp flushStamp = csrWithAubDump->flush(batchBuffer, engineType, &allocationsForResidency);
    EXPECT_EQ(flushStamp, csrWithAubDump->flushParametrization.flushStampToReturn);

    EXPECT_TRUE(csrWithAubDump->flushParametrization.wasCalled);
    EXPECT_EQ(&batchBuffer, csrWithAubDump->flushParametrization.receivedBatchBuffer);
    EXPECT_EQ(engineType, csrWithAubDump->flushParametrization.receivedEngine);
    EXPECT_EQ(&allocationsForResidency, csrWithAubDump->flushParametrization.receivedAllocationsForResidency);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().flushParametrization.wasCalled);
        EXPECT_EQ(&batchBuffer, csrWithAubDump->getAubMockCsr().flushParametrization.receivedBatchBuffer);
        EXPECT_EQ(engineType, csrWithAubDump->getAubMockCsr().flushParametrization.receivedEngine);
        EXPECT_EQ(&allocationsForResidency, csrWithAubDump->getAubMockCsr().flushParametrization.receivedAllocationsForResidency);
    }

    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenMakeResidentIsCalledThenBaseCsrMakeResidentIsCalled) {
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    csrWithAubDump->makeResident(*gfxAllocation);

    EXPECT_TRUE(csrWithAubDump->makeResidentParameterization.wasCalled);
    EXPECT_EQ(gfxAllocation, csrWithAubDump->makeResidentParameterization.receivedGfxAllocation);

    if (createAubCSR) {
        EXPECT_FALSE(csrWithAubDump->getAubMockCsr().makeResidentParameterization.wasCalled);
        EXPECT_EQ(nullptr, csrWithAubDump->getAubMockCsr().makeResidentParameterization.receivedGfxAllocation);
    }

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenProcessResidencyIsCalledThenBothBaseAndAubCsrProcessResidencyIsCalled) {
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    ResidencyContainer allocationsForResidency = {gfxAllocation};
    csrWithAubDump->processResidency(&allocationsForResidency);

    EXPECT_TRUE(csrWithAubDump->processResidencyParameterization.wasCalled);
    EXPECT_EQ(&allocationsForResidency, csrWithAubDump->processResidencyParameterization.receivedAllocationsForResidency);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().processResidencyParameterization.wasCalled);
        EXPECT_EQ(&allocationsForResidency, csrWithAubDump->getAubMockCsr().processResidencyParameterization.receivedAllocationsForResidency);
    }

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenMakeNonResidentIsCalledThenBaseCsrMakeNonResidentIsCalled) {
    auto gfxAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t), sizeof(uint32_t), false, false);
    ASSERT_NE(nullptr, gfxAllocation);

    csrWithAubDump->makeNonResident(*gfxAllocation);

    EXPECT_TRUE(csrWithAubDump->makeNonResidentParameterization.wasCalled);
    EXPECT_EQ(gfxAllocation, csrWithAubDump->makeNonResidentParameterization.receivedGfxAllocation);

    if (createAubCSR) {
        EXPECT_FALSE(csrWithAubDump->getAubMockCsr().makeNonResidentParameterization.wasCalled);
        EXPECT_EQ(nullptr, csrWithAubDump->getAubMockCsr().makeNonResidentParameterization.receivedGfxAllocation);
    }

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenActivateAubSubCaptureIsCalledThenBaseCsrCommandStreamReceiverIsCalled) {
    const DispatchInfo dispatchInfo;
    const MultiDispatchInfo multiDispatchInfo(dispatchInfo);

    csrWithAubDump->activateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(csrWithAubDump->activateAubSubCaptureParameterization.wasCalled);
    EXPECT_EQ(&multiDispatchInfo, csrWithAubDump->activateAubSubCaptureParameterization.receivedDispatchInfo);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().activateAubSubCaptureParameterization.wasCalled);
        EXPECT_EQ(&multiDispatchInfo, csrWithAubDump->getAubMockCsr().activateAubSubCaptureParameterization.receivedDispatchInfo);
    }
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenCreateMemoryManagerIsCalledThenItIsUsedByBothBaseAndAubCsr) {
    EXPECT_EQ(memoryManager, csrWithAubDump->getMemoryManager());
    if (createAubCSR) {
        EXPECT_EQ(memoryManager, csrWithAubDump->aubCSR->getMemoryManager());
    }
}

static bool createAubCSR[] = {
    false,
    true};

INSTANTIATE_TEST_CASE_P(
    CommandStreamReceiverWithAubDumpTest_Create,
    CommandStreamReceiverWithAubDumpTest,
    testing::ValuesIn(createAubCSR));
