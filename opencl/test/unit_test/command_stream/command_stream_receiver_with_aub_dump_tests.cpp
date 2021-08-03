/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/unit_test/fixtures/mock_aub_center_fixture.h"

#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_aub_csr.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "test.h"

using namespace NEO;

struct MyMockCsr : UltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> {
    MyMockCsr(ExecutionEnvironment &executionEnvironment,
              uint32_t rootDeviceIndex,
              const DeviceBitfield deviceBitfield)
        : UltCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushParametrization.wasCalled = true;
        flushParametrization.receivedBatchBuffer = &batchBuffer;
        flushParametrization.receivedEngine = osContext->getEngineType();
        flushParametrization.receivedAllocationsForResidency = &allocationsForResidency;
        processResidency(allocationsForResidency, 0u);
        flushStamp->setStamp(flushParametrization.flushStampToReturn);
        return true;
    }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        makeResidentParameterization.wasCalled = true;
        makeResidentParameterization.receivedGfxAllocation = &gfxAllocation;
        gfxAllocation.updateResidencyTaskCount(1, osContext->getContextId());
    }

    void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) override {
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
        const ResidencyContainer *receivedAllocationsForResidency = nullptr;
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
    MyMockCsrWithAubDump<BaseCSR>(bool createAubCSR,
                                  ExecutionEnvironment &executionEnvironment,
                                  const DeviceBitfield deviceBitfield)
        : CommandStreamReceiverWithAUBDump<BaseCSR>("aubfile", executionEnvironment, 0, deviceBitfield) {
        this->aubCSR.reset(createAubCSR ? new MyMockCsr(executionEnvironment, 0, deviceBitfield) : nullptr);
    }

    MyMockCsr &getAubMockCsr() const {
        return static_cast<MyMockCsr &>(*this->aubCSR);
    }
};

struct CommandStreamReceiverWithAubDumpTest : public ::testing::TestWithParam<bool /*createAubCSR*/>, MockAubCenterFixture, DeviceFixture {
    void SetUp() override {
        DeviceFixture::SetUp();
        MockAubCenterFixture::SetUp();
        setMockAubCenter(pDevice->getRootDeviceEnvironmentRef());
        executionEnvironment = pDevice->getExecutionEnvironment();
        executionEnvironment->initializeMemoryManager();
        memoryManager = executionEnvironment->memoryManager.get();
        ASSERT_NE(nullptr, memoryManager);
        createAubCSR = GetParam();
        DeviceBitfield deviceBitfield(1);
        csrWithAubDump = new MyMockCsrWithAubDump<MyMockCsr>(createAubCSR, *executionEnvironment, deviceBitfield);
        ASSERT_NE(nullptr, csrWithAubDump);

        auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csrWithAubDump,
                                                                                         EngineTypeUsage{getChosenEngineType(DEFAULT_TEST_PLATFORM::hwInfo), EngineUsage::Regular},
                                                                                         deviceBitfield,
                                                                                         PreemptionHelper::getDefaultPreemptionMode(DEFAULT_TEST_PLATFORM::hwInfo),
                                                                                         false);
        csrWithAubDump->setupContext(*osContext);
    }

    void TearDown() override {
        delete csrWithAubDump;
        MockAubCenterFixture::TearDown();
        DeviceFixture::TearDown();
    }

    ExecutionEnvironment *executionEnvironment;
    MyMockCsrWithAubDump<MyMockCsr> *csrWithAubDump;
    MemoryManager *memoryManager;
    bool createAubCSR;
};

