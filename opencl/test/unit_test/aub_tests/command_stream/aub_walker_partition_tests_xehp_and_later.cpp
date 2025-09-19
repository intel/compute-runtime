/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_walker_partition_fixture.h"

using namespace NEO;
using namespace WalkerPartition;

int32_t testPartitionCount[] = {1, 2, 4, 8, 16};
int32_t testPartitionType[] = {1, 2, 3};
uint32_t testWorkingDimensions[] = {3};

DispatchParameters dispatchParametersForTests[] = {
    {{12, 25, 21}, {3, 5, 7}},
    {{8, 16, 20}, {8, 4, 2}},
    {{7, 13, 17}, {1, 1, 1}},
};

using AubWalkerPartitionZeroTest = Test<AubWalkerPartitionZeroFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, whenPartitionCountSetToZeroThenProvideEqualSingleWalker) {

    size_t globalWorkOffset[3] = {0, 0, 0};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;
    size_t gwsSize[] = {128, 1, 1};
    size_t lwsSize[] = {32, 1, 1};

    auto retVal = pCmdQ->enqueueKernel(
        kernels[5].get(),
        workingDimensions,
        globalWorkOffset,
        gwsSize,
        lwsSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    auto cmdPartitionCount = static_cast<uint32_t>(partitionCount);

    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PARTITION_TYPE = typename WalkerType::PARTITION_TYPE;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, pCmdQ->getCS(0).getCpuBase(), pCmdQ->getCS(0).getUsed()));

    auto walkerCmds = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());

    auto walkersCount = static_cast<uint32_t>(walkerCmds.size());
    EXPECT_EQ(cmdPartitionCount + 1, walkersCount);

    for (auto &walkerCmd : walkerCmds) {
        auto walker = genCmdCast<WalkerType *>(*walkerCmd);
        auto cmdPartitionType = static_cast<PARTITION_TYPE>(partitionType);

        EXPECT_EQ(cmdPartitionCount, walker->getPartitionId());
        EXPECT_EQ(cmdPartitionType, walker->getPartitionType());
        EXPECT_EQ(cmdPartitionCount, walker->getPartitionSize());
    }

    auto dstGpuAddress = addrToPtr(ptrOffset(dstBuffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), dstBuffer->getOffset()));
    expectMemory<FamilyType>(dstGpuAddress, &gwsSize[workingDimensions - 1], sizeof(uint32_t));

    const uint32_t workgroupCount = static_cast<uint32_t>(gwsSize[workingDimensions - 1] / lwsSize[workingDimensions - 1]);
    auto groupSpecificWorkCounts = ptrOffset(dstGpuAddress, 4);
    StackVec<uint32_t, 8> workgroupCounts;
    workgroupCounts.resize(workgroupCount);

    for (uint32_t workgroupId = 0u; workgroupId < workgroupCount; workgroupId++) {
        workgroupCounts[workgroupId] = static_cast<uint32_t>(lwsSize[workingDimensions - 1]);
    }

    expectMemory<FamilyType>(groupSpecificWorkCounts, workgroupCounts.begin(), workgroupCounts.size() * sizeof(uint32_t));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, whenPipeControlIsBeingEmittedWithPartitionBitSetThenMultipleFieldsAreBeingUpdatedWithValue) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto partitionId = 1u;
    auto writeSize = 8u;
    auto miAddressOffset = WalkerPartition::addressOffsetCCSOffset;
    auto wparidOffset = WalkerPartition::wparidCCSOffset;
    uint64_t writeValue = 7llu;

    uint32_t totalBytesProgrammed = 0u;
    auto streamCpuPointer = taskStream->getSpace(0);

    WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, wparidOffset, totalBytesProgrammed, partitionId);
    WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, miAddressOffset, totalBytesProgrammed, writeSize);
    taskStream->getSpace(totalBytesProgrammed);

    void *pipeControlAddress = taskStream->getSpace(0);
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    auto pipeControl = retrieveSyncPipeControl<FamilyType>(pipeControlAddress, device->getRootDeviceEnvironment());
    ASSERT_NE(nullptr, pipeControl);
    pipeControl->setWorkloadPartitionIdOffsetEnable(true);

    flushStream();

    expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, 4u);
    // write needs to happen after 8 bytes
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress + 8), &writeValue, 4u);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenAtomicOperationDecOnLocalMemoryWhenItIsExecuteThenOperationUpdatesMemory) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto cpuAddress = reinterpret_cast<int *>(helperSurface->getUnderlyingBuffer());
    *cpuAddress = 10;

    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    uint32_t expectedValue = 9u;
    WalkerPartition::programMiAtomic<FamilyType>(streamCpuPointer, totalBytesProgrammed, writeAddress, false, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT);
    taskStream->getSpace(totalBytesProgrammed);

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &expectedValue, sizeof(expectedValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenAtomicOperationIncOnLocalMemoryWhenItIsExecuteThenOperationUpdatesMemory) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto cpuAddress = reinterpret_cast<int *>(helperSurface->getUnderlyingBuffer());
    *cpuAddress = 10;

    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    uint32_t expectedValue = 11u;
    WalkerPartition::programMiAtomic<FamilyType>(streamCpuPointer, totalBytesProgrammed, writeAddress, false, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);
    taskStream->getSpace(totalBytesProgrammed);

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &expectedValue, sizeof(expectedValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenVariousCompareModesWhenConditionalBatchBufferEndIsEmittedItThenHandlesCompareCorrectly) {
    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    auto writeAddress = helperSurface->getGpuAddress();
    auto compareAddress = reinterpret_cast<int *>(helperSurface->getUnderlyingBuffer());

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(taskStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(writeAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    uint32_t writeValue = 7u;
    uint32_t pipeControlNotExecutedValue = 0u;

    // this pipe control should be executed
    void *pipeControlAddress = taskStream->getSpace(0);
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    auto pipeControl = retrieveSyncPipeControl<FamilyType>(pipeControlAddress, device->getRootDeviceEnvironment());
    ASSERT_NE(nullptr, pipeControl);
    auto programPipeControl = [&]() {
        pipeControl->setImmediateData(writeValue);
        pipeControl->setAddress(static_cast<uint32_t>(writeAddress & 0x0000FFFFFFFFULL));
        pipeControl->setAddressHigh(static_cast<uint32_t>(writeAddress >> 32));
    };

    // we have now command buffer that has conditional batch buffer end and pipe control that tests whether batch buffer end acted correctly

    // MAD_GREATER_THAN_IDD If Indirect fetched data is greater than inline data then continue.
    // continue test
    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_GREATER_THAN_IDD);
    *compareAddress = 11;
    auto inlineData = 10u;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
    // terminate test
    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    // MAD_GREATER_THAN_OR_EQUAL_IDD	If Indirect fetched data is greater than or equal to inline data then continue.

    // continue test - greater
    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_GREATER_THAN_OR_EQUAL_IDD);
    *compareAddress = 11;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    // continue test - equal
    *compareAddress = 10;
    inlineData = 10u;

    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    // terminate test
    *compareAddress = 9;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    // MAD_LESS_THAN_IDD	If Indirect fetched data is less than inline data then continue.

    // continue test
    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_LESS_THAN_IDD);
    *compareAddress = 9;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    // terminate test
    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    // MAD_LESS_THAN_OR_EQUAL_IDD	If Indirect fetched data is less than or equal to inline data then continue.

    // continue test - less
    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_LESS_THAN_OR_EQUAL_IDD);
    *compareAddress = 9;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    // continue test - equal
    *compareAddress = 10;
    inlineData = 10u;

    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    // terminate test
    *compareAddress = 11;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    // MAD_EQUAL_IDD	If Indirect fetched data is equal to inline data then continue.

    // continue test equal
    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_EQUAL_IDD);
    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    // terminate test
    *compareAddress = 0;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    // MAD_NOT_EQUAL_IDD	If Indirect fetched data is not equal to inline data then continue.

    // continue test not equal
    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_NOT_EQUAL_IDD);
    *compareAddress = 11;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    // terminate test
    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));
}
template <bool enableNesting>
struct MultiLevelBatchAubFixture : public AUBFixture {
    void setUp() {
        if (enableNesting) {
            // turn on Batch Buffer nesting
            debugManager.flags.AubDumpAddMmioRegistersList.set(
                "0x1A09C;0x10001000");
        } else {
            // turn off Batch Buffer nesting
            debugManager.flags.AubDumpAddMmioRegistersList.set(
                "0x1A09C;0x10000000");
        }
        AUBFixture::setUp(nullptr);

        auto memoryManager = this->device->getMemoryManager();

        commandBufferProperties = std::make_unique<AllocationProperties>(device->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::commandBuffer, false, device->getDeviceBitfield());
        streamAllocation = memoryManager->allocateGraphicsMemoryWithProperties(*commandBufferProperties);
        helperSurface = memoryManager->allocateGraphicsMemoryWithProperties(*commandBufferProperties);
        memset(helperSurface->getUnderlyingBuffer(), 0, MemoryConstants::pageSize);
        taskStream = std::make_unique<LinearStream>(streamAllocation);

        secondLevelBatch = memoryManager->allocateGraphicsMemoryWithProperties(*commandBufferProperties);
        thirdLevelBatch = memoryManager->allocateGraphicsMemoryWithProperties(*commandBufferProperties);
        secondLevelBatchStream = std::make_unique<LinearStream>(secondLevelBatch);
        thirdLevelBatchStream = std::make_unique<LinearStream>(thirdLevelBatch);
    };
    void tearDown() {
        debugManager.flags.AubDumpAddMmioRegistersList.getRef() = "unk";
        debugManager.flags.AubDumpAddMmioRegistersList.getRef().shrink_to_fit();

        auto memoryManager = this->device->getMemoryManager();
        memoryManager->freeGraphicsMemory(thirdLevelBatch);
        memoryManager->freeGraphicsMemory(secondLevelBatch);
        memoryManager->freeGraphicsMemory(streamAllocation);
        memoryManager->freeGraphicsMemory(helperSurface);

        AUBFixture::tearDown();
    };

