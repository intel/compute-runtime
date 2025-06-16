/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_aub_center_fixture.h"
#include "shared/test/common/helpers/batch_buffer_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct MyMockCsr : UltCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> {
    MyMockCsr(ExecutionEnvironment &executionEnvironment,
              uint32_t rootDeviceIndex,
              const DeviceBitfield deviceBitfield)
        : UltCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushParametrization.wasCalled = true;
        flushParametrization.receivedBatchBuffer = &batchBuffer;
        flushParametrization.receivedEngine = osContext->getEngineType();
        flushParametrization.receivedAllocationsForResidency = &allocationsForResidency;
        processResidency(allocationsForResidency, 0u);
        flushStamp->setStamp(flushParametrization.flushStampToReturn);
        return SubmissionStatus::success;
    }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        makeResidentParameterization.wasCalled = true;
        makeResidentParameterization.receivedGfxAllocation = &gfxAllocation;
        gfxAllocation.updateResidencyTaskCount(1, osContext->getContextId());
    }

    SubmissionStatus processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) override {
        processResidencyParameterization.wasCalled = true;
        processResidencyParameterization.receivedAllocationsForResidency = &allocationsForResidency;
        return SubmissionStatus::success;
    }

    void makeNonResident(GraphicsAllocation &gfxAllocation) override {
        if (gfxAllocation.isResident(this->osContext->getContextId())) {
            makeNonResidentParameterization.wasCalled = true;
            makeNonResidentParameterization.receivedGfxAllocation = &gfxAllocation;
            gfxAllocation.releaseResidencyInOsContext(this->osContext->getContextId());
        }
    }

    AubSubCaptureStatus checkAndActivateAubSubCapture(const std::string &kernelName) override {
        checkAndActivateAubSubCaptureParameterization.wasCalled = true;
        checkAndActivateAubSubCaptureParameterization.kernelName = &kernelName;
        return {false, false};
    }

    bool expectMemory(const void *gfxAddress, const void *srcAddress,
                      size_t length, uint32_t compareOperation) override {
        expectMemoryParameterization.wasCalled = true;
        expectMemoryParameterization.gfxAddress = gfxAddress;
        expectMemoryParameterization.srcAddress = srcAddress;
        expectMemoryParameterization.length = length;
        expectMemoryParameterization.compareOperation = compareOperation;
        return true;
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
        const std::string *kernelName = nullptr;
    } checkAndActivateAubSubCaptureParameterization;

    struct ExpectMemoryParameterization {
        bool wasCalled = false;
        const void *gfxAddress = nullptr;
        const void *srcAddress = nullptr;
        size_t length = 0;
        uint32_t compareOperation = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;
    } expectMemoryParameterization;
};

template <typename BaseCSR>
struct MyMockCsrWithAubDump : CommandStreamReceiverWithAUBDump<BaseCSR> {
    MyMockCsrWithAubDump(bool createAubCSR,
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
        DeviceFixture::setUp();
        MockAubCenterFixture::setUp();
        setMockAubCenter(pDevice->getRootDeviceEnvironmentRef());
        executionEnvironment = pDevice->getExecutionEnvironment();
        executionEnvironment->initializeMemoryManager();
        memoryManager = executionEnvironment->memoryManager.get();
        ASSERT_NE(nullptr, memoryManager);
        createAubCSR = GetParam();
        DeviceBitfield deviceBitfield(1);
        csrWithAubDump = new MyMockCsrWithAubDump<MyMockCsr>(createAubCSR, *executionEnvironment, deviceBitfield);
        ASSERT_NE(nullptr, csrWithAubDump);

        auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(DEFAULT_TEST_PLATFORM::hwInfo), EngineUsage::regular},
                                                                             PreemptionHelper::getDefaultPreemptionMode(DEFAULT_TEST_PLATFORM::hwInfo));

        pDevice->getExecutionEnvironment()->memoryManager->reInitLatestContextId();
        auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csrWithAubDump, engineDescriptor);
        csrWithAubDump->setupContext(*osContext);
    }

    void TearDown() override {
        delete csrWithAubDump;
        MockAubCenterFixture::tearDown();
        DeviceFixture::tearDown();
    }

    ExecutionEnvironment *executionEnvironment;
    MyMockCsrWithAubDump<MyMockCsr> *csrWithAubDump;
    MemoryManager *memoryManager;
    bool createAubCSR;
};

