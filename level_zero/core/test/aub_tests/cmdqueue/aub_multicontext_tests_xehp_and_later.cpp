/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/aub_tests/cmdqueue/multicontext_cmdqueue_l0_aub_fixture.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"

#include "multitile_matchers.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

namespace L0 {
namespace ult {

template <uint32_t numberOfTilesInTest, NEO::MulticontextAubFixture::EnabledCommandStreamers enabledCommandStreamers>
struct MultitileMulticontextL0Fixture : public MulticontextCmdQueueL0AubFixture {
    void setUp() {
        MulticontextCmdQueueL0AubFixture::setUp(numberOfTilesInTest, enabledCommandStreamers, false);
    }

    template <typename FamilyType>
    void runAubTest() {
        constexpr size_t bufferSize = 64 * MemoryConstants::kiloByte;
        auto writePattern = static_cast<uint8_t *>(allocHost(bufferSize));
        std::fill(writePattern, writePattern + bufferSize, 1);

        std::vector<std::vector<void *>> regularBuffers(this->queues.size());
        std::vector<std::vector<void *>> tileOnlyBuffers(this->queues.size());

        for (uint32_t tile = 0; tile < this->queues.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < this->queues[tile].size(); tileEngine++) {
                auto regularBuffer = allocDevice(this->rootDevice, bufferSize);
                auto tileOnlyBuffer = allocDevice(getTileDevice(tile), bufferSize);
                regularBuffers[tile].push_back(regularBuffer);
                tileOnlyBuffers[tile].push_back(tileOnlyBuffer);

                auto &entry = this->queues[tile][tileEngine];
                EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, regularBuffer, writePattern, bufferSize, nullptr, 0, nullptr));
                EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, tileOnlyBuffer, writePattern, bufferSize, nullptr, 0, nullptr));
                closeAndExecute(entry);
            }
        }

        for (uint32_t tile = 0; tile < this->queues.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < this->queues[tile].size(); tileEngine++) {
                synchronize(this->queues[tile][tileEngine]);
                this->template getSimulatedCsr<FamilyType>(tile, tileEngine)->pollForCompletion();

                this->template expectMemory<FamilyType>(regularBuffers[tile][tileEngine], writePattern, bufferSize, tile, tileEngine);
                this->template expectMemory<FamilyType>(tileOnlyBuffers[tile][tileEngine], writePattern, bufferSize, tile, tileEngine);
            }
        }
    }

    template <typename FamilyType>
    void runAubWriteImageTest() {
        if (!this->rootDevice->getNEODevice()->getDeviceInfo().imageSupport) {
            GTEST_SKIP();
        }

        constexpr uint32_t testWidth = 5;
        constexpr uint32_t testHeight = 5;
        constexpr uint32_t testDepth = 1;
        constexpr size_t numPixels = testWidth * testHeight * testDepth;
        constexpr size_t elementSize = 4 * sizeof(float);
        constexpr size_t imageDataSize = numPixels * elementSize;

        auto srcMemory = static_cast<uint8_t *>(allocHost(imageDataSize));
        for (size_t i = 0; i < imageDataSize; ++i) {
            srcMemory[i] = static_cast<uint8_t>(i);
        }

        ze_image_desc_t imageDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
        imageDesc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;
        imageDesc.type = ZE_IMAGE_TYPE_2D;
        imageDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                            ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                            ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};
        imageDesc.width = testWidth;
        imageDesc.height = testHeight;
        imageDesc.depth = testDepth;
        imageDesc.arraylevels = 0;
        imageDesc.miplevels = 0;

        ze_image_region_t region = {0, 0, 0, testWidth, testHeight, testDepth};

        std::vector<std::vector<ze_image_handle_t>> images(this->queues.size());
        std::vector<std::vector<uint8_t *>> dstMemories(this->queues.size());

        for (uint32_t tile = 0; tile < this->queues.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < this->queues[tile].size(); tileEngine++) {
                ze_image_handle_t dstImage = nullptr;
                EXPECT_EQ(ZE_RESULT_SUCCESS, zeImageCreate(this->contextHandle, this->rootDevice->toHandle(), &imageDesc, &dstImage));
                ASSERT_NE(nullptr, dstImage);
                images[tile].push_back(dstImage);

                auto &entry = this->queues[tile][tileEngine];
                EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(entry.commandList, dstImage, srcMemory, &region, nullptr, 0, nullptr));
            }
        }

        for (uint32_t tile = 0; tile < this->queues.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < this->queues[tile].size(); tileEngine++) {
                closeAndExecute(this->queues[tile][tileEngine]);
            }
        }

        for (uint32_t tile = 0; tile < this->queues.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < this->queues[tile].size(); tileEngine++) {
                auto &entry = this->queues[tile][tileEngine];
                synchronize(entry);
                EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListReset(entry.commandList));

                auto dstMemory = static_cast<uint8_t *>(allocHost(imageDataSize));
                memset(dstMemory, 0xFF, imageDataSize);
                dstMemories[tile].push_back(dstMemory);

                EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(entry.commandList, dstMemory, images[tile][tileEngine], &region, nullptr, 0, nullptr));
                closeAndExecute(entry);
                synchronize(entry);

                this->template expectMemory<FamilyType>(dstMemory, srcMemory, imageDataSize, tile, tileEngine);
            }
        }

        for (auto &tileImages : images) {
            for (auto image : tileImages) {
                zeImageDestroy(image);
            }
        }
    }
};