struct CommandStreamReceiverWithAubDumpSimpleTest : Test<MockAubCenterFixture>, DeviceFixture {
    void SetUp() override {
        DeviceFixture::SetUp();
        MockAubCenterFixture::SetUp();
        setMockAubCenter(pDevice->getRootDeviceEnvironmentRef());
    }
    void TearDown() override {
        MockAubCenterFixture::TearDown();
        DeviceFixture::TearDown();
    }
};

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenSettingOsContextThenReplicateItToAubCsr) {
    ExecutionEnvironment *executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment, 0, deviceBitfield);

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    MockOsContext osContext(0, 1, HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                            PreemptionHelper::getDefaultPreemptionMode(*hwInfo), false);

    csrWithAubDump.setupContext(osContext);
    EXPECT_EQ(&osContext, &csrWithAubDump.getOsContext());
    EXPECT_EQ(&osContext, &csrWithAubDump.aubCSR->getOsContext());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenTbxCsrWithAubDumpIsCreatedThenAubCsrIsNotCreated) {
    ExecutionEnvironment *executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, fileName, CommandStreamReceiverType::CSR_TBX_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment, 0, deviceBitfield);
    ASSERT_EQ(nullptr, csrWithAubDump.aubCSR);
    EXPECT_EQ(CommandStreamReceiverType::CSR_TBX_WITH_AUB, csrWithAubDump.getType());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenHwCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    ExecutionEnvironment *executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, fileName, CommandStreamReceiverType::CSR_HW_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment, 0, deviceBitfield);
    ASSERT_NE(nullptr, csrWithAubDump.aubCSR);
    EXPECT_EQ(CommandStreamReceiverType::CSR_HW_WITH_AUB, csrWithAubDump.getType());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenWaitingForTaskCountThenAddPollForCompletion) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();

    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();
    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, "file_name.aub", CommandStreamReceiverType::CSR_HW_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(new MockAubManager());

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);
    csrWithAubDump.initializeTagAllocation();

    auto mockAubCsr = new MockAubCsr<FamilyType>("file_name.aub", false, *executionEnvironment, 0, deviceBitfield);
    mockAubCsr->initializeTagAllocation();
    csrWithAubDump.aubCSR.reset(mockAubCsr);

    EXPECT_FALSE(mockAubCsr->pollForCompletionCalled);
    csrWithAubDump.waitForTaskCountWithKmdNotifyFallback(1, 0, false, false);
    EXPECT_TRUE(mockAubCsr->pollForCompletionCalled);

    csrWithAubDump.aubCSR.reset(nullptr);
    csrWithAubDump.waitForTaskCountWithKmdNotifyFallback(1, 0, false, false);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenCreatingAubCsrThenInitializeTagAllocation) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, "file_name.aub", CommandStreamReceiverType::CSR_HW_WITH_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(new MockAubManager());

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);

    EXPECT_NE(nullptr, csrWithAubDump.aubCSR->getTagAllocation());
    EXPECT_NE(nullptr, csrWithAubDump.aubCSR->getTagAddress());
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), *csrWithAubDump.aubCSR->getTagAddress());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubCsrWithHwWhenAddingCommentThenAddCommentToAubManager) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, "file_name.aub", CommandStreamReceiverType::CSR_HW_WITH_AUB);
    auto mockAubManager = new MockAubManager();
    mockAubCenter->aubManager.reset(mockAubManager);

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    EXPECT_FALSE(mockAubManager->addCommentCalled);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);
    csrWithAubDump.addAubComment("test");
    EXPECT_TRUE(mockAubManager->addCommentCalled);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubCsrWithTbxWhenAddingCommentThenDontAddCommentToAubManager) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, "file_name.aub", CommandStreamReceiverType::CSR_TBX_WITH_AUB);
    auto mockAubManager = new MockAubManager();
    mockAubCenter->aubManager.reset(mockAubManager);

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);
    csrWithAubDump.addAubComment("test");
    EXPECT_FALSE(mockAubManager->addCommentCalled);
}

struct CommandStreamReceiverTagTests : public ::testing::Test {
    template <typename FamilyType>
    using AubWithHw = CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>>;

    template <typename FamilyType>
    using AubWithTbx = CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>>;

    template <typename CsrT, typename FamilyType, typename... Args>
    bool isTimestampPacketNodeReleasable(Args &&...args) {
        CsrT csr(std::forward<Args>(args)...);
        auto hwInfo = csr.peekExecutionEnvironment().rootDeviceEnvironments[0]->getHardwareInfo();
        MockOsContext osContext(0, 1, HwHelper::get(hwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0],
                                PreemptionHelper::getDefaultPreemptionMode(*hwInfo), false);
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();

        typename FamilyType::TimestampPacketType zeros[4] = {};

        for (uint32_t i = 0; i < TimestampPacketSizeControl::preferredPacketCount; i++) {
            tag->assignDataToAllTimestamps(i, zeros);
        }

        bool canBeReleased = tag->canBeReleased();
        allocator->returnTag(tag);

        return canBeReleased;
    };

    template <typename CsrT, typename... Args>
    size_t getPreferredTagPoolSize(Args &&...args) {
        CsrT csr(std::forward<Args>(args)...);
        return csr.getPreferredTagPoolSize();
    };

    void SetUp() override {

        executionEnvironment = platform()->peekExecutionEnvironment();
        executionEnvironment->initializeMemoryManager();

        auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

        MockAubManager *mockManager = new MockAubManager();
        MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), *gmmHelper, false, fileName, CommandStreamReceiverType::CSR_HW_WITH_AUB);
        mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

        executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    }

    const std::string fileName = "file_name.aub";
    ExecutionEnvironment *executionEnvironment = nullptr;
};

