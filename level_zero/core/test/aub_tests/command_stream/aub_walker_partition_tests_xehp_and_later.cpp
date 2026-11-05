/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/aub_tests/command_stream/command_stream_aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <tuple>

extern bool generateRandomInput;

using namespace NEO;
using namespace WalkerPartition;

namespace L0 {
namespace ult {

struct WalkerPartitionDispatchParameters {
    size_t globalWorkSize[3];
    size_t localWorkSize[3];
};

int32_t walkerPartitionTestPartitionCount[] = {1, 2, 4, 8, 16};
int32_t walkerPartitionTestPartitionType[] = {1, 2, 3};
uint32_t walkerPartitionTestWorkingDimensions[] = {3};

WalkerPartitionDispatchParameters walkerPartitionTestDispatchParameters[] = {
    {{12, 25, 21}, {3, 5, 7}},
    {{8, 16, 20}, {8, 4, 2}},
    {{7, 13, 17}, {1, 1, 1}},
};

struct AubWalkerPartitionFixture : public CommandStreamAubFixture {
    void tearDown() {
        if (this->event) {
            zeEventDestroy(this->event);
        }
        if (this->eventPool) {
            zeEventPoolDestroy(this->eventPool);
        }
        CommandStreamAubFixture::tearDown();
    }

    void createKernelWithOutputBuffer() {
        this->kernel = this->createKernel("simple_kernels", "simple_kernel_5", true);
        ASSERT_NE(nullptr, this->kernel);

        this->sizeUserMemory = 16 * MemoryConstants::kiloByte;
        if (generateRandomInput) {
            this->sizeUserMemory = 16000 * MemoryConstants::kiloByte;
        }

        this->dstMemory = this->allocateDeviceMemory(this->sizeUserMemory, MemoryConstants::pageSize);
        ASSERT_NE(nullptr, this->dstMemory);

        auto zeroMemory = std::make_unique<uint8_t[]>(this->sizeUserMemory);
        auto dstAllocationData = this->driverHandle->getSvmAllocsManager()->getSVMAlloc(this->dstMemory);
        ASSERT_NE(nullptr, dstAllocationData);
        auto dstAllocation = dstAllocationData->gpuAllocations.getGraphicsAllocation(this->rootDeviceIndex);
        EXPECT_TRUE(NEO::MemoryTransferHelper::transferMemoryToAllocation(dstAllocation->isCompressionEnabled(), *this->neoDevice, dstAllocation, 0, zeroMemory.get(), this->sizeUserMemory));

        ASSERT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(this->dstMemory), &this->dstMemory));
    }

    void dispatchKernel(const size_t *globalWorkSize, const size_t *localWorkSize, ze_event_handle_t signalEvent, uint32_t commandListPartitionCount) {
        uint32_t groupSize[3] = {1, 1, 1};
        ze_group_count_t groupCount = {1, 1, 1};
        uint32_t *groupCounts[3] = {&groupCount.groupCountX, &groupCount.groupCountY, &groupCount.groupCountZ};
        for (auto dimension = 0u; dimension < this->workingDimensions; dimension++) {
            groupSize[dimension] = static_cast<uint32_t>(localWorkSize[dimension]);
            *groupCounts[dimension] = static_cast<uint32_t>(globalWorkSize[dimension] / localWorkSize[dimension]);
        }
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(this->kernel, groupSize[0], groupSize[1], groupSize[2]));