struct CommandStreamReceiverWithAubDumpSimpleTest : Test<MockAubCenterFixture>, DeviceFixture {
    void SetUp() override {
        DeviceFixture::setUp();
        MockAubCenterFixture::setUp();
        setMockAubCenter(pDevice->getRootDeviceEnvironmentRef());
    }
    void TearDown() override {
        MockAubCenterFixture::tearDown();
        DeviceFixture::tearDown();
    }
};

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenSettingOsContextThenReplicateItToAubCsr) {
    ExecutionEnvironment *executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment, 0, deviceBitfield);

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                            PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));

    csrWithAubDump.setupContext(osContext);
    EXPECT_EQ(&osContext, &csrWithAubDump.getOsContext());
    EXPECT_EQ(&osContext, &csrWithAubDump.aubCSR->getOsContext());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenTbxCsrWithAubDumpIsCreatedThenAubCsrIsNotCreated) {
    ExecutionEnvironment *executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::tbxWithAub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment, 0, deviceBitfield);
    ASSERT_EQ(nullptr, csrWithAubDump.aubCSR);
    EXPECT_EQ(CommandStreamReceiverType::tbxWithAub, csrWithAubDump.getType());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubManagerAvailableWhenHwCsrWithAubDumpIsCreatedThenAubCsrIsCreated) {
    ExecutionEnvironment *executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();

    std::string fileName = "file_name.aub";
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, fileName, CommandStreamReceiverType::hardwareWithAub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("aubfile", *executionEnvironment, 0, deviceBitfield);
    ASSERT_NE(nullptr, csrWithAubDump.aubCSR);
    EXPECT_EQ(CommandStreamReceiverType::hardwareWithAub, csrWithAubDump.getType());
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenWaitingForTaskCountThenAddPollForCompletion) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();

    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, "file_name.aub", CommandStreamReceiverType::hardwareWithAub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(new MockAubManager());

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);
    csrWithAubDump.initializeTagAllocation();

    auto mockAubCsr = new MockAubCsr<FamilyType>("file_name.aub", false, *executionEnvironment, 0, deviceBitfield);
    mockAubCsr->initializeTagAllocation();
    csrWithAubDump.aubCSR.reset(mockAubCsr);

    EXPECT_FALSE(mockAubCsr->pollForCompletionCalled);
    csrWithAubDump.waitForTaskCountWithKmdNotifyFallback(1, 0, false, QueueThrottle::MEDIUM);
    EXPECT_TRUE(mockAubCsr->pollForCompletionCalled);

    csrWithAubDump.aubCSR.reset(nullptr);
    csrWithAubDump.waitForTaskCountWithKmdNotifyFallback(1, 0, false, QueueThrottle::MEDIUM);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, whenPollForAubCompletionCalledThenDontInsertPoll) {
    MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));

    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();

    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, "file_name.aub", CommandStreamReceiverType::hardwareWithAub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(new MockAubManager());

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);
    csrWithAubDump.initializeTagAllocation();

    auto mockAubCsr = new MockAubCsr<FamilyType>("file_name.aub", false, *executionEnvironment, 0, deviceBitfield);
    mockAubCsr->initializeTagAllocation();
    csrWithAubDump.aubCSR.reset(mockAubCsr);
    csrWithAubDump.setupContext(osContext);

    csrWithAubDump.pollForAubCompletion();
    EXPECT_FALSE(csrWithAubDump.pollForCompletionCalled);
    EXPECT_TRUE(mockAubCsr->pollForCompletionCalled);
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenPollForCompletionCalledThenAubCsrPollForCompletionCalled) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();

    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, "file_name.aub", CommandStreamReceiverType::hardwareWithAub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(new MockAubManager());

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    DeviceBitfield deviceBitfield(1);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);
    csrWithAubDump.initializeTagAllocation();

    csrWithAubDump.aubCSR.reset(nullptr);
    csrWithAubDump.pollForCompletion();

    auto mockAubCsr = new MockAubCsr<FamilyType>("file_name.aub", false, *executionEnvironment, 0, deviceBitfield);
    mockAubCsr->initializeTagAllocation();
    csrWithAubDump.aubCSR.reset(mockAubCsr);

    EXPECT_FALSE(mockAubCsr->pollForCompletionCalled);
    csrWithAubDump.pollForCompletion();
    EXPECT_TRUE(mockAubCsr->pollForCompletionCalled);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenExpectMemoryIsCalledThenBothCommandStreamReceiversAreCalled) {
    uint32_t compareOperation = AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual;
    uint8_t buffer[0x10000]{};
    size_t length = sizeof(buffer);

    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csrWithAubDump->getRootDeviceIndex(), false, length}, buffer);
    ASSERT_NE(nullptr, gfxAllocation);

    csrWithAubDump->makeResidentHostPtrAllocation(gfxAllocation);
    csrWithAubDump->expectMemory(reinterpret_cast<void *>(gfxAllocation->getGpuAddress()), buffer, length, compareOperation);

    EXPECT_TRUE(csrWithAubDump->expectMemoryParameterization.wasCalled);
    EXPECT_EQ(reinterpret_cast<void *>(gfxAllocation->getGpuAddress()), csrWithAubDump->expectMemoryParameterization.gfxAddress);
    EXPECT_EQ(buffer, csrWithAubDump->expectMemoryParameterization.srcAddress);
    EXPECT_EQ(length, csrWithAubDump->expectMemoryParameterization.length);
    EXPECT_EQ(compareOperation, csrWithAubDump->expectMemoryParameterization.compareOperation);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().expectMemoryParameterization.wasCalled);
        EXPECT_EQ(reinterpret_cast<void *>(gfxAllocation->getGpuAddress()), csrWithAubDump->getAubMockCsr().expectMemoryParameterization.gfxAddress);
        EXPECT_EQ(buffer, csrWithAubDump->getAubMockCsr().expectMemoryParameterization.srcAddress);
        EXPECT_EQ(length, csrWithAubDump->getAubMockCsr().expectMemoryParameterization.length);
        EXPECT_EQ(compareOperation, csrWithAubDump->getAubMockCsr().expectMemoryParameterization.compareOperation);
    }

    memoryManager->freeGraphicsMemoryImpl(gfxAllocation);
}