    void flushStream() {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.guardCommandBufferWithPipeControl = true;

        csr->makeResident(*helperSurface);
        csr->flushTask(*taskStream, 0,
                       &csr->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u),
                       &csr->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u),
                       &csr->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u),
                       0u, dispatchFlags, device->getDevice());

        csr->flushBatchedSubmissions();
    }

    DebugManagerStateRestore debugRestorer;

    std::unique_ptr<LinearStream> taskStream;
    GraphicsAllocation *streamAllocation = nullptr;
    GraphicsAllocation *helperSurface = nullptr;
    std::unique_ptr<AllocationProperties> commandBufferProperties;

    std::unique_ptr<LinearStream> secondLevelBatchStream;
    std::unique_ptr<LinearStream> thirdLevelBatchStream;

    GraphicsAllocation *secondLevelBatch = nullptr;
    GraphicsAllocation *thirdLevelBatch = nullptr;
};

using MultiLevelBatchTestsWithNesting = Test<MultiLevelBatchAubFixture<true>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithNesting, givenConditionalBatchBufferEndWhenItExitsThirdLevelCommandBufferThenSecondLevelBatchIsResumed) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    getSimulatedCsr<FamilyType>()->initializeEngine();
    writeMMIO<FamilyType>(0x1A09C, 0x10001000);

    // nest to second level
    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(secondLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    // nest to third  level
    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(thirdLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    // this pipe control should be executed
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    csr->makeResident(*secondLevelBatch);
    csr->makeResident(*thirdLevelBatch);
    flushStream();

    writeAddress = helperSurface->getGpuAddress() + sizeof(uint64_t);
    writeValue = 7u;

    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
    writeAddress += sizeof(uint64_t);
    writeValue++;
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithNesting, givenConditionalBatchBufferEndWhenItExitsToTheRingThenAllCommandBufferLevelsAreSkipped) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    // nest to second level
    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(secondLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    // nest to third  level
    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(thirdLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(0);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    // this pipe control should NOT be executed
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    csr->makeResident(*secondLevelBatch);
    csr->makeResident(*thirdLevelBatch);
    flushStream();

    writeAddress = helperSurface->getGpuAddress() + sizeof(uint64_t);
    writeValue = 0u;

    // pipe controls are not emitted
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
    writeAddress += sizeof(uint64_t);
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithNesting, givenCommandBufferCacheOnWhenBatchBufferIsExecutedThenItWorksCorrectly) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto writeValue = 7u;

    using BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    // nest to second level
    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(secondLevelBatch->getGpuAddress());
    batchBufferStart->setEnableCommandCache(1u);
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    // this pipe control should be executed
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    auto batchBufferEnd = reinterpret_cast<BATCH_BUFFER_END *>(secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_END)));
    batchBufferEnd->init();

    csr->makeResident(*secondLevelBatch);

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}
using MultiLevelBatchTestsWithoutNesting = Test<MultiLevelBatchAubFixture<false>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithoutNesting, givenConditionalBBEndWhenItExitsFromSecondLevelThenUpperLevelIsResumed) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    // nest to second level
    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(secondLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    // nest to third  level
    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(thirdLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(0);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    // this pipe control should not be executed
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    // and this shouldn't as well, we returned to ring
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    csr->makeResident(*secondLevelBatch);
    csr->makeResident(*thirdLevelBatch);
    flushStream();

    writeAddress = helperSurface->getGpuAddress() + sizeof(uint64_t);
    auto zeroValue = 0llu;

    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, sizeof(zeroValue));
    writeAddress += sizeof(uint64_t);
    writeValue++;
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, sizeof(zeroValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithoutNesting, givenConditionalBBEndWhenExitsFromSecondLevelToRingThenFirstLevelIsNotExecuted) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    // nest to second level
    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(secondLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    // nest to third  level
    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(thirdLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    // this pipe control should not be executed
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    // and this should , we returned to primary batch
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    csr->makeResident(*secondLevelBatch);
    csr->makeResident(*thirdLevelBatch);
    flushStream();

    writeAddress = helperSurface->getGpuAddress() + sizeof(uint64_t);
    writeValue = 7u;
    auto zeroValue = 0llu;

    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, sizeof(zeroValue));
    writeAddress += sizeof(uint64_t);
    writeValue++;
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenBlockingAtomicOperationIncOnLocalMemoryWhenItIsExecutedThenOperationUpdatesMemory) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto cpuAddress = reinterpret_cast<int *>(helperSurface->getUnderlyingBuffer());
    *cpuAddress = 10;

    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    uint32_t expectedValue = 11u;
    WalkerPartition::programMiAtomic<FamilyType>(streamCpuPointer, totalBytesProgrammed, writeAddress, true, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);
    taskStream->getSpace(totalBytesProgrammed);

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &expectedValue, sizeof(expectedValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenBlockingAtomicOperationIncOnSystemMemoryWhenItIsExecutedThenOperationUpdatesMemory) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto cpuAddress = reinterpret_cast<int *>(helperSurface->getUnderlyingBuffer());
    *cpuAddress = 10;

    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    uint32_t expectedValue = 11u;
    WalkerPartition::programMiAtomic<FamilyType>(streamCpuPointer, totalBytesProgrammed, writeAddress, true, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);
    taskStream->getSpace(totalBytesProgrammed);

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &expectedValue, sizeof(expectedValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenNonBlockingAtomicOperationIncOnSystemMemoryWhenItIsExecutedThenOperationUpdatesMemory) {
    auto writeAddress = helperSurface->getGpuAddress();
    auto cpuAddress = reinterpret_cast<int *>(helperSurface->getUnderlyingBuffer());
    *cpuAddress = 10;

    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    uint32_t expectedValue = 11u;
    WalkerPartition::programMiAtomic<FamilyType>(streamCpuPointer, totalBytesProgrammed, writeAddress, false, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);
    taskStream->getSpace(totalBytesProgrammed);

    flushStream();
    expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &expectedValue, sizeof(expectedValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenPredicatedCommandBufferWhenItIsExecutedThenAtomicIsIncrementedEquallyToPartitionCountPlusOne) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());

    auto streamCpuPointer = taskStream->getSpace(0);
    auto postSyncAddress = helperSurface->getGpuAddress();

    uint32_t totalBytesProgrammed = 0u;
    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walkerCmd.setPartitionType(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X);
    walkerCmd.getInterfaceDescriptor().setNumberOfThreadsInGpgpuThreadGroup(1u);
    walkerCmd.getPostSync().setDestinationAddress(postSyncAddress);
    walkerCmd.getPostSync().setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
    if constexpr (FamilyType::template isHeaplessMode<DefaultWalkerType>()) {
        walkerCmd.setMaximumNumberOfThreads(64);
    }

    WalkerPartition::WalkerPartitionArgs testArgs = {};
    testArgs.initializeWparidRegister = true;
    testArgs.crossTileAtomicSynchronization = true;
    testArgs.emitPipeControlStall = true;
    testArgs.tileCount = 1;
    testArgs.partitionCount = 16u;
    testArgs.synchronizeBeforeExecution = false;
    testArgs.secondaryBatchBuffer = false;
    testArgs.emitSelfCleanup = false;
    testArgs.dcFlushEnable = NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(
        streamCpuPointer,
        nullptr,
        taskStream->getGraphicsAllocation()->getGpuAddress(),
        &walkerCmd,
        totalBytesProgrammed,
        testArgs,
        this->device->getDevice());
    taskStream->getSpace(totalBytesProgrammed);
    flushStream();
    auto expectedGpuAddress = taskStream->getGraphicsAllocation()->getGpuAddress() +
                              WalkerPartition::computeControlSectionOffset<FamilyType, DefaultWalkerType>(testArgs);

    // 16 partitions updated atomic to value 16
    // 17th partition updated it to 17 and was predicated out of the batch buffer
    uint32_t expectedValue = 17u;
    expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddress), &expectedValue, sizeof(expectedValue));
    // this is 1 tile scenario
    uint32_t expectedTileValue = 1u;
    expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddress + 4llu), &expectedTileValue, sizeof(expectedTileValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenGeneralPurposeRegisterWhenItIsLoadedAndFetchedThenItIsNotPrivileged) {
    auto writeAddress = helperSurface->getGpuAddress();
    uint32_t writeValue = 7u;

    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    uint32_t wparidValue = 5u;
    WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, generalPurposeRegister0, totalBytesProgrammed, wparidValue);
    WalkerPartition::programMiLoadRegisterReg<FamilyType>(streamCpuPointer, totalBytesProgrammed, generalPurposeRegister0, generalPurposeRegister1);
    WalkerPartition::programMiLoadRegisterReg<FamilyType>(streamCpuPointer, totalBytesProgrammed, generalPurposeRegister1, generalPurposeRegister2);
    WalkerPartition::programMiLoadRegisterReg<FamilyType>(streamCpuPointer, totalBytesProgrammed, generalPurposeRegister2, generalPurposeRegister3);
    WalkerPartition::programMiLoadRegisterReg<FamilyType>(streamCpuPointer, totalBytesProgrammed, generalPurposeRegister3, generalPurposeRegister4);
    WalkerPartition::programMiLoadRegisterReg<FamilyType>(streamCpuPointer, totalBytesProgrammed, generalPurposeRegister4, generalPurposeRegister5);
    WalkerPartition::programMiLoadRegisterReg<FamilyType>(streamCpuPointer, totalBytesProgrammed, generalPurposeRegister5, wparidCCSOffset);
    WalkerPartition::programWparidMask<FamilyType>(streamCpuPointer, totalBytesProgrammed, 4u);
    WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, true);
    // this command must not execute
    taskStream->getSpace(totalBytesProgrammed);
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

    streamCpuPointer = taskStream->getSpace(0);
    totalBytesProgrammed = 0u;
    WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, false);
    taskStream->getSpace(totalBytesProgrammed);
    flushStream();
    expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenPredicationWhenItIsOnThenCommandMustNotBeExecuted) {
    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    auto writeValue = 1u;
    auto zeroValue = 0u;
    auto addressShift = 8u;
    auto writeAddress = helperSurface->getGpuAddress();

    // program WPARID mask to 16 partitions
    WalkerPartition::programWparidMask<FamilyType>(streamCpuPointer, totalBytesProgrammed, 16u);
    streamCpuPointer = taskStream->getSpace(totalBytesProgrammed);
    // program WPARID to value within 0-19
    for (uint32_t wparid = 0u; wparid < 20; wparid++) {
        totalBytesProgrammed = 0;
        streamCpuPointer = taskStream->getSpace(0);
        WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, WalkerPartition::wparidCCSOffset, totalBytesProgrammed, wparid);
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, true);
        taskStream->getSpace(totalBytesProgrammed);
        // emit pipe control
        PipeControlArgs args;
        MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
            *taskStream, PostSyncMode::immediateData,
            writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

        // turn off predication
        streamCpuPointer = taskStream->getSpace(0);
        totalBytesProgrammed = 0;
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, false);
        taskStream->getSpace(totalBytesProgrammed);

        writeAddress += addressShift;
        writeValue++;
    }

    flushStream();
    writeAddress = helperSurface->getGpuAddress();
    writeValue = 1u;
    for (uint32_t wparid = 0u; wparid < 20; wparid++) {
        if (wparid < 16) {
            expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, 4u);
        } else {
            expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, 4u);
        }
        writeAddress += addressShift;
        writeValue++;
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenPredicationWhenItIsOnThenPipeControlInWparidIsNotExecuted) {
    auto streamCpuPointer = taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    auto writeValue = 1u;
    auto zeroValue = 0u;
    auto addressShift = 32u;
    auto writeAddress = helperSurface->getGpuAddress();

    WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, WalkerPartition::addressOffsetCCSOffset, totalBytesProgrammed, addressShift);
    // program WPARID mask to 8 partitions
    WalkerPartition::programWparidMask<FamilyType>(streamCpuPointer, totalBytesProgrammed, 8u);
    streamCpuPointer = taskStream->getSpace(totalBytesProgrammed);
    // program WPARID to value within 0-13
    for (uint32_t wparid = 0u; wparid < 13; wparid++) {
        totalBytesProgrammed = 0;
        streamCpuPointer = taskStream->getSpace(0);
        WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, WalkerPartition::wparidCCSOffset, totalBytesProgrammed, wparid);
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, true);
        taskStream->getSpace(totalBytesProgrammed);

        // emit pipe control
        void *pipeControlAddress = taskStream->getSpace(0);
        PipeControlArgs args;
        MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
            *taskStream, PostSyncMode::immediateData,
            writeAddress, writeValue, device->getRootDeviceEnvironment(), args);

        auto pipeControl = retrieveSyncPipeControl<FamilyType>(pipeControlAddress, device->getRootDeviceEnvironment());
        ASSERT_NE(nullptr, pipeControl);
        pipeControl->setWorkloadPartitionIdOffsetEnable(true);
        // turn off predication
        streamCpuPointer = taskStream->getSpace(0);
        totalBytesProgrammed = 0;
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, false);
        taskStream->getSpace(totalBytesProgrammed);

        writeValue++;
    }

    flushStream();
    writeAddress = helperSurface->getGpuAddress();
    writeValue = 1u;
    for (uint32_t wparid = 0u; wparid < 13; wparid++) {
        if (wparid < 8) {
            expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, 4u);
        } else {
            expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, 4u);
        }
        writeAddress += addressShift;
        writeValue++;
    }
}