using FourTilesAllContextsL0AubTest = Test<MultitileMulticontextL0Fixture<4, NEO::MulticontextAubFixture::EnabledCommandStreamers::all>>;
HWTEST2_F(FourTilesAllContextsL0AubTest, GENERATEONLY_givenFourTilesAndAllContextsWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using FourTilesDualContextL0AubTest = Test<MultitileMulticontextL0Fixture<4, NEO::MulticontextAubFixture::EnabledCommandStreamers::dual>>;
HWTEST2_F(FourTilesDualContextL0AubTest, HEAVY_givenFourTilesAndDualContextWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using FourTilesSingleContextL0AubTest = Test<MultitileMulticontextL0Fixture<4, NEO::MulticontextAubFixture::EnabledCommandStreamers::single>>;
HWTEST2_F(FourTilesSingleContextL0AubTest, givenFourTilesAndSingleContextWhenSubmittingThenDataIsValid, SupportsMultiTile) {
    runAubTest<FamilyType>();
}

struct WalkerPartitionFourTilesL0Fixture : public MultitileMulticontextL0Fixture<4, NEO::MulticontextAubFixture::EnabledCommandStreamers::single> {
    using BaseClass = MultitileMulticontextL0Fixture<4, NEO::MulticontextAubFixture::EnabledCommandStreamers::single>;

    void setUp() {
        MulticontextCmdQueueL0AubFixture::setUp(4, NEO::MulticontextAubFixture::EnabledCommandStreamers::single, true);
        if (::testing::Test::IsSkipped()) {
            return;
        }
        if (!this->rootDevice->isImplicitScalingCapable()) {
            GTEST_SKIP();
        }

        EXPECT_EQ(4u, this->getRootCsr()->getOsContext().getNumSupportedDevices());

        this->rootQueue = createQueue(this->rootDevice, NEO::EngineGroupType::compute, 0);
        this->additionalQueues.push_back(this->rootQueue);
        ASSERT_NE(nullptr, this->rootQueue.csr);
        EXPECT_EQ(4u, this->rootQueue.csr->getOsContext().getNumSupportedDevices());

        this->buffer = allocDevice(this->rootDevice, bufferSize);
        auto zeroMemory = allocHost(bufferSize);
        memset(zeroMemory, 0, bufferSize);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(this->rootQueue.commandList, this->buffer, zeroMemory, bufferSize, nullptr, 0, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(this->rootQueue.commandList, nullptr, 0, nullptr));
    }

    void tearDown() {
        for (auto kernel : this->kernels) {
            zeKernelDestroy(kernel);
        }
        if (this->module) {
            zeModuleDestroy(this->module);
        }
        BaseClass::tearDown();
    }

    ze_kernel_handle_t createKernel(const char *kernelName) {
        if (!this->module) {
            this->module = AUBFixtureL0::createModuleFromFile("simple_kernels", this->contextHandle, this->rootDevice->toHandle(), "", true);
        }
        if (!this->module) {
            return nullptr;
        }
        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = kernelName;
        ze_kernel_handle_t kernel = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelCreate(this->module, &kernelDesc, &kernel));
        if (kernel) {
            this->kernels.push_back(kernel);
        }
        return kernel;
    }

    template <typename FamilyType>
    void runKernelWithoutAtomicTest() {
        constexpr uint32_t gwsSize = 1024;
        constexpr uint32_t lwsSize = 32;
        constexpr uint32_t workgroupCount = gwsSize / lwsSize;
        uint32_t kernelIncrementCounter = 1024;

        auto kernel = createKernel("simple_kernel_3");
        ASSERT_NE(nullptr, kernel);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, lwsSize, 1, 1));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &this->buffer));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 1, sizeof(kernelIncrementCounter), &kernelIncrementCounter));

        ze_group_count_t groupCount = {workgroupCount, 1, 1};
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(this->rootQueue.commandList, kernel, &groupCount, nullptr, 0, nullptr));
        closeAndExecute(this->rootQueue);
        synchronize(this->rootQueue);

        std::array<uint32_t, workgroupCount> workgroupCounts;
        std::fill(workgroupCounts.begin(), workgroupCounts.end(), kernelIncrementCounter);
        EXPECT_TRUE(this->rootQueue.csr->expectMemory(this->buffer, workgroupCounts.data(), workgroupCounts.size() * sizeof(uint32_t), aub_stream::CompareOperationValues::CompareEqual));
    }

    static constexpr size_t bufferSize = 16 * MemoryConstants::kiloByte;
    QueueEntry rootQueue = {};
    void *buffer = nullptr;
    ze_module_handle_t module = nullptr;
    std::vector<ze_kernel_handle_t> kernels;
};

