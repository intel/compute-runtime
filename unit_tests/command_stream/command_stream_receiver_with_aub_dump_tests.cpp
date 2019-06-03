/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "runtime/command_stream/preemption.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/mock_aub_center_fixture.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_os_context.h"

using namespace NEO;

struct MyMockCsr : UltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> {
    MyMockCsr(ExecutionEnvironment &executionEnvironment)
        : UltCommandStreamReceiver(executionEnvironment) {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushParametrization.wasCalled = true;
        flushParametrization.receivedBatchBuffer = &batchBuffer;
        flushParametrization.receivedEngine = osContext->getEngineType();
        flushParametrization.receivedAllocationsForResidency = &allocationsForResidency;
        processResidency(allocationsForResidency);
        return flushParametrization.flushStampToReturn;
    }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        makeResidentParameterization.wasCalled = true;
        makeResidentParameterization.receivedGfxAllocation = &gfxAllocation;
        gfxAllocation.updateResidencyTaskCount(1, osContext->getContextId());
    }

    void processResidency(ResidencyContainer &allocationsForResidency) override {
        processResidencyParameterization.wasCalled = true;
        processResidencyParameterization.receivedAllocationsForResidency = &allocationsForResidency;
    }

    void makeNonResident(GraphicsAllocation &gfxAllocation) override {
        if (gfxAllocation.isResident(this->osContext->getContextId())) {
            makeNonResidentParameterization.wasCalled = true;
            makeNonResidentParameterization.receivedGfxAllocation = &gfxAllocation;
            gfxAllocation.releaseResidencyInOsContext(this->osContext->getContextId());
        }
    }

    AubSubCaptureStatus checkAndActivateAubSubCapture(const MultiDispatchInfo &dispatchInfo) override {
        checkAndActivateAubSubCaptureParameterization.wasCalled = true;
        checkAndActivateAubSubCaptureParameterization.receivedDispatchInfo = &dispatchInfo;
        return {false, false};
    }

    struct FlushParameterization {
        bool wasCalled = false;
        FlushStamp flushStampToReturn = 1;
        BatchBuffer *receivedBatchBuffer = nullptr;
        aub_stream::EngineType receivedEngine = aub_stream::ENGINE_RCS;
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

    struct CheckAndActivateAubSubCaptureParameterization {
        bool wasCalled = false;
        const MultiDispatchInfo *receivedDispatchInfo = nullptr;
    } checkAndActivateAubSubCaptureParameterization;
};

template <typename BaseCSR>
struct MyMockCsrWithAubDump : CommandStreamReceiverWithAUBDump<BaseCSR> {
    MyMockCsrWithAubDump<BaseCSR>(bool createAubCSR, ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverWithAUBDump<BaseCSR>("aubfile", executionEnvironment) {
        this->aubCSR.reset(createAubCSR ? new MyMockCsr(executionEnvironment) : nullptr);
    }

    MyMockCsr &getAubMockCsr() const {
        return static_cast<MyMockCsr &>(*this->aubCSR);
    }
};

struct CommandStreamReceiverWithAubDumpTest : public ::testing::TestWithParam<bool /*createAubCSR*/>, MockAubCenterFixture {
    void SetUp() override {
        MockAubCenterFixture::SetUp();
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        createAubCSR = GetParam();
        csrWithAubDump = new MyMockCsrWithAubDump<MyMockCsr>(createAubCSR, *executionEnvironment);
        ASSERT_NE(nullptr, csrWithAubDump);
        executionEnvironment->initializeMemoryManager();
        memoryManager = executionEnvironment->memoryManager.get();
        ASSERT_NE(nullptr, memoryManager);

        auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csrWithAubDump,
                                                                                         getChosenEngineType(DEFAULT_TEST_PLATFORM::hwInfo), 1,
                                                                                         PreemptionHelper::getDefaultPreemptionMode(DEFAULT_TEST_PLATFORM::hwInfo), false);
        csrWithAubDump->setupContext(*osContext);
    }

    void TearDown() override {
        MockAubCenterFixture::TearDown();
        delete csrWithAubDump;
    }
    ExecutionEnvironment *executionEnvironment;
    MyMockCsrWithAubDump<MyMockCsr> *csrWithAubDump;
    MemoryManager *memoryManager;
    bool createAubCSR;
};

