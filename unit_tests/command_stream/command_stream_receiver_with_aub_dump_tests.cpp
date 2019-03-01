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
#include "test.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_aub_center.h"
#include "unit_tests/mocks/mock_aub_manager.h"
#include "unit_tests/mocks/mock_os_context.h"

using namespace OCLRT;

struct MyMockCsr : UltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> {
    MyMockCsr(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment)
        : UltCommandStreamReceiver(hwInfoIn, executionEnvironment) {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushParametrization.wasCalled = true;
        flushParametrization.receivedBatchBuffer = &batchBuffer;
        flushParametrization.receivedEngine = osContext->getEngineType().type;
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
    MyMockCsrWithAubDump<BaseCSR>(const HardwareInfo &hwInfoIn, bool createAubCSR, ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverWithAUBDump<BaseCSR>(hwInfoIn, "aubfile", executionEnvironment) {
        this->aubCSR.reset(createAubCSR ? new MyMockCsr(hwInfoIn, executionEnvironment) : nullptr);
    }

    MyMockCsr &getAubMockCsr() const {
        return static_cast<MyMockCsr &>(*this->aubCSR);
    }
};

struct CommandStreamReceiverWithAubDumpTest : public ::testing::TestWithParam<bool /*createAubCSR*/> {
    void SetUp() override {
        createAubCSR = GetParam();
        csrWithAubDump = new MyMockCsrWithAubDump<MyMockCsr>(DEFAULT_TEST_PLATFORM::hwInfo, createAubCSR, executionEnvironment);
        ASSERT_NE(nullptr, csrWithAubDump);
        memoryManager = csrWithAubDump->createMemoryManager(false, false);
        executionEnvironment.memoryManager.reset(memoryManager);
        ASSERT_NE(nullptr, memoryManager);

        auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csrWithAubDump,
                                                                                        getChosenEngineType(DEFAULT_TEST_PLATFORM::hwInfo),
                                                                                        1, PreemptionHelper::getDefaultPreemptionMode(DEFAULT_TEST_PLATFORM::hwInfo));
        csrWithAubDump->setupContext(*osContext);
    }

    void TearDown() override {
        delete csrWithAubDump;
    }
    ExecutionEnvironment executionEnvironment;
    MyMockCsrWithAubDump<MyMockCsr> *csrWithAubDump;
    MemoryManager *memoryManager;
    bool createAubCSR;
};

using CommandStreamReceiverWithAubDumpSimpleTest = ::testing::Test;

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenSettingOsContextThenReplicateItToAubCsr) {
    ExecutionEnvironment executionEnvironment;

    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump(*platformDevices[0], "aubfile", executionEnvironment);
    MockOsContext osContext(nullptr, 0, 1, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));

    csrWithAubDump.setupContext(osContext);
    EXPECT_EQ(&osContext, &csrWithAubDump.getOsContext());
    EXPECT_EQ(&osContext, &csrWithAubDump.aubCSR->getOsContext());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenTbxCsrWithAubDumpIsCreatedThenAubCsrIsNotCreated) {
    HardwareInfo hwInfo = *platformDevices[0];
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(&hwInfo, false, fileName, CommandStreamReceiverType::CSR_TBX_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump(*platformDevices[0], "aubfile", executionEnvironment);
    ASSERT_EQ(nullptr, csrWithAubDump.aubCSR);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenHwCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    HardwareInfo hwInfo = *platformDevices[0];
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(&hwInfo, false, fileName, CommandStreamReceiverType::CSR_HW_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump(*platformDevices[0], "aubfile", executionEnvironment);
    ASSERT_NE(nullptr, csrWithAubDump.aubCSR);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenNullAubManagerAvailableWhenTbxCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    MockAubCenter *mockAubCenter = new MockAubCenter();
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump(*platformDevices[0], "aubfile", executionEnvironment);
    EXPECT_NE(nullptr, csrWithAubDump.aubCSR);
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
    auto engineType = csrWithAubDump->getOsContext().getEngineType().type;

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

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenMakeNonResidentIsCalledThenBothBaseAndAubCsrMakeNonResidentIsCalled) {
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);
    csrWithAubDump->makeResident(*gfxAllocation);

    csrWithAubDump->makeNonResident(*gfxAllocation);

    EXPECT_TRUE(csrWithAubDump->makeNonResidentParameterization.wasCalled);
    EXPECT_EQ(gfxAllocation, csrWithAubDump->makeNonResidentParameterization.receivedGfxAllocation);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().makeNonResidentParameterization.wasCalled);
        EXPECT_EQ(gfxAllocation, csrWithAubDump->getAubMockCsr().makeNonResidentParameterization.receivedGfxAllocation);
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