struct DynamicWalkerPartitionFourTilesL0Fixture : public WalkerPartitionFourTilesL0Fixture {
    void setUp() {
        debugManager.flags.EnableStaticPartitioning.set(0);
        WalkerPartitionFourTilesL0Fixture::setUp();
    }
};

using DynamicWalkerPartitionFourTilesL0AubTests = Test<DynamicWalkerPartitionFourTilesL0Fixture>;

HWTEST2_F(DynamicWalkerPartitionFourTilesL0AubTests, whenWalkerPartitionIsEnabledForKernelWithAtomicThenOutputDataIsValid, SupportsMultiTile) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    constexpr uint32_t gwsSize = 512;
    constexpr uint32_t lwsSize = 32;
    constexpr uint32_t workgroupCount = gwsSize / lwsSize;

    debugManager.flags.ProgramWalkerPartitionSelfCleanup.set(0);

    auto kernel = createKernel("simple_kernel_5");
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetGroupSize(kernel, lwsSize, 1, 1));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &buffer));

    auto &cmdStream = *L0::CommandList::fromHandle(rootQueue.commandList)->getCmdContainer().getCommandStream();
    auto streamOffset = cmdStream.getUsed();

    ze_group_count_t groupCount = {workgroupCount, 1, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendLaunchKernel(rootQueue.commandList, kernel, &groupCount, nullptr, 0, nullptr));

    NEO::HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, streamOffset);

    bool lastSemaphoreFound = false;
    uint64_t tileAtomicGpuAddress = 0;
    for (auto it = hwParser.cmdList.rbegin(); it != hwParser.cmdList.rend(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            if (NEO::UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*semaphoreCmd)) {
                continue;
            }
            EXPECT_EQ(4u, NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitData(semaphoreCmd));
            tileAtomicGpuAddress = NEO::UnitTestHelper<FamilyType>::getSemaphoreWaitAddress(semaphoreCmd);
            lastSemaphoreFound = true;
            break;
        }
    }

    if (NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired()) {
        EXPECT_TRUE(lastSemaphoreFound);
        EXPECT_NE(0u, tileAtomicGpuAddress);
    } else {
        EXPECT_FALSE(lastSemaphoreFound);
        EXPECT_EQ(0u, tileAtomicGpuAddress);
    }

    closeAndExecute(rootQueue);
    synchronize(rootQueue);

    EXPECT_TRUE(rootQueue.csr->expectMemory(buffer, &gwsSize, sizeof(uint32_t), aub_stream::CompareOperationValues::CompareEqual));
    const uint32_t expectedAtomicValue = 4;
    if (NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired()) {
        EXPECT_TRUE(rootQueue.csr->expectMemory(reinterpret_cast<void *>(tileAtomicGpuAddress), &expectedAtomicValue, sizeof(uint32_t), aub_stream::CompareOperationValues::CompareEqual));
    }

    std::array<uint32_t, workgroupCount> workgroupCounts;
    std::fill(workgroupCounts.begin(), workgroupCounts.end(), lwsSize);
    EXPECT_TRUE(rootQueue.csr->expectMemory(ptrOffset(buffer, sizeof(uint32_t)), workgroupCounts.data(), workgroupCounts.size() * sizeof(uint32_t), aub_stream::CompareOperationValues::CompareEqual));
}