        const auto originalPartitionCount = this->commandList->partitionCount;
        this->commandList->partitionCount = commandListPartitionCount;
        auto result = zeCommandListAppendLaunchKernel(this->commandList->toHandle(), this->kernel, &groupCount, signalEvent, 0, nullptr);
        this->commandList->partitionCount = originalPartitionCount;
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
        this->executeAndSynchronize();
    }

    void dispatchPartitionedKernelWithTimestampEvent(uint64_t &postSyncAddress) {
        ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
        eventPoolDesc.count = maxTestedPartitionCount;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(this->context->toHandle(), &eventPoolDesc, 0, nullptr, &this->eventPool));

        ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
        eventDesc.index = 0;
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(this->eventPool, &eventDesc, &this->event));

        postSyncAddress = Event::fromHandle(this->event)->getPacketAddress(this->device);

        const auto commandListPartitionCount = static_cast<uint32_t>(std::max(this->partitionCount, minimalPartitionCountForWalkerPartitioning));
        this->dispatchKernel(this->dispatchParameters.globalWorkSize, this->dispatchParameters.localWorkSize, this->event, commandListPartitionCount);
    }

    template <typename FamilyType>
    void validatePartitionProgramming(uint64_t postSyncAddress, int32_t partitionCount) {
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
        uint32_t totalWorkgroupCount = 1u;
        uint32_t totalWorkItemsInWorkgroup = 1u;
        uint32_t totalWorkItemsCount = 1;

        for (auto dimension = 0u; dimension < this->workingDimensions; dimension++) {
            totalWorkgroupCount *= static_cast<uint32_t>(this->dispatchParameters.globalWorkSize[dimension] / this->dispatchParameters.localWorkSize[dimension]);
            totalWorkItemsInWorkgroup *= static_cast<uint32_t>(this->dispatchParameters.localWorkSize[dimension]);
            totalWorkItemsCount *= static_cast<uint32_t>(this->dispatchParameters.globalWorkSize[dimension]);
        }

        const uint32_t workgroupCount = static_cast<uint32_t>(this->dispatchParameters.globalWorkSize[this->partitionType - 1] / this->dispatchParameters.localWorkSize[this->partitionType - 1]);
        auto partitionSize = Math::divideAndRoundUp(workgroupCount, partitionCount);

        if (static_cast<uint32_t>(this->partitionType) > this->workingDimensions) {
            partitionSize = 1;
        }

        this->hwParser.parseCommands<FamilyType>(*this->commandList->commandContainer.getCommandStream(), 0);
        this->hwParser.findHardwareCommands<FamilyType>();
        ASSERT_NE(this->hwParser.cmdList.end(), this->hwParser.itorWalker);

        auto walkerCmd = genCmdCast<WalkerType *>(*this->hwParser.itorWalker);
        ASSERT_NE(nullptr, walkerCmd);

        EXPECT_EQ(0u, walkerCmd->getPartitionId());

        if (partitionCount > 1) {
            EXPECT_TRUE(walkerCmd->getWorkloadPartitionEnable());
            EXPECT_EQ(partitionSize, walkerCmd->getPartitionSize());
            EXPECT_EQ(this->partitionType, walkerCmd->getPartitionType());
        } else {
            EXPECT_FALSE(walkerCmd->getWorkloadPartitionEnable());
            EXPECT_EQ(0u, walkerCmd->getPartitionSize());
            EXPECT_EQ(0u, walkerCmd->getPartitionType());
        }

        EXPECT_EQ(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP, walkerCmd->getPostSync().getOperation());
        EXPECT_EQ(postSyncAddress, walkerCmd->getPostSync().getDestinationAddress());

        typename FamilyType::TimestampPacketType notExpectedValue[] = {1, 1, 1, 1};
        const auto timestampPacketSize = GfxCoreHelperHw<FamilyType>::getSingleTimestampPacketSizeHw();

        for (auto partitionId = 0; partitionId < debugManager.flags.ExperimentalSetWalkerPartitionCount.get(); partitionId++) {
            this->expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(postSyncAddress), &notExpectedValue, sizeof(notExpectedValue));
            postSyncAddress += timestampPacketSize;
        }

        auto dstGpuAddress = this->dstMemory;
        this->expectMemory<FamilyType>(dstGpuAddress, &totalWorkItemsCount, sizeof(uint32_t));
        auto groupSpecificWorkCounts = ptrOffset(dstGpuAddress, 4);
        StackVec<uint32_t, 8> workgroupCounts;
        workgroupCounts.resize(totalWorkgroupCount);

        for (uint32_t workgroupId = 0u; workgroupId < totalWorkgroupCount; workgroupId++) {
            workgroupCounts[workgroupId] = totalWorkItemsInWorkgroup;
        }

        this->expectMemory<FamilyType>(groupSpecificWorkCounts, workgroupCounts.begin(), workgroupCounts.size() * sizeof(uint32_t));
    }

    static constexpr uint32_t maxTestedPartitionCount = 16u;
    static constexpr int32_t minimalPartitionCountForWalkerPartitioning = 2;

    ze_kernel_handle_t kernel = nullptr;
    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_handle_t event = nullptr;
    void *dstMemory = nullptr;
    size_t sizeUserMemory = 0;

    uint32_t workingDimensions = 1;
    int32_t partitionCount = 0;
    int32_t partitionType = 0;

    HardwareParse hwParser;
    WalkerPartitionDispatchParameters dispatchParameters = {};
};