HWCMDTEST_P(IGFX_XE_HP_CORE, AubWalkerPartitionTest, whenPartitionsAreUsedWithVariousInputsThenHardwareProgrammingIsCorrect) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event;

    auto retVal = pCmdQ->enqueueKernel(
        kernels[5].get(),
        workingDimensions,
        globalWorkOffset,
        dispatchParameters.globalWorkSize,
        dispatchParameters.localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    auto neoEvent = castToObject<Event>(event);
    auto container = neoEvent->getTimestampPacketNodes();
    auto postSyncAddress = TimestampPacketHelper::getContextStartGpuAddress(*container->peekNodes()[0]);
    validatePartitionProgramming<FamilyType>(postSyncAddress, partitionCount);

    clReleaseEvent(event);
}

INSTANTIATE_TEST_SUITE_P(
    AUBWPARID,
    AubWalkerPartitionTest,
    ::testing::Combine(
        ::testing::ValuesIn(testPartitionCount),
        ::testing::ValuesIn(testPartitionType),
        ::testing::ValuesIn(dispatchParametersForTests),
        ::testing::ValuesIn(testWorkingDimensions)));

using AubWparidTests = Test<AubWalkerPartitionFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWparidTests, whenPartitionCountSetAndPartitionIdSpecifiedViaWPARIDThenProvideEqualNumberWalkers) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event event;
    workingDimensions = 3;
    dispatchParameters.globalWorkSize[0] = 30;
    dispatchParameters.globalWorkSize[1] = 39;
    dispatchParameters.globalWorkSize[2] = 5;
    dispatchParameters.localWorkSize[0] = 10;
    dispatchParameters.localWorkSize[1] = 3;
    dispatchParameters.localWorkSize[2] = 1;

    partitionType = 3;

    int32_t partitionCount = 4;

    debugManager.flags.ExperimentalSetWalkerPartitionType.set(partitionType);
    debugManager.flags.ExperimentalSetWalkerPartitionCount.set(partitionCount);
    debugManager.flags.EnableWalkerPartition.set(1u);

    auto retVal = pCmdQ->enqueueKernel(
        kernels[5].get(),
        workingDimensions,
        globalWorkOffset,
        dispatchParameters.globalWorkSize,
        dispatchParameters.localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        &event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    auto neoEvent = castToObject<Event>(event);
    auto container = neoEvent->getTimestampPacketNodes();
    auto postSyncAddress = TimestampPacketHelper::getContextStartGpuAddress(*container->peekNodes()[0]);

    validatePartitionProgramming<FamilyType>(postSyncAddress, partitionCount);
    clReleaseEvent(event);
}