HWTEST2_F(DynamicWalkerPartitionFourTilesL0AubTests, whenWalkerPartitionIsEnabledForKernelWithoutAtomicThenOutputDataIsValid, SupportsMultiTile) {
    runKernelWithoutAtomicTest<FamilyType>();
}

struct StaticWalkerPartitionFourTilesL0Fixture : public WalkerPartitionFourTilesL0Fixture {
    void setUp() {
        debugManager.flags.EnableStaticPartitioning.set(1);
        WalkerPartitionFourTilesL0Fixture::setUp();
    }

    std::unique_ptr<NEO::LinearStream> createTaskStream() {
        auto neoRootDevice = this->rootDevice->getNEODevice();
        const NEO::AllocationProperties commandStreamAllocationProperties{neoRootDevice->getRootDeviceIndex(),
                                                                          true,
                                                                          MemoryConstants::pageSize,
                                                                          NEO::AllocationType::commandBuffer,
                                                                          true,
                                                                          false,
                                                                          neoRootDevice->getDeviceBitfield()};
        auto streamAllocation = neoRootDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
        return std::make_unique<NEO::LinearStream>(streamAllocation);
    }

    void destroyTaskStream(NEO::LinearStream &stream) {
        this->rootDevice->getNEODevice()->getMemoryManager()->freeGraphicsMemory(stream.getGraphicsAllocation());
    }

    void flushTaskStream(NEO::LinearStream &stream) {
        auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.guardCommandBufferWithPipeControl = true;

        auto rootCsr = this->getRootCsr();
        rootCsr->flushTask(stream, 0,
                           &rootCsr->getIndirectHeap(NEO::IndirectHeap::Type::dynamicState, 0u),
                           &rootCsr->getIndirectHeap(NEO::IndirectHeap::Type::indirectObject, 0u),
                           &rootCsr->getIndirectHeap(NEO::IndirectHeap::Type::surfaceState, 0u),
                           0u, dispatchFlags, *this->rootDevice->getNEODevice());

        rootCsr->flushBatchedSubmissions();
        rootCsr->pollForCompletion();
    }