HWTEST_P(CommandStreamReceiverWithAubDumpTest, givenCommandStreamReceiverWithAubDumpWhenWriteMemoryIsCalledThenBothCommandStreamReceiversAreCalled) {
    MockGraphicsAllocation mockAllocation;

    EXPECT_EQ(0u, csrWithAubDump->writeMemoryParams.totalCallCount);
    if (createAubCSR) {
        EXPECT_EQ(0u, csrWithAubDump->getAubMockCsr().writeMemoryParams.totalCallCount);
    }

    csrWithAubDump->writeMemory(mockAllocation, false, 0, 0);

    EXPECT_EQ(1u, csrWithAubDump->writeMemoryParams.totalCallCount);
    EXPECT_EQ(&mockAllocation, csrWithAubDump->writeMemoryParams.latestGfxAllocation);
    EXPECT_FALSE(csrWithAubDump->writeMemoryParams.latestChunkedMode);

    if (createAubCSR) {
        EXPECT_EQ(1u, csrWithAubDump->getAubMockCsr().writeMemoryParams.totalCallCount);
        EXPECT_EQ(&mockAllocation, csrWithAubDump->getAubMockCsr().writeMemoryParams.latestGfxAllocation);
        EXPECT_FALSE(csrWithAubDump->getAubMockCsr().writeMemoryParams.latestChunkedMode);
    }

    csrWithAubDump->writeMemory(mockAllocation, true, 1, 2);

    EXPECT_EQ(2u, csrWithAubDump->writeMemoryParams.totalCallCount);
    EXPECT_TRUE(csrWithAubDump->writeMemoryParams.latestChunkedMode);
    EXPECT_EQ(&mockAllocation, csrWithAubDump->writeMemoryParams.latestGfxAllocation);
    EXPECT_EQ(1u, csrWithAubDump->writeMemoryParams.latestGpuVaChunkOffset);
    EXPECT_EQ(2u, csrWithAubDump->writeMemoryParams.latestChunkSize);

    if (createAubCSR) {
        EXPECT_EQ(2u, csrWithAubDump->getAubMockCsr().writeMemoryParams.totalCallCount);
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().writeMemoryParams.latestChunkedMode);
        EXPECT_EQ(&mockAllocation, csrWithAubDump->getAubMockCsr().writeMemoryParams.latestGfxAllocation);
        EXPECT_EQ(1u, csrWithAubDump->getAubMockCsr().writeMemoryParams.latestGpuVaChunkOffset);
        EXPECT_EQ(2u, csrWithAubDump->getAubMockCsr().writeMemoryParams.latestChunkSize);
    }
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenCsrWithAubDumpWhenCreatingAubCsrThenInitializeTagAllocation) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();

    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, "file_name.aub", CommandStreamReceiverType::hardwareWithAub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(new MockAubManager());

    executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);

    uint32_t subDevicesCount = 4;

    DeviceBitfield deviceBitfield = maxNBitValue(subDevicesCount);
    CommandStreamReceiverWithAUBDump<UltCommandStreamReceiver<FamilyType>> csrWithAubDump("file_name.aub", *executionEnvironment, 0, deviceBitfield);

    EXPECT_NE(nullptr, csrWithAubDump.aubCSR->getTagAllocation());
    EXPECT_NE(nullptr, csrWithAubDump.aubCSR->getTagAddress());

    auto tagAddressToInitialize = csrWithAubDump.aubCSR->getTagAddress();

    for (uint32_t i = 0; i < subDevicesCount; i++) {
        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), *tagAddressToInitialize);
        tagAddressToInitialize = ptrOffset(tagAddressToInitialize, csrWithAubDump.aubCSR->getImmWritePostSyncWriteOffset());
    }
}