using CommandStreamReceiverWithAubDumpSimpleTest = Test<MockAubCenterFixture>;

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenSettingOsContextThenReplicateItToAubCsr) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();

    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment);
    MockOsContext osContext(0, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0],
                            PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);

    csrWithAubDump.setupContext(osContext);
    EXPECT_EQ(&osContext, &csrWithAubDump.getOsContext());
    EXPECT_EQ(&osContext, &csrWithAubDump.aubCSR->getOsContext());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenTbxCsrWithAubDumpIsCreatedThenAubCsrIsNotCreated) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(*platformDevices, false, fileName, CommandStreamReceiverType::CSR_TBX_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment);
    ASSERT_EQ(nullptr, csrWithAubDump.aubCSR);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenHwCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(*platformDevices, false, fileName, CommandStreamReceiverType::CSR_HW_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment);
    ASSERT_NE(nullptr, csrWithAubDump.aubCSR);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenNullAubManagerAvailableWhenTbxCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    MockAubCenter *mockAubCenter = new MockAubCenter();
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment);
    EXPECT_NE(nullptr, csrWithAubDump.aubCSR);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerNotAvailableWhenHwCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    std::string fileName = "file_name.aub";

    MockExecutionEnvironment executionEnvironment(platformDevices[0]);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", executionEnvironment);
    ASSERT_NE(nullptr, csrWithAubDump.aubCSR);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenCtorIsCalledThenAubCsrIsInitialized) {
    if (createAubCSR) {
        EXPECT_NE(nullptr, csrWithAubDump->aubCSR);
    } else {
        EXPECT_EQ(nullptr, csrWithAubDump->aubCSR);
    }
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenFlushIsCalledThenBaseCsrFlushStampIsReturned) {
    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};
    auto engineType = csrWithAubDump->getOsContext().getEngineType();

    ResidencyContainer allocationsForResidency;
    FlushStamp flushStamp = csrWithAubDump->flush(batchBuffer, allocationsForResidency);
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
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenFlushIsCalledThenBothBaseAndAubCsrProcessResidencyIsCalled) {
    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    FlushStamp flushStamp = csrWithAubDump->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(flushStamp, csrWithAubDump->flushParametrization.flushStampToReturn);

    EXPECT_TRUE(csrWithAubDump->processResidencyParameterization.wasCalled);
    EXPECT_EQ(&allocationsForResidency, csrWithAubDump->processResidencyParameterization.receivedAllocationsForResidency);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().processResidencyParameterization.wasCalled);
        EXPECT_EQ(&allocationsForResidency, csrWithAubDump->getAubMockCsr().processResidencyParameterization.receivedAllocationsForResidency);
    }

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenFlushIsCalledThenLatestSentTaskCountShouldBeUpdatedForAubCsr) {
    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, cs.getUsed(), &cs};

    ResidencyContainer allocationsForResidency;

    EXPECT_EQ(0u, csrWithAubDump->peekLatestSentTaskCount());
    if (createAubCSR) {
        EXPECT_EQ(0u, csrWithAubDump->getAubMockCsr().peekLatestSentTaskCount());
    }

    csrWithAubDump->setLatestSentTaskCount(1u);
    csrWithAubDump->flush(batchBuffer, allocationsForResidency);

    EXPECT_EQ(1u, csrWithAubDump->peekLatestSentTaskCount());
    if (createAubCSR) {
        EXPECT_EQ(csrWithAubDump->peekLatestSentTaskCount(), csrWithAubDump->getAubMockCsr().peekLatestSentTaskCount());
    }

    memoryManager->freeGraphicsMemoryImpl(commandBuffer);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenMakeNonResidentIsCalledThenBothBaseAndAubCsrMakeNonResidentIsCalled) {
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);
    csrWithAubDump->makeResident(*gfxAllocation);

    csrWithAubDump->makeNonResident(*gfxAllocation);

    EXPECT_TRUE(csrWithAubDump->makeNonResidentParameterization.wasCalled);
    EXPECT_EQ(gfxAllocation, csrWithAubDump->makeNonResidentParameterization.receivedGfxAllocation);
    EXPECT_FALSE(gfxAllocation->isResident(csrWithAubDump->getOsContext().getContextId()));

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().makeNonResidentParameterization.wasCalled);
        EXPECT_EQ(gfxAllocation, csrWithAubDump->getAubMockCsr().makeNonResidentParameterization.receivedGfxAllocation);
    }

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenCheckAndActivateAubSubCaptureIsCalledThenBaseCsrCommandStreamReceiverIsCalled) {
    const DispatchInfo dispatchInfo;
    MultiDispatchInfo multiDispatchInfo;
    multiDispatchInfo.push(dispatchInfo);
    csrWithAubDump->checkAndActivateAubSubCapture(multiDispatchInfo);

    EXPECT_TRUE(csrWithAubDump->checkAndActivateAubSubCaptureParameterization.wasCalled);
    EXPECT_EQ(&multiDispatchInfo, csrWithAubDump->checkAndActivateAubSubCaptureParameterization.receivedDispatchInfo);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().checkAndActivateAubSubCaptureParameterization.wasCalled);
        EXPECT_EQ(&multiDispatchInfo, csrWithAubDump->getAubMockCsr().checkAndActivateAubSubCaptureParameterization.receivedDispatchInfo);
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