struct AubWalkerPartitionTest : public AubWalkerPartitionFixture,
                                public ::testing::TestWithParam<std::tuple<int32_t, int32_t, WalkerPartitionDispatchParameters, uint32_t>> {
    void SetUp() override {
        AubWalkerPartitionFixture::setUp();
        this->createKernelWithOutputBuffer();
        std::tie(this->partitionCount, this->partitionType, this->dispatchParameters, this->workingDimensions) = GetParam();

        if (generateRandomInput) {
            this->workingDimensions = (rand() % 3 + 1);
            this->partitionType = (rand() % 3 + 1);
            this->partitionCount = rand() % 16 + 1;

            auto goodWorkingSizeGenerated = false;
            while (!goodWorkingSizeGenerated) {
                this->dispatchParameters.localWorkSize[0] = rand() % 128 + 1;
                this->dispatchParameters.localWorkSize[1] = rand() % 128 + 1;
                this->dispatchParameters.localWorkSize[2] = rand() % 128 + 1;
                auto totalWorkItemsInWorkgroup = 1;
                for (auto dimension = 0u; dimension < this->workingDimensions; dimension++) {
                    totalWorkItemsInWorkgroup *= static_cast<uint32_t>(this->dispatchParameters.localWorkSize[dimension]);
                }
                if (totalWorkItemsInWorkgroup <= 1024) {
                    this->dispatchParameters.globalWorkSize[0] = this->dispatchParameters.localWorkSize[0] * (rand() % 32 + 1);
                    this->dispatchParameters.globalWorkSize[1] = this->dispatchParameters.localWorkSize[1] * (rand() % 32 + 1);
                    this->dispatchParameters.globalWorkSize[2] = this->dispatchParameters.localWorkSize[2] * (rand() % 32 + 1);

                    printf("\n generated following dispatch parameters work dim %u gws %zu %zu %zu lws %zu %zu %zu, partition type %d partitionCount %d",
                           this->workingDimensions,
                           this->dispatchParameters.globalWorkSize[0],
                           this->dispatchParameters.globalWorkSize[1],
                           this->dispatchParameters.globalWorkSize[2],
                           this->dispatchParameters.localWorkSize[0],
                           this->dispatchParameters.localWorkSize[1],
                           this->dispatchParameters.localWorkSize[2],
                           this->partitionType,
                           this->partitionCount);
                    IoFunctions::fflushPtr(stdout);
                    goodWorkingSizeGenerated = true;
                }
            }
        }

        debugManager.flags.ExperimentalSetWalkerPartitionCount.set(this->partitionCount);
        debugManager.flags.ExperimentalSetWalkerPartitionType.set(this->partitionType);
        debugManager.flags.EnableWalkerPartition.set(1u);
    }

    void TearDown() override {
        AubWalkerPartitionFixture::tearDown();
    }
};

struct AubWalkerPartitionZeroFixture : public AubWalkerPartitionFixture {
    void setUp() {
        AubWalkerPartitionFixture::setUp();

        this->partitionCount = 0;
        this->partitionType = 0;
        this->workingDimensions = 1;

        debugManager.flags.ExperimentalSetWalkerPartitionCount.set(0);
        debugManager.flags.ExperimentalSetWalkerPartitionType.set(0);

        this->helperSurface = this->allocate(MemoryConstants::pageSize, AllocationType::commandBuffer);
        ASSERT_NE(nullptr, this->helperSurface);
        memset(this->helperSurface->getUnderlyingBuffer(), 0, MemoryConstants::pageSize);

        this->systemSurface = this->allocate(MemoryConstants::pageSize, AllocationType::svmCpu);
        ASSERT_NE(nullptr, this->systemSurface);
        memset(this->systemSurface->getUnderlyingBuffer(), 0, MemoryConstants::pageSize);
    }

    template <typename FamilyType>
    void runMiAtomic(GraphicsAllocation &surface, bool requireReturnValue, typename WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES opcode, uint32_t expectedValue) {
        auto writeAddress = surface.getGpuAddress();
        *reinterpret_cast<uint32_t *>(surface.getUnderlyingBuffer()) = atomicInitialValue;

        auto streamCpuPointer = this->taskStream->getSpace(0);
        uint32_t totalBytesProgrammed = 0u;
        WalkerPartition::programMiAtomic<FamilyType>(streamCpuPointer, totalBytesProgrammed, writeAddress, requireReturnValue, opcode);
        this->taskStream->getSpace(totalBytesProgrammed);

        this->flushStream();
        this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &expectedValue, sizeof(expectedValue));
    }

    static constexpr uint32_t atomicInitialValue = 10u;

    GraphicsAllocation *helperSurface = nullptr;
    GraphicsAllocation *systemSurface = nullptr;
};

