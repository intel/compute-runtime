/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/memory_manager/surface.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/matchers.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_builtins.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_program.h"

#include "gmock/gmock.h"

using namespace OCLRT;

struct CommandStreamReceiverTest : public DeviceFixture,
                                   public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();

        commandStreamReceiver = &pDevice->getCommandStreamReceiver();
        ASSERT_NE(nullptr, commandStreamReceiver);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    CommandStreamReceiver *commandStreamReceiver;
};

HWTEST_F(CommandStreamReceiverTest, testCtor) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(0u, csr.peekTaskLevel());
    EXPECT_EQ(0u, csr.peekTaskCount());
    EXPECT_FALSE(csr.isPreambleSent);
}

HWTEST_F(CommandStreamReceiverTest, testInitProgrammingFlags) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.initProgrammingFlags();
    EXPECT_FALSE(csr.isPreambleSent);
    EXPECT_FALSE(csr.GSBAFor32BitProgrammed);
    EXPECT_TRUE(csr.mediaVfeStateDirty);
    EXPECT_FALSE(csr.lastVmeSubslicesConfig);
    EXPECT_EQ(0u, csr.lastSentL3Config);
    EXPECT_EQ(-1, csr.lastSentCoherencyRequest);
    EXPECT_EQ(-1, csr.lastMediaSamplerConfig);
    EXPECT_EQ(PreemptionMode::Initial, csr.lastPreemptionMode);
    EXPECT_EQ(0u, csr.latestSentStatelessMocsConfig);
}

TEST_F(CommandStreamReceiverTest, makeResident_setsBufferResidencyFlag) {
    MockContext context;
    float srcMemory[] = {1.0f};

    auto retVal = CL_INVALID_VALUE;
    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        sizeof(srcMemory),
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, buffer);
    EXPECT_FALSE(buffer->getGraphicsAllocation()->isResident(0u));

    commandStreamReceiver->makeResident(*buffer->getGraphicsAllocation());

    EXPECT_TRUE(buffer->getGraphicsAllocation()->isResident(0u));

    delete buffer;
}

TEST_F(CommandStreamReceiverTest, commandStreamReceiverFromDeviceHasATagValue) {
    EXPECT_NE(nullptr, const_cast<uint32_t *>(commandStreamReceiver->getTagAddress()));
}

TEST_F(CommandStreamReceiverTest, GetCommandStreamReturnsValidObject) {
    auto &cs = commandStreamReceiver->getCS();
    EXPECT_NE(nullptr, &cs);
}

TEST_F(CommandStreamReceiverTest, getCommandStreamContainsMemoryForRequest) {
    size_t requiredSize = 16384;
    const auto &commandStream = commandStreamReceiver->getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getAvailableSpace(), requiredSize);
}

TEST_F(CommandStreamReceiverTest, getCsReturnsCsWithCsOverfetchSizeIncludedInGraphicsAllocation) {
    size_t sizeRequested = 560;
    const auto &commandStream = commandStreamReceiver->getCS(sizeRequested);
    ASSERT_NE(nullptr, &commandStream);
    auto *allocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, allocation);

    size_t expectedTotalSize = alignUp(sizeRequested + MemoryConstants::cacheLineSize, MemoryConstants::pageSize) + CSRequirements::csOverfetchSize;

    EXPECT_LT(commandStream.getAvailableSpace(), expectedTotalSize);
    EXPECT_LE(commandStream.getAvailableSpace(), expectedTotalSize - CSRequirements::csOverfetchSize);
    EXPECT_EQ(expectedTotalSize, allocation->getUnderlyingBufferSize());
}

TEST_F(CommandStreamReceiverTest, getCommandStreamCanRecycle) {
    auto &commandStreamInitial = commandStreamReceiver->getCS();
    size_t requiredSize = commandStreamInitial.getMaxAvailableSpace() + 42;

    const auto &commandStream = commandStreamReceiver->getCS(requiredSize);
    ASSERT_NE(nullptr, &commandStream);
    EXPECT_GE(commandStream.getMaxAvailableSpace(), requiredSize);
}

TEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenGetCSIsCalledThenCommandStreamAllocationTypeShouldBeSetToLinearStream) {
    const auto &commandStream = commandStreamReceiver->getCS();
    auto commandStreamAllocation = commandStream.getGraphicsAllocation();
    ASSERT_NE(nullptr, commandStreamAllocation);

    EXPECT_EQ(GraphicsAllocation::AllocationType::LINEAR_STREAM, commandStreamAllocation->getAllocationType());
}

HWTEST_F(CommandStreamReceiverTest, givenPtrAndSizeThatMeetL3CriteriaWhenMakeResidentHostPtrThenCsrEnableL3) {
    void *hostPtr = reinterpret_cast<void *>(0xF000);
    auto size = 0x2000u;

    auto memoryManager = commandStreamReceiver->getMemoryManager();
    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemory(size, hostPtr);
    ASSERT_NE(nullptr, graphicsAllocation);
    commandStreamReceiver->makeResidentHostPtrAllocation(graphicsAllocation);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(csr.disableL3Cache);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenPtrAndSizeThatDoNotMeetL3CriteriaWhenMakeResidentHostPtrThenCsrDisableL3) {
    void *hostPtr = reinterpret_cast<void *>(0xF001);
    auto size = 0x2001u;

    auto memoryManager = commandStreamReceiver->getMemoryManager();
    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemory(size, hostPtr);
    ASSERT_NE(nullptr, graphicsAllocation);
    commandStreamReceiver->makeResidentHostPtrAllocation(graphicsAllocation);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_TRUE(csr.disableL3Cache);
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(CommandStreamReceiverTest, memoryManagerHasAccessToCSR) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();
    EXPECT_EQ(commandStreamReceiver, memoryManager->getCommandStreamReceiver(0));
}

HWTEST_F(CommandStreamReceiverTest, whenStoreAllocationThenStoredAllocationHasTaskCountFromCsr) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto *memoryManager = csr.getMemoryManager();
    void *host_ptr = (void *)0x1234;
    auto allocation = memoryManager->allocateGraphicsMemory(1, host_ptr);

    EXPECT_FALSE(allocation->isUsed());

    csr.taskCount = 2u;

    csr.getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(allocation), REUSABLE_ALLOCATION);

    EXPECT_EQ(csr.peekTaskCount(), allocation->getTaskCount(0));
}

HWTEST_F(CommandStreamReceiverTest, givenCommandStreamReceiverWhenCheckedForInitialStatusOfStatelessMocsIndexThenUnknownMocsIsReturend) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(CacheSettings::unknownMocs, csr.latestSentStatelessMocsConfig);
    EXPECT_FALSE(csr.disableL3Cache);
}

TEST_F(CommandStreamReceiverTest, makeResidentPushesAllocationToMemoryManagerResidencyList) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();

    GraphicsAllocation *graphicsAllocation = memoryManager->allocateGraphicsMemory(0x1000);

    ASSERT_NE(nullptr, graphicsAllocation);

    commandStreamReceiver->makeResident(*graphicsAllocation);

    auto &residencyAllocations = commandStreamReceiver->getResidencyAllocations();

    ASSERT_EQ(1u, residencyAllocations.size());
    EXPECT_EQ(graphicsAllocation, residencyAllocations[0]);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(CommandStreamReceiverTest, makeResidentWithoutParametersDoesNothing) {
    commandStreamReceiver->processResidency(commandStreamReceiver->getResidencyAllocations(), *pDevice->getOsContext());
    auto &residencyAllocations = commandStreamReceiver->getResidencyAllocations();
    EXPECT_EQ(0u, residencyAllocations.size());
}

TEST_F(CommandStreamReceiverTest, givenForced32BitAddressingWhenDebugSurfaceIsAllocatedThenRegularAllocationIsReturned) {
    auto *memoryManager = commandStreamReceiver->getMemoryManager();
    memoryManager->setForce32BitAllocations(true);
    auto allocation = commandStreamReceiver->allocateDebugSurface(1024);
    EXPECT_FALSE(allocation->is32BitAllocation);
}

