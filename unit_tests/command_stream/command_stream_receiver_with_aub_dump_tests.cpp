/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/os_interface/os_context.h"

#include "test.h"

using namespace OCLRT;

struct MyMockCsr : UltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> {
    MyMockCsr(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment)
        : UltCommandStreamReceiver(hwInfoIn, executionEnvironment) {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer &allocationsForResidency, OsContext &osContext) override {
        flushParametrization.wasCalled = true;
        flushParametrization.receivedBatchBuffer = &batchBuffer;
        flushParametrization.receivedEngine = engineOrdinal;
        flushParametrization.receivedAllocationsForResidency = &allocationsForResidency;
        return flushParametrization.flushStampToReturn;
    }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        makeResidentParameterization.wasCalled = true;
        makeResidentParameterization.receivedGfxAllocation = &gfxAllocation;
    }

    void processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) override {
        processResidencyParameterization.wasCalled = true;
        processResidencyParameterization.receivedAllocationsForResidency = &allocationsForResidency;
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
    MyMockCsrWithAubDump<BaseCSR>(const HardwareInfo &hwInfoIn, bool createAubCSR, ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverWithAUBDump<BaseCSR>(hwInfoIn, executionEnvironment) {
        if (this->aubCSR != nullptr) {
            delete this->aubCSR;
            this->aubCSR = nullptr;
        }
        if (createAubCSR) {
            // overwrite with mock
            this->aubCSR = new MyMockCsr(hwInfoIn, executionEnvironment);
        }
    }

    MyMockCsr &getAubMockCsr() {
        return static_cast<MyMockCsr &>(*this->aubCSR);
    }
};

struct CommandStreamReceiverWithAubDumpTest : public ::testing::TestWithParam<bool /*createAubCSR*/> {
    void SetUp() override {
        createAubCSR = GetParam();
        csrWithAubDump = new MyMockCsrWithAubDump<MyMockCsr>(DEFAULT_TEST_PLATFORM::hwInfo, createAubCSR, executionEnvironment);
        ASSERT_NE(nullptr, csrWithAubDump);

        memoryManager = csrWithAubDump->createMemoryManager(false, false);
        ASSERT_NE(nullptr, memoryManager);
    }

    void TearDown() override {
        delete csrWithAubDump;
        delete memoryManager;
    }
    ExecutionEnvironment executionEnvironment;
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

    OsContext osContext(nullptr, 0u);

    ResidencyContainer allocationsForResidency;
    FlushStamp flushStamp = csrWithAubDump->flush(batchBuffer, engineType, allocationsForResidency, osContext);
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
    OsContext osContext(nullptr, 0u);
    csrWithAubDump->processResidency(allocationsForResidency, osContext);

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
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
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