using AubWalkerPartitionZeroTest = Test<AubWalkerPartitionZeroFixture>;
using SystemMemoryAtomicsMatcher = IsNotAnyGfxCores<IGFX_GEN12LP_CORE, IGFX_XE_HPC_CORE>;

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, whenPartitionCountSetToZeroThenProvideEqualSingleWalker) {
    size_t gwsSize[] = {128, 1, 1};
    size_t lwsSize[] = {32, 1, 1};

    ASSERT_NO_FATAL_FAILURE(this->createKernelWithOutputBuffer());
    this->dispatchKernel(gwsSize, lwsSize, nullptr, this->commandList->partitionCount);

    auto cmdPartitionCount = static_cast<uint32_t>(this->partitionCount);

    using WalkerType = typename FamilyType::DefaultWalkerType;
    using PARTITION_TYPE = typename WalkerType::PARTITION_TYPE;

    auto commandStream = this->commandList->commandContainer.getCommandStream();
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandStream->getCpuBase(), commandStream->getUsed()));

    auto walkerCmds = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());

    auto walkersCount = static_cast<uint32_t>(walkerCmds.size());
    EXPECT_EQ(cmdPartitionCount + 1, walkersCount);

    for (auto &walkerCmd : walkerCmds) {
        auto walker = genCmdCast<WalkerType *>(*walkerCmd);
        auto cmdPartitionType = static_cast<PARTITION_TYPE>(this->partitionType);

        EXPECT_EQ(cmdPartitionCount, walker->getPartitionId());
        EXPECT_EQ(cmdPartitionType, walker->getPartitionType());
        EXPECT_EQ(cmdPartitionCount, walker->getPartitionSize());
    }

    auto dstGpuAddress = this->dstMemory;
    this->expectMemory<FamilyType>(dstGpuAddress, &gwsSize[this->workingDimensions - 1], sizeof(uint32_t));

    const uint32_t workgroupCount = static_cast<uint32_t>(gwsSize[this->workingDimensions - 1] / lwsSize[this->workingDimensions - 1]);
    auto groupSpecificWorkCounts = ptrOffset(dstGpuAddress, 4);
    StackVec<uint32_t, 8> workgroupCounts;
    workgroupCounts.resize(workgroupCount);

    for (uint32_t workgroupId = 0u; workgroupId < workgroupCount; workgroupId++) {
        workgroupCounts[workgroupId] = static_cast<uint32_t>(lwsSize[this->workingDimensions - 1]);
    }

    this->expectMemory<FamilyType>(groupSpecificWorkCounts, workgroupCounts.begin(), workgroupCounts.size() * sizeof(uint32_t));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, whenPipeControlIsBeingEmittedWithPartitionBitSetThenMultipleFieldsAreBeingUpdatedWithValue) {
    auto writeAddress = this->helperSurface->getGpuAddress();
    auto partitionId = 1u;
    auto writeSize = 8u;
    auto miAddressOffset = WalkerPartition::addressOffsetCCSOffset;
    auto wparidOffset = WalkerPartition::wparidCCSOffset;
    uint64_t writeValue = 7llu;

    uint32_t totalBytesProgrammed = 0u;
    auto streamCpuPointer = this->taskStream->getSpace(0);

    WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, wparidOffset, totalBytesProgrammed, partitionId);
    WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, miAddressOffset, totalBytesProgrammed, writeSize);
    this->taskStream->getSpace(totalBytesProgrammed);

    void *pipeControlAddress = this->taskStream->getSpace(0);
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(pipeControlAddress);
    ASSERT_NE(nullptr, pipeControl);
    pipeControl->setWorkloadPartitionIdOffsetEnable(true);

    this->flushStream();

    this->expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, 4u);
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress + 8), &writeValue, 4u);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenAtomicOperationDecOnLocalMemoryWhenItIsExecuteThenOperationUpdatesMemory) {
    this->runMiAtomic<FamilyType>(*this->helperSurface, false, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT, atomicInitialValue - 1);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenAtomicOperationIncOnLocalMemoryWhenItIsExecuteThenOperationUpdatesMemory) {
    this->runMiAtomic<FamilyType>(*this->helperSurface, false, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, atomicInitialValue + 1);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenVariousCompareModesWhenConditionalBatchBufferEndIsEmittedThenHandlesCompareCorrectly) {
    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    auto writeAddress = this->helperSurface->getGpuAddress();
    auto compareAddress = reinterpret_cast<int *>(this->helperSurface->getUnderlyingBuffer());

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->taskStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(writeAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    uint32_t writeValue = 7u;
    uint32_t pipeControlNotExecutedValue = 0u;

    void *pipeControlAddress = this->taskStream->getSpace(0);
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(pipeControlAddress);
    ASSERT_NE(nullptr, pipeControl);
    auto programPipeControl = [&]() {
        pipeControl->setImmediateData(writeValue);
        pipeControl->setAddress(static_cast<uint32_t>(writeAddress & 0x0000FFFFFFFFULL));
        pipeControl->setAddressHigh(static_cast<uint32_t>(writeAddress >> 32));
    };
    auto flushStreamWithUpdatedCompareValue = [&]() {
        this->helperSurface->setAubWritable(true, GraphicsAllocation::allBanks);
        this->helperSurface->setTbxWritable(true, GraphicsAllocation::allBanks);
        this->flushStream();
    };

    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_GREATER_THAN_IDD);
    *compareAddress = 11;
    auto inlineData = 10u;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_GREATER_THAN_OR_EQUAL_IDD);
    *compareAddress = 11;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    *compareAddress = 10;
    inlineData = 10u;

    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    *compareAddress = 9;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_LESS_THAN_IDD);
    *compareAddress = 9;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_LESS_THAN_OR_EQUAL_IDD);
    *compareAddress = 9;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    *compareAddress = 10;
    inlineData = 10u;

    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    *compareAddress = 11;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_EQUAL_IDD);
    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    *compareAddress = 0;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));

    conditionalBatchBufferEnd->setCompareOperation(CONDITIONAL_BATCH_BUFFER_END::COMPARE_OPERATION::COMPARE_OPERATION_MAD_NOT_EQUAL_IDD);
    *compareAddress = 11;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();

    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));

    *compareAddress = 10;
    inlineData = 10u;
    writeAddress += sizeof(uint64_t);
    writeValue++;

    conditionalBatchBufferEnd->setCompareDataDword(inlineData);
    programPipeControl();
    flushStreamWithUpdatedCompareValue();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &pipeControlNotExecutedValue, sizeof(pipeControlNotExecutedValue));
}
template <bool enableNesting>
struct MultiLevelBatchAubFixture : public CommandStreamAubFixture {
    void setUp() {
        if (enableNesting) {
            debugManager.flags.AubDumpAddMmioRegistersList.set("0x1A09C;0x10001000");
        } else {
            debugManager.flags.AubDumpAddMmioRegistersList.set("0x1A09C;0x10000000");
        }
        CommandStreamAubFixture::setUp();

        this->helperSurface = this->allocate(MemoryConstants::pageSize, AllocationType::commandBuffer);
        ASSERT_NE(nullptr, this->helperSurface);
        memset(this->helperSurface->getUnderlyingBuffer(), 0, MemoryConstants::pageSize);

        this->secondLevelBatch = this->allocate(MemoryConstants::pageSize, AllocationType::commandBuffer);
        ASSERT_NE(nullptr, this->secondLevelBatch);
        this->thirdLevelBatch = this->allocate(MemoryConstants::pageSize, AllocationType::commandBuffer);
        ASSERT_NE(nullptr, this->thirdLevelBatch);
        this->secondLevelBatchStream = std::make_unique<LinearStream>(this->secondLevelBatch);
        this->thirdLevelBatchStream = std::make_unique<LinearStream>(this->thirdLevelBatch);
    }