HWTEST_F(CommandStreamReceiverTest, givenDefaultCommandStreamReceiverThenDefaultDispatchingPolicyIsImmediateSubmission) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(DispatchMode::ImmediateDispatch, csr.dispatchMode);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenGetIndirectHeapIsCalledThenHeapIsReturned) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 10u);
    EXPECT_NE(nullptr, heap.getGraphicsAllocation());
    EXPECT_NE(nullptr, csr.indirectHeap[IndirectHeap::DYNAMIC_STATE]);
    EXPECT_EQ(&heap, csr.indirectHeap[IndirectHeap::DYNAMIC_STATE]);
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenReleaseIndirectHeapIsCalledThenHeapAllocationIsNull) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &heap = csr.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 10u);
    csr.releaseIndirectHeap(IndirectHeap::DYNAMIC_STATE);
    EXPECT_EQ(nullptr, heap.getGraphicsAllocation());
    EXPECT_EQ(0u, heap.getMaxAvailableSpace());
}

HWTEST_F(CommandStreamReceiverTest, givenCsrWhenAllocateHeapMemoryIsCalledThenHeapMemoryIsAllocated) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    IndirectHeap *dsh = nullptr;
    csr.allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, 4096u, dsh);
    EXPECT_NE(nullptr, dsh);
    ASSERT_NE(nullptr, dsh->getGraphicsAllocation());
    csr.getMemoryManager()->freeGraphicsMemory(dsh->getGraphicsAllocation());
    delete dsh;
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWithoutTagAllocationWhenGetTagAllocationIsCalledThenNullptrIsReturned) {
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver csr(executionEnvironment);
    EXPECT_EQ(nullptr, csr.getTagAllocation());
}

HWTEST_F(CommandStreamReceiverTest, givenDebugVariableEnabledWhenCreatingCsrThenEnableTimestampPacketWriteMode) {
    DebugManagerStateRestore restore;

    DebugManager.flags.EnableTimestampPacket.set(true);
    ExecutionEnvironment executionEnvironment;
    CommandStreamReceiverHw<FamilyType> csr1(*platformDevices[0], executionEnvironment);
    EXPECT_TRUE(csr1.peekTimestampPacketWriteEnabled());

    DebugManager.flags.EnableTimestampPacket.set(false);
    CommandStreamReceiverHw<FamilyType> csr2(*platformDevices[0], executionEnvironment);
    EXPECT_FALSE(csr2.peekTimestampPacketWriteEnabled());
}

HWTEST_F(CommandStreamReceiverTest, whenCsrIsCreatedThenUseTimestampPacketWriteIfPossible) {
    CommandStreamReceiverHw<FamilyType> csr(*platformDevices[0], executionEnvironment);
    EXPECT_EQ(UnitTestHelper<FamilyType>::isTimestampPacketWriteSupported(), csr.peekTimestampPacketWriteEnabled());
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWithTagAllocationSetWhenGetTagAllocationIsCalledThenCorrectAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver csr(executionEnvironment);
    MockGraphicsAllocation allocation(reinterpret_cast<void *>(0x1000), 0x1000);
    csr.setTagAllocation(&allocation);
    EXPECT_EQ(&allocation, csr.getTagAllocation());
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenItIsDestroyedThenItDestroysTagAllocation) {
    struct MockGraphicsAllocationWithDestructorTracing : public GraphicsAllocation {
        using GraphicsAllocation::GraphicsAllocation;
        ~MockGraphicsAllocationWithDestructorTracing() override { *destructorCalled = true; }
        bool *destructorCalled = nullptr;
    };

    bool destructorCalled = false;

    auto mockGraphicsAllocation = new MockGraphicsAllocationWithDestructorTracing(nullptr, 0llu, 0llu, 1u);
    mockGraphicsAllocation->destructorCalled = &destructorCalled;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.commandStreamReceivers.push_back(std::make_unique<MockCommandStreamReceiver>(executionEnvironment));
    auto csr = executionEnvironment.commandStreamReceivers[0].get();
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(false, false, executionEnvironment));
    csr->setTagAllocation(mockGraphicsAllocation);
    EXPECT_FALSE(destructorCalled);
    executionEnvironment.commandStreamReceivers[0].reset(nullptr);
    EXPECT_TRUE(destructorCalled);
}