    template <typename FamilyType>
    void runStaticPartitioningSyncTest(bool synchronizeBeforeExecution, uint32_t expectedPreWalkerSyncValue, uint32_t expectedPostWalkerSyncValue) {
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

        auto taskStream = createTaskStream();
        auto taskStreamCpu = taskStream->getSpace(0);
        auto taskStreamGpu = taskStream->getGraphicsAllocation()->getGpuAddress();

        uint32_t totalBytesProgrammed = 0u;
        DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
        walkerCmd.setPartitionType(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X);
        walkerCmd.getInterfaceDescriptor().setNumberOfThreadsInGpgpuThreadGroup(1u);
        if constexpr (FamilyType::template isHeaplessMode<DefaultWalkerType>()) {
            walkerCmd.setMaximumNumberOfThreads(64);
        }

        WalkerPartition::WalkerPartitionArgs testArgs = {};
        testArgs.initializeWparidRegister = true;
        testArgs.crossTileAtomicSynchronization = true;
        testArgs.emitPipeControlStall = true;
        testArgs.tileCount = static_cast<uint32_t>(this->rootDevice->getNEODevice()->getDeviceBitfield().count());
        testArgs.partitionCount = testArgs.tileCount;
        testArgs.synchronizeBeforeExecution = synchronizeBeforeExecution;
        testArgs.secondaryBatchBuffer = false;
        testArgs.emitSelfCleanup = false;
        testArgs.staticPartitioning = true;
        testArgs.workPartitionAllocationGpuVa = this->getRootCsr()->getWorkPartitionAllocationGpuAddress();
        testArgs.dcFlushEnable = this->getRootCsr()->getDcFlushSupport();

        WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(
            taskStreamCpu,
            nullptr,
            taskStreamGpu,
            &walkerCmd,
            totalBytesProgrammed,
            testArgs,
            *this->rootDevice->getNEODevice());
        taskStream->getSpace(totalBytesProgrammed);
        flushTaskStream(*taskStream);

        const auto controlSectionAddress = taskStreamGpu + WalkerPartition::computeStaticPartitioningControlSectionOffset<FamilyType, DefaultWalkerType>(testArgs);
        const auto preWalkerSyncAddress = controlSectionAddress + offsetof(WalkerPartition::StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
        const auto postWalkerSyncAddress = controlSectionAddress + offsetof(WalkerPartition::StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
        EXPECT_TRUE(this->getRootCsr()->expectMemory(reinterpret_cast<void *>(preWalkerSyncAddress), &expectedPreWalkerSyncValue, sizeof(expectedPreWalkerSyncValue), aub_stream::CompareOperationValues::CompareEqual));
        EXPECT_TRUE(this->getRootCsr()->expectMemory(reinterpret_cast<void *>(postWalkerSyncAddress), &expectedPostWalkerSyncValue, sizeof(expectedPostWalkerSyncValue), aub_stream::CompareOperationValues::CompareEqual));

        destroyTaskStream(*taskStream);
    }
};

using StaticWalkerPartitionFourTilesL0AubTests = Test<StaticWalkerPartitionFourTilesL0Fixture>;

HWTEST2_F(StaticWalkerPartitionFourTilesL0AubTests, givenFourTilesWhenStaticWalkerPartitionIsEnabledForKernelThenOutputDataIsValid, SupportsMultiTile) {
    runKernelWithoutAtomicTest<FamilyType>();
}

HWTEST2_F(StaticWalkerPartitionFourTilesL0AubTests, givenPreWalkerSyncWhenStaticWalkerPartitionIsThenAtomicsAreIncrementedCorrectly, SupportsMultiTile) {
    runStaticPartitioningSyncTest<FamilyType>(true, 4u, 4u);
}

HWTEST2_F(StaticWalkerPartitionFourTilesL0AubTests, whenNoPreWalkerSyncThenAtomicsAreIncrementedCorrectly, SupportsMultiTile) {
    runStaticPartitioningSyncTest<FamilyType>(false, 0u, 4u);
}

using TwoTilesAllContextsL0AubTest = Test<MultitileMulticontextL0Fixture<2, NEO::MulticontextAubFixture::EnabledCommandStreamers::all>>;
HWTEST2_F(TwoTilesAllContextsL0AubTest, HEAVY_givenTwoTilesAndAllContextsWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using TwoTilesDualContextL0AubTest = Test<MultitileMulticontextL0Fixture<2, NEO::MulticontextAubFixture::EnabledCommandStreamers::dual>>;
HWTEST2_F(TwoTilesDualContextL0AubTest, givenTwoTilesAndDualContextWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using TwoTilesSingleContextL0AubTest = Test<MultitileMulticontextL0Fixture<2, NEO::MulticontextAubFixture::EnabledCommandStreamers::single>>;
HWTEST2_F(TwoTilesSingleContextL0AubTest, givenTwoTilesAndSingleContextWhenSubmittingThenDataIsValid, SupportsMultiTile) {
    runAubTest<FamilyType>();
}

using SingleTileAllContextsL0AubTest = Test<MultitileMulticontextL0Fixture<1, NEO::MulticontextAubFixture::EnabledCommandStreamers::all>>;
HWTEST2_F(SingleTileAllContextsL0AubTest, GENERATEONLY_givenSingleTileAndAllContextsWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using SingleTileDualContextL0AubTest = Test<MultitileMulticontextL0Fixture<1, NEO::MulticontextAubFixture::EnabledCommandStreamers::dual>>;
HWTEST2_F(SingleTileDualContextL0AubTest, givenSingleTileAndDualContextWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

HWTEST2_F(SingleTileDualContextL0AubTest, givenSingleAllocationWhenUpdatedFromDifferentContextThenDataIsValid, IsPVC) {
    constexpr size_t bufferSize = 256;
    constexpr size_t halfBufferSize = bufferSize / 2;

    auto buffer = static_cast<uint8_t *>(allocDevice(rootDevice, bufferSize));
    auto writePattern1 = static_cast<uint8_t *>(allocHost(halfBufferSize));
    auto writePattern2 = static_cast<uint8_t *>(allocHost(halfBufferSize));
    std::fill(writePattern1, writePattern1 + halfBufferSize, 1);
    std::fill(writePattern2, writePattern2 + halfBufferSize, 2);

    auto &firstEntry = queues[0][0];
    auto &secondEntry = queues[0][1];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(firstEntry.commandList, buffer, writePattern1, halfBufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(secondEntry.commandList, buffer + halfBufferSize, writePattern2, halfBufferSize, nullptr, 0, nullptr));

    closeAndExecute(secondEntry);
    synchronize(secondEntry);
    closeAndExecute(firstEntry);
    synchronize(firstEntry);

    expectMemory<FamilyType>(buffer, writePattern1, halfBufferSize, 0, 0);
    expectMemory<FamilyType>(buffer + halfBufferSize, writePattern2, halfBufferSize, 0, 1);
}

HWTEST2_F(SingleTileDualContextL0AubTest, givenSingleTileAndDualContextWhenWritingImageThenDataIsValid, IsPVC) {
    runAubWriteImageTest<FamilyType>();
}

HWTEST2_F(SingleTileAllContextsL0AubTest, HEAVY_givenSingleTileAndAllContextsWhenWritingImageThenDataIsValid, IsPVC) {
    runAubWriteImageTest<FamilyType>();
}

HWTEST2_F(TwoTilesSingleContextL0AubTest, givenTwoTilesAndSingleContextWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

HWTEST2_F(TwoTilesDualContextL0AubTest, givenTwoTilesAndDualContextWhenWritingImageThenDataIsValid, IsPVC) {
    runAubWriteImageTest<FamilyType>();
}

HWTEST2_F(TwoTilesAllContextsL0AubTest, GENERATEONLY_givenTwoTilesAndAllContextsWhenWritingImageThenDataIsValid, IsPVC) {
    runAubWriteImageTest<FamilyType>();
}

HWTEST2_F(FourTilesSingleContextL0AubTest, givenFourTilesAndSingleContextWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

HWTEST2_F(FourTilesDualContextL0AubTest, GENERATEONLY_givenFourTilesAndDualContextWhenWritingImageThenDataIsValid, IsPVC) {
    runAubWriteImageTest<FamilyType>();
}

HWTEST2_F(FourTilesAllContextsL0AubTest, GENERATEONLY_givenFourTilesAndAllContextsWhenWritingImageThenDataIsValid, IsPVC) {
    runAubWriteImageTest<FamilyType>();
}

} // namespace ult
} // namespace L0