    GraphicsAllocation *helperSurface = nullptr;
    std::unique_ptr<LinearStream> secondLevelBatchStream;
    std::unique_ptr<LinearStream> thirdLevelBatchStream;
    GraphicsAllocation *secondLevelBatch = nullptr;
    GraphicsAllocation *thirdLevelBatch = nullptr;
};

using MultiLevelBatchTestsWithNesting = Test<MultiLevelBatchAubFixture<true>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithNesting, givenConditionalBatchBufferEndWhenItExitsThirdLevelCommandBufferThenSecondLevelBatchIsResumed) {
    auto writeAddress = this->helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    this->getSimulatedCsr<FamilyType>()->initializeEngine();
    this->getSimulatedCsr<FamilyType>()->writeMMIO(0x1A09C, 0x10001000);

    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->secondLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->thirdLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    this->flushStream();

    writeAddress = this->helperSurface->getGpuAddress() + sizeof(uint64_t);
    writeValue = 7u;

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
    writeAddress += sizeof(uint64_t);
    writeValue++;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithNesting, givenConditionalBatchBufferEndWhenItExitsToTheRingThenAllCommandBufferLevelsAreSkipped) {
    auto writeAddress = this->helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->secondLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->thirdLevelBatch->getGpuAddress());
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(0);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    this->flushStream();

    writeAddress = this->helperSurface->getGpuAddress() + sizeof(uint64_t);
    writeValue = 0u;

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
    writeAddress += sizeof(uint64_t);
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithNesting, givenCommandBufferCacheOnWhenBatchBufferIsExecutedThenItWorksCorrectly) {
    auto writeAddress = this->helperSurface->getGpuAddress();
    auto writeValue = 7u;

    using BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->secondLevelBatch->getGpuAddress());
    batchBufferStart->setEnableCommandCache(1u);
    batchBufferStart->setNestedLevelBatchBuffer(BATCH_BUFFER_START::NESTED_LEVEL_BATCH_BUFFER::NESTED_LEVEL_BATCH_BUFFER_NESTED);

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    auto batchBufferEnd = reinterpret_cast<BATCH_BUFFER_END *>(this->secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_END)));
    batchBufferEnd->init();

    this->flushStream();
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}
using MultiLevelBatchTestsWithoutNesting = Test<MultiLevelBatchAubFixture<false>>;

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithoutNesting, givenConditionalBBEndWhenExitsFromSecondLevelToRingThenFirstLevelIsNotExecuted) {
    auto writeAddress = this->helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->secondLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->thirdLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(0);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    this->flushStream();

    writeAddress = this->helperSurface->getGpuAddress() + sizeof(uint64_t);
    auto zeroValue = 0llu;

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, sizeof(zeroValue));
    writeAddress += sizeof(uint64_t);
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, sizeof(zeroValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, MultiLevelBatchTestsWithoutNesting, givenConditionalBBEndWhenItExitsFromSecondLevelThenUpperLevelIsResumed) {
    auto writeAddress = this->helperSurface->getGpuAddress();
    auto compareAddress = writeAddress;

    using CONDITIONAL_BATCH_BUFFER_END = typename FamilyType::MI_CONDITIONAL_BATCH_BUFFER_END;
    using BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    auto batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->taskStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->secondLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    batchBufferStart = reinterpret_cast<BATCH_BUFFER_START *>(this->secondLevelBatchStream->getSpace(sizeof(BATCH_BUFFER_START)));
    batchBufferStart->init();
    batchBufferStart->setBatchBufferStartAddress(this->thirdLevelBatch->getGpuAddress());
    batchBufferStart->setSecondLevelBatchBuffer(BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);

    auto conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->thirdLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    auto writeValue = 7u;

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->secondLevelBatchStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    conditionalBatchBufferEnd = reinterpret_cast<CONDITIONAL_BATCH_BUFFER_END *>(this->secondLevelBatchStream->getSpace(sizeof(CONDITIONAL_BATCH_BUFFER_END)));
    conditionalBatchBufferEnd->init();
    conditionalBatchBufferEnd->setCompareAddress(compareAddress);
    conditionalBatchBufferEnd->setEndCurrentBatchBufferLevel(1);
    conditionalBatchBufferEnd->setCompareSemaphore(1);

    writeAddress += sizeof(uint64_t);
    writeValue++;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    this->flushStream();

    writeAddress = this->helperSurface->getGpuAddress() + sizeof(uint64_t);
    writeValue = 7u;
    auto zeroValue = 0llu;

    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, sizeof(zeroValue));
    writeAddress += sizeof(uint64_t);
    writeValue++;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenBlockingAtomicOperationIncOnLocalMemoryWhenItIsExecutedThenOperationUpdatesMemory) {
    this->runMiAtomic<FamilyType>(*this->helperSurface, true, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, atomicInitialValue + 1);
}