HWTEST_F(CommandStreamReceiverWithAubDumpSimpleTest, givenAubCsrWithHwWhenAddingCommentThenAddCommentToAubManager) {
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();

    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, "file_name.aub", CommandStreamReceiverType::hardwareWithAub);
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

    MockAubCenter *mockAubCenter = new MockAubCenter(pDevice->getRootDeviceEnvironment(), false, "file_name.aub", CommandStreamReceiverType::tbxWithAub);
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
        auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        MockOsContext osContext(0, EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment->rootDeviceEnvironments[0])[0],
                                                                                PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
        csr.setupContext(osContext);

        auto allocator = csr.getTimestampPacketAllocator();
        auto tag = allocator->getTag();

        typename FamilyType::TimestampPacketType zeros[4] = {};

        for (uint32_t i = 0; i < FamilyType::timestampPacketCount; i++) {
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

        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->initializeMemoryManager();

        MockAubManager *mockManager = new MockAubManager();
        MockAubCenter *mockAubCenter = new MockAubCenter(*executionEnvironment->rootDeviceEnvironments[0], false, fileName, CommandStreamReceiverType::hardwareWithAub);
        mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);

        executionEnvironment->rootDeviceEnvironments[0]->aubCenter = std::unique_ptr<MockAubCenter>(mockAubCenter);
    }

    const std::string fileName = "file_name.aub";
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
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
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());

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
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());

    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[expectedRootDeviceIndex]->aubCenter.get());
    EXPECT_FALSE(rootDeviceEnvironment->initAubCenterCalled);
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>>>("", executionEnvironment, expectedRootDeviceIndex, deviceBitfield);
    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_NE(nullptr, rootDeviceEnvironment->aubCenter.get());
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

    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());
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
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

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
    BatchBuffer batchBuffer = BatchBufferHelper::createDefaultBatchBuffer(cs.getGraphicsAllocation(), &cs, cs.getUsed());

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
        EXPECT_EQ(csrWithAubDump->peekLatestSentTaskCount(), csrWithAubDump->getAubMockCsr().peekLatestFlushedTaskCount());
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
    std::string kernelName = "";
    csrWithAubDump->checkAndActivateAubSubCapture(kernelName);

    EXPECT_TRUE(csrWithAubDump->checkAndActivateAubSubCaptureParameterization.wasCalled);
    EXPECT_EQ(&kernelName, csrWithAubDump->checkAndActivateAubSubCaptureParameterization.kernelName);

    if (createAubCSR) {
        EXPECT_TRUE(csrWithAubDump->getAubMockCsr().checkAndActivateAubSubCaptureParameterization.wasCalled);
        EXPECT_EQ(&kernelName, csrWithAubDump->getAubMockCsr().checkAndActivateAubSubCaptureParameterization.kernelName);
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

INSTANTIATE_TEST_SUITE_P(
    CommandStreamReceiverWithAubDumpTest_Create,
    CommandStreamReceiverWithAubDumpTest,
    testing::ValuesIn(createAubCSR));