HWTEST_F(CommandStreamReceiverTagTests, givenCsrTypeWhenCreatingTimestampPacketAllocatorThenSetDefaultCompletionCheckType) {
    bool result = isTimestampPacketNodeReleasable<CommandStreamReceiverHw<FamilyType>, FamilyType>(*executionEnvironment, 0, 1);
    EXPECT_TRUE(result);

    result = isTimestampPacketNodeReleasable<AUBCommandStreamReceiverHw<FamilyType>, FamilyType>(fileName, false, *executionEnvironment, 0, 1);
    EXPECT_FALSE(result);

    result = isTimestampPacketNodeReleasable<AubWithHw<FamilyType>, FamilyType>(fileName, *executionEnvironment, 0, 1);
    EXPECT_FALSE(result);

    result = isTimestampPacketNodeReleasable<AubWithTbx<FamilyType>, FamilyType>(fileName, *executionEnvironment, 0, 1);
    EXPECT_FALSE(result);
}

HWTEST_F(CommandStreamReceiverTagTests, givenCsrTypeWhenAskingForTagPoolSizeThenReturnOneForAubTbxMode) {
    EXPECT_EQ(2048u, getPreferredTagPoolSize<CommandStreamReceiverHw<FamilyType>>(*executionEnvironment, 0, 1));
    EXPECT_EQ(1u, getPreferredTagPoolSize<AUBCommandStreamReceiverHw<FamilyType>>(fileName, false, *executionEnvironment, 0, 1));
    EXPECT_EQ(1u, getPreferredTagPoolSize<AubWithHw<FamilyType>>(fileName, *executionEnvironment, 0, 1));
    EXPECT_EQ(1u, getPreferredTagPoolSize<AubWithTbx<FamilyType>>(fileName, *executionEnvironment, 0, 1));
}

using SimulatedCsrTest = ::testing::Test;
HWTEST_F(SimulatedCsrTest, givenHwWithAubDumpCsrTypeWhenCreateCommandStreamReceiverThenProperAubCenterIsInitialized) {
    uint32_t expectedRootDeviceIndex = 10;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, expectedRootDeviceIndex + 2);
    executionEnvironment.initializeMemoryManager();

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex].get());
    rootDeviceEnvironment->setHwInfo(defaultHwInfo.get());

    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>>>("", executionEnvironment, expectedRootDeviceIndex, deviceBitfield);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_NE(nullptr, rootDeviceEnvironment->aubCenter.get());
}
HWTEST_F(SimulatedCsrTest, givenTbxWithAubDumpCsrTypeWhenCreateCommandStreamReceiverThenProperAubCenterIsInitialized) {
    uint32_t expectedRootDeviceIndex = 10;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, expectedRootDeviceIndex + 2);
    executionEnvironment.initializeMemoryManager();

    auto rootDeviceEnvironment = new MockRootDeviceEnvironment(executionEnvironment);
    executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex].reset(rootDeviceEnvironment);
    rootDeviceEnvironment->setHwInfo(defaultHwInfo.get());

    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>>>("", executionEnvironment, expectedRootDeviceIndex, deviceBitfield);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_NE(nullptr, rootDeviceEnvironment->aubCenter.get());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenNullAubManagerAvailableWhenTbxCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    MockAubCenter *mockAubCenter = new MockAubCenter();
    ExecutionEnvironment *executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment, 0, deviceBitfield);
    EXPECT_NE(nullptr, csrWithAubDump.aubCSR);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerNotAvailableWhenHwCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    std::string fileName = "file_name.aub";

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", executionEnvironment, 0, deviceBitfield);
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
    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csrWithAubDump->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);

    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};
    auto engineType = csrWithAubDump->getOsContext().getEngineType();

    ResidencyContainer allocationsForResidency;
    csrWithAubDump->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(csrWithAubDump->obtainCurrentFlushStamp(), csrWithAubDump->flushParametrization.flushStampToReturn);

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
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csrWithAubDump->getRootDeviceIndex(), MemoryConstants::pageSize});
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
    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csrWithAubDump->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csrWithAubDump->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, gfxAllocation);
    ResidencyContainer allocationsForResidency = {gfxAllocation};

    csrWithAubDump->flush(batchBuffer, allocationsForResidency);
    EXPECT_EQ(csrWithAubDump->obtainCurrentFlushStamp(), csrWithAubDump->flushParametrization.flushStampToReturn);

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
    GraphicsAllocation *commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csrWithAubDump->getRootDeviceIndex(), MemoryConstants::pageSize});
    ASSERT_NE(nullptr, commandBuffer);
    LinearStream cs(commandBuffer);
    BatchBuffer batchBuffer{cs.getGraphicsAllocation(), 0, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount, cs.getUsed(), &cs, nullptr, false};

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
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csrWithAubDump->getRootDeviceIndex(), MemoryConstants::pageSize});
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