HWTEST2_F(AubWalkerPartitionZeroTest, givenBlockingAtomicIncOnSystemMemoryWhenExecutedThenMemoryIsUpdated, SystemMemoryAtomicsMatcher) {
    this->runMiAtomic<FamilyType>(*this->systemSurface, true, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, atomicInitialValue + 1);
}

HWTEST2_F(AubWalkerPartitionZeroTest, givenNonBlockingAtomicIncOnSystemMemoryWhenExecutedThenMemoryIsUpdated, SystemMemoryAtomicsMatcher) {
    this->runMiAtomic<FamilyType>(*this->systemSurface, false, WalkerPartition::MI_ATOMIC<FamilyType>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT, atomicInitialValue + 1);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenPredicatedCommandBufferWhenItIsExecutedThenAtomicIsIncrementedEquallyToPartitionCountPlusOne) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PostSyncType = decltype(FamilyType::template getPostSyncType<DefaultWalkerType>());

    auto streamCpuPointer = this->taskStream->getSpace(0);
    auto postSyncAddress = this->helperSurface->getGpuAddress();

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
    testArgs.dcFlushEnable = NEO::MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, this->neoDevice->getRootDeviceEnvironment());

    WalkerPartition::constructDynamicallyPartitionedCommandBuffer<FamilyType>(
        streamCpuPointer,
        nullptr,
        this->taskStream->getGraphicsAllocation()->getGpuAddress(),
        &walkerCmd,
        totalBytesProgrammed,
        testArgs,
        *this->neoDevice);
    this->taskStream->getSpace(totalBytesProgrammed);
    this->flushStream();
    auto expectedGpuAddress = this->taskStream->getGraphicsAllocation()->getGpuAddress() +
                              WalkerPartition::computeControlSectionOffset<FamilyType, DefaultWalkerType>(testArgs);

    uint32_t expectedValue = 17u;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddress), &expectedValue, sizeof(expectedValue));
    uint32_t expectedTileValue = 1u;
    this->expectMemory<FamilyType>(reinterpret_cast<void *>(expectedGpuAddress + 4llu), &expectedTileValue, sizeof(expectedTileValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenGeneralPurposeRegisterWhenItIsLoadedAndFetchedThenItIsNotPrivileged) {
    auto writeAddress = this->helperSurface->getGpuAddress();
    uint32_t writeValue = 7u;

    auto streamCpuPointer = this->taskStream->getSpace(0);
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
    this->taskStream->getSpace(totalBytesProgrammed);
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
        *this->taskStream, PostSyncMode::immediateData,
        writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

    streamCpuPointer = this->taskStream->getSpace(0);
    totalBytesProgrammed = 0u;
    WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, false);
    this->taskStream->getSpace(totalBytesProgrammed);
    this->flushStream();
    this->expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, sizeof(writeValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenPredicationWhenItIsOnThenCommandMustNotBeExecuted) {
    auto streamCpuPointer = this->taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    auto writeValue = 1u;
    auto zeroValue = 0u;
    auto addressShift = 8u;
    auto writeAddress = this->helperSurface->getGpuAddress();

    WalkerPartition::programWparidMask<FamilyType>(streamCpuPointer, totalBytesProgrammed, 16u);
    streamCpuPointer = this->taskStream->getSpace(totalBytesProgrammed);
    for (uint32_t wparid = 0u; wparid < 20; wparid++) {
        totalBytesProgrammed = 0;
        streamCpuPointer = this->taskStream->getSpace(0);
        WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, WalkerPartition::wparidCCSOffset, totalBytesProgrammed, wparid);
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, true);
        this->taskStream->getSpace(totalBytesProgrammed);
        PipeControlArgs args;
        MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
            *this->taskStream, PostSyncMode::immediateData,
            writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

        streamCpuPointer = this->taskStream->getSpace(0);
        totalBytesProgrammed = 0;
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, false);
        this->taskStream->getSpace(totalBytesProgrammed);

        writeAddress += addressShift;
        writeValue++;
    }

    this->flushStream();
    writeAddress = this->helperSurface->getGpuAddress();
    writeValue = 1u;
    for (uint32_t wparid = 0u; wparid < 20; wparid++) {
        if (wparid < 16) {
            this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, 4u);
        } else {
            this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, 4u);
        }
        writeAddress += addressShift;
        writeValue++;
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWalkerPartitionZeroTest, givenPredicationWhenItIsOnThenPipeControlInWparidIsNotExecuted) {
    auto streamCpuPointer = this->taskStream->getSpace(0);
    uint32_t totalBytesProgrammed = 0u;
    auto writeValue = 1u;
    auto zeroValue = 0u;
    auto addressShift = 32u;
    auto writeAddress = this->helperSurface->getGpuAddress();

    WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, WalkerPartition::addressOffsetCCSOffset, totalBytesProgrammed, addressShift);
    WalkerPartition::programWparidMask<FamilyType>(streamCpuPointer, totalBytesProgrammed, 8u);
    streamCpuPointer = this->taskStream->getSpace(totalBytesProgrammed);
    for (uint32_t wparid = 0u; wparid < 13; wparid++) {
        totalBytesProgrammed = 0;
        streamCpuPointer = this->taskStream->getSpace(0);
        WalkerPartition::programRegisterWithValue<FamilyType>(streamCpuPointer, WalkerPartition::wparidCCSOffset, totalBytesProgrammed, wparid);
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, true);
        this->taskStream->getSpace(totalBytesProgrammed);

        void *pipeControlAddress = this->taskStream->getSpace(0);
        PipeControlArgs args;
        MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(
            *this->taskStream, PostSyncMode::immediateData,
            writeAddress, writeValue, this->neoDevice->getRootDeviceEnvironment(), args);

        auto pipeControl = genCmdCast<typename FamilyType::PIPE_CONTROL *>(pipeControlAddress);
        ASSERT_NE(nullptr, pipeControl);
        pipeControl->setWorkloadPartitionIdOffsetEnable(true);
        streamCpuPointer = this->taskStream->getSpace(0);
        totalBytesProgrammed = 0;
        WalkerPartition::programWparidPredication<FamilyType>(streamCpuPointer, totalBytesProgrammed, false);
        this->taskStream->getSpace(totalBytesProgrammed);

        writeValue++;
    }

    this->flushStream();
    writeAddress = this->helperSurface->getGpuAddress();
    writeValue = 1u;
    for (uint32_t wparid = 0u; wparid < 13; wparid++) {
        if (wparid < 8) {
            this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &writeValue, 4u);
        } else {
            this->expectMemory<FamilyType>(reinterpret_cast<void *>(writeAddress), &zeroValue, 4u);
        }
        writeAddress += addressShift;
        writeValue++;
    }
}