TEST(CommandStreamReceiverSimpleTest, givenCommandStreamReceiverWhenInitializeTagAllocationIsCalledThenTagAllocationIsBeingAllocated) {
    ExecutionEnvironment executionEnvironment;
    auto csr = new MockCommandStreamReceiver(executionEnvironment);
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(csr));
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(false, false, executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() == nullptr);
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() != nullptr);
    EXPECT_EQ(*csr->getTagAddress(), initialHardwareTag);
}

TEST(CommandStreamReceiverSimpleTest, givenNullHardwareDebugModeWhenInitializeTagAllocationIsCalledThenTagAllocationIsBeingAllocatedAndinitialValueIsMinusOne) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableNullHardware.set(true);
    ExecutionEnvironment executionEnvironment;
    auto csr = new MockCommandStreamReceiver(executionEnvironment);
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(csr));
    executionEnvironment.memoryManager.reset(new OsAgnosticMemoryManager(false, false, executionEnvironment));
    EXPECT_EQ(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() == nullptr);
    csr->initializeTagAllocation();
    EXPECT_NE(nullptr, csr->getTagAllocation());
    EXPECT_TRUE(csr->getTagAddress() != nullptr);
    EXPECT_EQ(*csr->getTagAddress(), static_cast<uint32_t>(-1));
}

TEST(CommandStreamReceiverSimpleTest, givenCSRWhenWaitBeforeMakingNonResidentWhenRequiredIsCalledWithBlockingFlagSetThenItReturnsImmediately) {
    ExecutionEnvironment executionEnvironment;
    MockCommandStreamReceiver csr(executionEnvironment);
    uint32_t tag = 0;
    MockGraphicsAllocation allocation(&tag, sizeof(tag));
    csr.latestFlushedTaskCount = 3;
    csr.setTagAllocation(&allocation);
    csr.waitBeforeMakingNonResidentWhenRequired();

    EXPECT_EQ(0u, tag);
}

TEST(CommandStreamReceiverMultiContextTests, givenMultipleCsrsWhenSameResourcesAreUsedThenResidencyIsProperlyHandled) {
    auto executionEnvironment = new ExecutionEnvironment;

    std::unique_ptr<Device> device0(Device::create<Device>(nullptr, executionEnvironment, 0u));
    std::unique_ptr<Device> device1(Device::create<Device>(nullptr, executionEnvironment, 1u));

    auto &commandStreamReceiver0 = device0->getCommandStreamReceiver();
    auto &commandStreamReceiver1 = device1->getCommandStreamReceiver();

    auto graphicsAllocation = executionEnvironment->memoryManager->allocateGraphicsMemory(4096u);

    commandStreamReceiver0.makeResident(*graphicsAllocation);
    commandStreamReceiver1.makeResident(*graphicsAllocation);

    EXPECT_EQ(1u, commandStreamReceiver0.getResidencyAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getResidencyAllocations().size());

    EXPECT_EQ(1u, graphicsAllocation->getResidencyTaskCount(0u));
    EXPECT_EQ(1u, graphicsAllocation->getResidencyTaskCount(1u));

    commandStreamReceiver0.makeNonResident(*graphicsAllocation);
    commandStreamReceiver1.makeNonResident(*graphicsAllocation);

    EXPECT_FALSE(graphicsAllocation->isResident(0u));
    EXPECT_FALSE(graphicsAllocation->isResident(1u));

    EXPECT_EQ(1u, commandStreamReceiver0.getEvictionAllocations().size());
    EXPECT_EQ(1u, commandStreamReceiver1.getEvictionAllocations().size());

    executionEnvironment->memoryManager->freeGraphicsMemory(graphicsAllocation);
}

struct CreateAllocationForHostSurfaceTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new ExecutionEnvironment;
        gmockMemoryManager = new ::testing::NiceMock<GMockMemoryManager>(*executionEnvironment);
        executionEnvironment->memoryManager.reset(gmockMemoryManager);
        device.reset(Device::create<Device>(&hwInfo, executionEnvironment, 0u));
        commandStreamReceiver = &device->getCommandStreamReceiver();
    }
    HardwareInfo hwInfo = *platformDevices[0];
    ExecutionEnvironment *executionEnvironment = nullptr;
    GMockMemoryManager *gmockMemoryManager = nullptr;
    std::unique_ptr<Device> device;
    CommandStreamReceiver *commandStreamReceiver = nullptr;
};

TEST_F(CreateAllocationForHostSurfaceTest, givenReadOnlyHostPointerWhenAllocationForHostSurfaceWithPtrCopyAllowedIsCreatedThenCopyAllocationIsCreatedAndMemoryCopied) {
    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);
    HostPtrSurface surface(const_cast<char *>(memory), size, true);

    if (device->isFullRangeSvm()) {
        EXPECT_CALL(*gmockMemoryManager, populateOsHandles(::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(MemoryManager::AllocationStatus::InvalidHostPointer));
    } else {
        EXPECT_CALL(*gmockMemoryManager, allocateGraphicsMemoryForNonSvmHostPtr(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(nullptr));
    }

    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, *device, false);
    EXPECT_TRUE(result);

    auto allocation = surface.getAllocation();
    ASSERT_NE(nullptr, allocation);

    EXPECT_NE(memory, allocation->getUnderlyingBuffer());
    EXPECT_THAT(allocation->getUnderlyingBuffer(), MemCompare(memory, size));

    allocation->updateTaskCount(commandStreamReceiver->peekLatestFlushedTaskCount(), 0u);
}

TEST_F(CreateAllocationForHostSurfaceTest, givenReadOnlyHostPointerWhenAllocationForHostSurfaceWithPtrCopyNotAllowedIsCreatedThenCopyAllocationIsNotCreated) {
    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);
    HostPtrSurface surface(const_cast<char *>(memory), size, false);

    if (device->isFullRangeSvm()) {
        EXPECT_CALL(*gmockMemoryManager, populateOsHandles(::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(MemoryManager::AllocationStatus::InvalidHostPointer));
    } else {
        EXPECT_CALL(*gmockMemoryManager, allocateGraphicsMemoryForNonSvmHostPtr(::testing::_, ::testing::_))
            .Times(1)
            .WillOnce(::testing::Return(nullptr));
    }

    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, *device, false);
    EXPECT_FALSE(result);

    auto allocation = surface.getAllocation();
    EXPECT_EQ(nullptr, allocation);
}

struct ReducedAddrSpaceCommandStreamReceiverTest : public CreateAllocationForHostSurfaceTest {
    void SetUp() override {
        hwInfo.capabilityTable.gpuAddressSpace = MemoryConstants::max32BitAddress;
        CreateAllocationForHostSurfaceTest::SetUp();
    }
};

TEST_F(ReducedAddrSpaceCommandStreamReceiverTest,
       givenReducedGpuAddressSpaceWhenAllocationForHostSurfaceIsCreatedThenAllocateGraphicsMemoryForNonSvmHostPtrIsCalled) {

    char memory[8] = {};
    HostPtrSurface surface(const_cast<char *>(memory), sizeof(memory), false);

    EXPECT_CALL(*gmockMemoryManager, allocateGraphicsMemoryForNonSvmHostPtr(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(nullptr));

    bool result = commandStreamReceiver->createAllocationForHostSurface(surface, *device, false);
    EXPECT_FALSE(result);
}