HWCMDTEST_P(IGFX_XE_HP_CORE, AubWalkerPartitionTest, whenPartitionsAreUsedWithVariousInputsThenHardwareProgrammingIsCorrect) {
    uint64_t postSyncAddress = 0;
    this->dispatchPartitionedKernelWithTimestampEvent(postSyncAddress);

    this->validatePartitionProgramming<FamilyType>(postSyncAddress, this->partitionCount);
}

INSTANTIATE_TEST_SUITE_P(
    AUBWPARID,
    AubWalkerPartitionTest,
    ::testing::Combine(
        ::testing::ValuesIn(walkerPartitionTestPartitionCount),
        ::testing::ValuesIn(walkerPartitionTestPartitionType),
        ::testing::ValuesIn(walkerPartitionTestDispatchParameters),
        ::testing::ValuesIn(walkerPartitionTestWorkingDimensions)));

using AubWparidTests = Test<AubWalkerPartitionFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, AubWparidTests, whenPartitionCountSetAndPartitionIdSpecifiedViaWPARIDThenProvideEqualNumberWalkers) {
    this->workingDimensions = 3;
    this->dispatchParameters.globalWorkSize[0] = 30;
    this->dispatchParameters.globalWorkSize[1] = 39;
    this->dispatchParameters.globalWorkSize[2] = 5;
    this->dispatchParameters.localWorkSize[0] = 10;
    this->dispatchParameters.localWorkSize[1] = 3;
    this->dispatchParameters.localWorkSize[2] = 1;

    this->partitionType = 3;
    this->partitionCount = 4;

    ASSERT_NO_FATAL_FAILURE(this->createKernelWithOutputBuffer());
    debugManager.flags.ExperimentalSetWalkerPartitionType.set(this->partitionType);
    debugManager.flags.ExperimentalSetWalkerPartitionCount.set(this->partitionCount);
    debugManager.flags.EnableWalkerPartition.set(1u);

    uint64_t postSyncAddress = 0;
    this->dispatchPartitionedKernelWithTimestampEvent(postSyncAddress);

    this->validatePartitionProgramming<FamilyType>(postSyncAddress, this->partitionCount);
}

} // namespace ult
} // namespace L0
