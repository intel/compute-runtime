/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_ocl_aub_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "multitile_matchers.h"

using namespace NEO;

template <uint32_t numberOfTiles, MulticontextOclAubFixture::EnabledCommandStreamers enabledCommandStreamers>
struct MultitileMulticontextTests : public MulticontextOclAubFixture, public ::testing::Test {
    void SetUp() override {
        debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0);
        MulticontextOclAubFixture::setUp(numberOfTiles, enabledCommandStreamers, false);
    }
    void TearDown() override {
        MulticontextOclAubFixture::tearDown();
    }

    template <typename FamilyType>
    void runAubTest() {
        cl_int retVal = CL_SUCCESS;
        const uint32_t bufferSize = 64 * MemoryConstants::kiloByte;
        uint8_t writePattern[bufferSize];
        uint8_t initPattern[bufferSize];
        std::fill(writePattern, writePattern + sizeof(writePattern), 1);
        std::fill(initPattern, initPattern + sizeof(initPattern), 0);

        std::vector<std::vector<std::unique_ptr<Buffer>>> regularBuffers;
        std::vector<std::vector<std::unique_ptr<Buffer>>> tileOnlyBuffers;

        cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR;

        regularBuffers.resize(tileDevices.size());
        tileOnlyBuffers.resize(tileDevices.size());
        for (uint32_t tile = 0; tile < tileDevices.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < commandQueues[tile].size(); tileEngine++) {
                debugManager.flags.DoCpuCopyOnWriteBuffer.set(true);
                auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 0, 0,
                                                                                         &context->getDevice(0)->getDevice());
                auto regularBuffer = Buffer::create(
                    context.get(), memoryProperties, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 0, bufferSize, initPattern, retVal);
                auto tileOnlyProperties = ClMemoryPropertiesHelper::createMemoryProperties(
                    flags, 0, 0, context->getDevice(0)->getDevice().getNearestGenericSubDevice(tile));
                auto tileOnlyBuffer = Buffer::create(context.get(), tileOnlyProperties, flags, 0, bufferSize, initPattern, retVal);
                debugManager.flags.DoCpuCopyOnWriteBuffer.set(false);
                regularBuffer->forceDisallowCPUCopy = true;
                tileOnlyBuffer->forceDisallowCPUCopy = true;
                regularBuffers[tile].push_back(std::unique_ptr<Buffer>(regularBuffer));
                tileOnlyBuffers[tile].push_back(std::unique_ptr<Buffer>(tileOnlyBuffer));

                commandQueues[tile][tileEngine]->enqueueWriteBuffer(regularBuffer, CL_FALSE, 0, bufferSize, writePattern, nullptr, 0, nullptr, nullptr);
                commandQueues[tile][tileEngine]->enqueueWriteBuffer(tileOnlyBuffer, CL_FALSE, 0, bufferSize, writePattern, nullptr, 0, nullptr, nullptr);

                commandQueues[tile][tileEngine]->flush();
            }
        }

        for (uint32_t tile = 0; tile < tileDevices.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < commandQueues[tile].size(); tileEngine++) {
                getSimulatedCsr<FamilyType>(tile, tileEngine)->pollForCompletion();

                auto regularBufferGpuAddress = ptrOffset(regularBuffers[tile][tileEngine]->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), regularBuffers[tile][tileEngine]->getOffset());
                auto tileOnlyBufferGpuAddress = ptrOffset(tileOnlyBuffers[tile][tileEngine]->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress(), tileOnlyBuffers[tile][tileEngine]->getOffset());
                expectMemory<FamilyType>(addrToPtr(regularBufferGpuAddress), writePattern, bufferSize, tile, tileEngine);
                expectMemory<FamilyType>(addrToPtr(tileOnlyBufferGpuAddress), writePattern, bufferSize, tile, tileEngine);
            }
        }
    }

    template <typename FamilyType>
    void runAubWriteImageTest() {
        if (!tileDevices[0]->getSharedDeviceInfo().imageSupport) {
            GTEST_SKIP();
        }

        cl_int retVal = CL_SUCCESS;
        auto testWidth = 5u;
        auto testHeight = 5u;
        auto testDepth = 1u;
        auto numPixels = testWidth * testHeight * testDepth;

        cl_image_format imageFormat;
        imageFormat.image_channel_data_type = CL_FLOAT;
        imageFormat.image_channel_order = CL_RGBA;

        cl_mem_flags flags = 0;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

        cl_image_desc imageDesc;
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = testWidth;
        imageDesc.image_height = testHeight;
        imageDesc.image_depth = testDepth;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;

        auto perChannelDataSize = 4u;
        auto numChannels = 4u;
        auto elementSize = perChannelDataSize * numChannels;
        auto srcMemory = (uint8_t *)alignedMalloc(elementSize * numPixels, MemoryConstants::pageSize);
        for (size_t i = 0; i < numPixels * elementSize; ++i) {
            auto origValue = static_cast<uint8_t>(i);
            memcpy(srcMemory + i, &origValue, sizeof(origValue));
        }

        size_t origin[3] = {0, 0, 0};
        const size_t region[3] = {testWidth, testHeight, testDepth};
        size_t inputRowPitch = testWidth * elementSize;
        size_t inputSlicePitch = inputRowPitch * testHeight;

        std::vector<std::vector<std::unique_ptr<Image>>> images;
        images.resize(tileDevices.size());

        for (uint32_t tile = 0; tile < tileDevices.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < commandQueues[tile].size(); tileEngine++) {
                Image *dstImage = Image::create(
                    context.get(),
                    ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                    flags,
                    0,
                    surfaceFormat,
                    &imageDesc,
                    nullptr,
                    retVal);
                ASSERT_NE(nullptr, dstImage);
                memset(dstImage->getCpuAddress(), 0xFF, dstImage->getSize());

                retVal = commandQueues[tile][tileEngine]->enqueueWriteImage(
                    dstImage,
                    CL_FALSE,
                    origin,
                    region,
                    inputRowPitch,
                    inputSlicePitch,
                    srcMemory,
                    nullptr,
                    0,
                    nullptr,
                    nullptr);
                EXPECT_EQ(CL_SUCCESS, retVal);

                images[tile].push_back(std::unique_ptr<Image>(dstImage));
            }
        }

        for (uint32_t tile = 0; tile < tileDevices.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < commandQueues[tile].size(); tileEngine++) {
                commandQueues[tile][tileEngine]->flush();
            }
        }

        std::unique_ptr<uint8_t[]> dstMemory;

        for (uint32_t tile = 0; tile < tileDevices.size(); tile++) {
            for (uint32_t tileEngine = 0; tileEngine < commandQueues[tile].size(); tileEngine++) {

                dstMemory.reset(new uint8_t[images[tile][tileEngine]->getSize()]);
                memset(dstMemory.get(), 0xFF, images[tile][tileEngine]->getSize());

                commandQueues[tile][tileEngine]->enqueueReadImage(
                    images[tile][tileEngine].get(), CL_FALSE, origin, region, 0, 0, dstMemory.get(), nullptr, 0, nullptr, nullptr);

                commandQueues[tile][tileEngine]->flush();

                auto rowPitch = images[tile][tileEngine]->getHostPtrRowPitch();
                auto slicePitch = images[tile][tileEngine]->getHostPtrSlicePitch();

                auto pSrcMemory = srcMemory;
                auto pDstMemory = dstMemory.get();
                for (size_t z = 0; z < testDepth; ++z) {
                    for (size_t y = 0; y < testHeight; ++y) {
                        expectMemory<FamilyType>(pDstMemory, pSrcMemory, testWidth * elementSize, tile, tileEngine);
                        pSrcMemory = ptrOffset(pSrcMemory, testWidth * elementSize);
                        pDstMemory = ptrOffset(pDstMemory, rowPitch);
                    }
                    pDstMemory = ptrOffset(pDstMemory, slicePitch - (rowPitch * (testHeight > 0 ? testHeight : 1)));
                }
            }
        }

        alignedFree(srcMemory);
    }
};

// 4 Tiles
using FourTilesAllContextsTest = MultitileMulticontextTests<4, MulticontextOclAubFixture::EnabledCommandStreamers::all>;
HWTEST2_F(FourTilesAllContextsTest, GENERATEONLY_givenFourTilesAndAllContextsWhenSubmittingThenDataIsValid, SupportsMultiTile) {
    runAubTest<FamilyType>();
}

using FourTilesDualContextTest = MultitileMulticontextTests<4, MulticontextOclAubFixture::EnabledCommandStreamers::dual>;
HWTEST2_F(FourTilesDualContextTest, HEAVY_givenFourTilesAndDualContextWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using FourTilesSingleContextTest = MultitileMulticontextTests<4, MulticontextOclAubFixture::EnabledCommandStreamers::single>;
HWTEST2_F(FourTilesSingleContextTest, givenFourTilesAndSingleContextWhenSubmittingThenDataIsValid, SupportsMultiTile) {
    runAubTest<FamilyType>();
}

struct EnqueueWithWalkerPartitionFourTilesTests : public FourTilesSingleContextTest, SimpleKernelFixture {
    void SetUp() override {
        debugManager.flags.EnableWalkerPartition.set(1u);
        kernelIds |= (1 << 5);
        kernelIds |= (1 << 8);

        FourTilesSingleContextTest::SetUp();
        SimpleKernelFixture::setUp(rootDevice, context.get());

        rootCsr = rootDevice->getDefaultEngine().commandStreamReceiver;
        EXPECT_EQ(4u, rootCsr->getOsContext().getNumSupportedDevices());
        engineControlForFusedQueue = {rootCsr, &rootCsr->getOsContext()};

        bufferSize = 16 * MemoryConstants::kiloByte;

        auto destMemory = std::make_unique<uint8_t[]>(bufferSize);
        memset(destMemory.get(), 0x0, bufferSize);

        cl_int retVal = CL_SUCCESS;
        buffer.reset(Buffer::create(multiTileDefaultContext.get(), CL_MEM_COPY_HOST_PTR, bufferSize, destMemory.get(), retVal));

        clBuffer = buffer.get();
    }

    void TearDown() override {
        SimpleKernelFixture::tearDown();
        FourTilesSingleContextTest::TearDown();
    }

    void *getGpuAddress(Buffer &buffer) {
        return addrToPtr(ptrOffset(buffer.getGraphicsAllocation(this->rootDeviceIndex)->getGpuAddress(), buffer.getOffset()));
    }

    uint32_t bufferSize = 0;
    std::unique_ptr<Buffer> buffer;
    cl_mem clBuffer;
    EngineControl engineControlForFusedQueue = {};
    CommandStreamReceiver *rootCsr = nullptr;
};

struct DynamicWalkerPartitionFourTilesTests : EnqueueWithWalkerPartitionFourTilesTests {
    void SetUp() override {
        debugManager.flags.EnableStaticPartitioning.set(0);
        EnqueueWithWalkerPartitionFourTilesTests::SetUp();
    }
    DebugManagerStateRestore restore{};
};

HWTEST2_F(DynamicWalkerPartitionFourTilesTests, whenWalkerPartitionIsEnabledForKernelWithAtomicThenOutputDataIsValid, SupportsMultiTile) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    auto mockCommandQueue = new MockCommandQueueHw<FamilyType>(multiTileDefaultContext.get(), rootDevice, nullptr);

    commandQueues[0][0].reset(mockCommandQueue);

    constexpr size_t globalWorkOffset[] = {0, 0, 0};
    constexpr size_t gwsSize[] = {512, 1, 1};
    constexpr size_t lwsSize[] = {32, 1, 1};
    constexpr cl_uint workingDimensions = 1;
    cl_int retVal = CL_SUCCESS;

    kernels[5]->setArg(0, sizeof(cl_mem), &clBuffer);
    retVal = mockCommandQueue->enqueueKernel(kernels[5].get(), workingDimensions, globalWorkOffset, gwsSize, lwsSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    mockCommandQueue->flush();

    HardwareParse hwParser;
    auto &cmdStream = mockCommandQueue->getCS(0);
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    bool lastSemaphoreFound = false;
    uint64_t tileAtomicGpuAddress = 0;
    for (auto it = hwParser.cmdList.rbegin(); it != hwParser.cmdList.rend(); it++) {
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*it);
        if (semaphoreCmd) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*semaphoreCmd)) {
                continue;
            }
            EXPECT_EQ(4u, semaphoreCmd->getSemaphoreDataDword());
            tileAtomicGpuAddress = semaphoreCmd->getSemaphoreGraphicsAddress();
            lastSemaphoreFound = true;
            break;
        }
    }

    if (ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired()) {
        EXPECT_TRUE(lastSemaphoreFound);
        EXPECT_NE(0u, tileAtomicGpuAddress);
    } else {
        EXPECT_FALSE(lastSemaphoreFound);
        EXPECT_EQ(0u, tileAtomicGpuAddress);
    }

    expectMemory<FamilyType>(getGpuAddress(*buffer), &gwsSize[workingDimensions - 1], sizeof(uint32_t), 0, 0);
    uint32_t expectedAtomicValue = 4;
    if (ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired()) {
        expectMemory<FamilyType>(reinterpret_cast<void *>(tileAtomicGpuAddress), &expectedAtomicValue, sizeof(uint32_t), 0, 0);
    }

    constexpr uint32_t workgroupCount = static_cast<uint32_t>(gwsSize[workingDimensions - 1] / lwsSize[workingDimensions - 1]);
    auto groupSpecificWorkCounts = ptrOffset(getGpuAddress(*buffer), 4);
    std::array<uint32_t, workgroupCount> workgroupCounts;
    std::fill(workgroupCounts.begin(), workgroupCounts.end(), static_cast<uint32_t>(lwsSize[workingDimensions - 1]));

    expectMemory<FamilyType>(groupSpecificWorkCounts, &workgroupCounts[0], workgroupCounts.size() * sizeof(uint32_t), 0, 0);
}

HWTEST2_F(DynamicWalkerPartitionFourTilesTests, whenWalkerPartitionIsEnabledForKernelWithoutAtomicThenOutputDataIsValid, SupportsMultiTile) {

    auto mockCommandQueue = new MockCommandQueueHw<FamilyType>(multiTileDefaultContext.get(), rootDevice, nullptr);

    commandQueues[0][0].reset(mockCommandQueue);

    constexpr size_t globalWorkOffset[3] = {0, 0, 0};
    constexpr size_t gwsSize[3] = {1024, 1, 1};
    constexpr size_t lwsSize[3] = {32, 1, 1};
    constexpr cl_uint workingDimensions = 1;
    cl_uint kernelIncrementCounter = 1024;
    cl_int retVal = CL_SUCCESS;

    kernels[8]->setArg(0, sizeof(cl_mem), &clBuffer);
    kernels[8]->setArg(1, kernelIncrementCounter);
    retVal = mockCommandQueue->enqueueKernel(kernels[8].get(), workingDimensions, globalWorkOffset, gwsSize, lwsSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    mockCommandQueue->flush();

    constexpr uint32_t workgroupCount = static_cast<uint32_t>(gwsSize[workingDimensions - 1] / lwsSize[workingDimensions - 1]);
    std::array<uint32_t, workgroupCount> workgroupCounts;
    std::fill(workgroupCounts.begin(), workgroupCounts.end(), kernelIncrementCounter);

    expectMemory<FamilyType>(getGpuAddress(*buffer), &workgroupCounts[0], workgroupCounts.size() * sizeof(uint32_t), 0, 0);
}

struct StaticWalkerPartitionFourTilesTests : EnqueueWithWalkerPartitionFourTilesTests {
    void SetUp() override {
        debugManager.flags.EnableStaticPartitioning.set(1);
        debugManager.flags.EnableBlitterOperationsSupport.set(1);
        EnqueueWithWalkerPartitionFourTilesTests::SetUp();
    }

    std::unique_ptr<LinearStream> createTaskStream() {
        const AllocationProperties commandStreamAllocationProperties{rootDevice->getRootDeviceIndex(),
                                                                     true,
                                                                     MemoryConstants::pageSize,
                                                                     AllocationType::commandBuffer,
                                                                     true,
                                                                     false,
                                                                     rootDevice->getDeviceBitfield()};
        GraphicsAllocation *streamAllocation = rootDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
        return std::make_unique<LinearStream>(streamAllocation);
    }

    void destroyTaskStream(LinearStream &stream) {
        rootDevice->getMemoryManager()->freeGraphicsMemory(stream.getGraphicsAllocation());
    }

    void flushTaskStream(LinearStream &stream) {
        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
        dispatchFlags.guardCommandBufferWithPipeControl = true;

        rootCsr->flushTask(stream, 0,
                           &rootCsr->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u),
                           &rootCsr->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u),
                           &rootCsr->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u),
                           0u, dispatchFlags, rootDevice->getDevice());

        rootCsr->flushBatchedSubmissions();
    }

    template <typename FamilyType>
    void expectMemoryOnRootCsr(void *gfxAddress, const void *srcAddress, size_t length) {
        auto csr = static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(rootCsr);
        csr->expectMemoryEqual(gfxAddress, srcAddress, length);
    }

    DebugManagerStateRestore restore{};
};

HWTEST2_F(StaticWalkerPartitionFourTilesTests, givenFourTilesWhenStaticWalkerPartitionIsEnabledForKernelThenOutputDataIsValid, SupportsMultiTile) {

    auto mockCommandQueue = new MockCommandQueueHw<FamilyType>(multiTileDefaultContext.get(), rootDevice, nullptr);

    commandQueues[0][0].reset(mockCommandQueue);

    constexpr size_t globalWorkOffset[3] = {0, 0, 0};
    constexpr size_t gwsSize[3] = {1024, 1, 1};
    constexpr size_t lwsSize[3] = {32, 1, 1};
    constexpr cl_uint workingDimensions = 1;
    cl_uint kernelIncrementCounter = 1024;
    cl_int retVal = CL_SUCCESS;

    kernels[8]->setArg(0, sizeof(cl_mem), &clBuffer);
    kernels[8]->setArg(1, kernelIncrementCounter);
    retVal = mockCommandQueue->enqueueKernel(kernels[8].get(), workingDimensions, globalWorkOffset, gwsSize, lwsSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    mockCommandQueue->flush();

    constexpr uint32_t workgroupCount = static_cast<uint32_t>(gwsSize[workingDimensions - 1] / lwsSize[workingDimensions - 1]);
    std::array<uint32_t, workgroupCount> workgroupCounts;
    std::fill(workgroupCounts.begin(), workgroupCounts.end(), kernelIncrementCounter);

    expectMemoryOnRootCsr<FamilyType>(getGpuAddress(*buffer), &workgroupCounts[0], workgroupCounts.size() * sizeof(uint32_t));
}

HWTEST2_F(StaticWalkerPartitionFourTilesTests, givenPreWalkerSyncWhenStaticWalkerPartitionIsThenAtomicsAreIncrementedCorrectly, SupportsMultiTile) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto taskStream = createTaskStream();
    auto taskStreamCpu = taskStream->getSpace(0);
    auto taskStreamGpu = taskStream->getGraphicsAllocation()->getGpuAddress();

    uint32_t totalBytesProgrammed = 0u;
    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walkerCmd.setPartitionType(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X);
    walkerCmd.getInterfaceDescriptor().setNumberOfThreadsInGpgpuThreadGroup(1u);

    WalkerPartition::WalkerPartitionArgs testArgs = {};
    testArgs.initializeWparidRegister = true;
    testArgs.crossTileAtomicSynchronization = true;
    testArgs.emitPipeControlStall = true;
    testArgs.tileCount = static_cast<uint32_t>(rootDevice->getDeviceBitfield().count());
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.synchronizeBeforeExecution = true;
    testArgs.secondaryBatchBuffer = false;
    testArgs.emitSelfCleanup = false;
    testArgs.staticPartitioning = true;
    testArgs.workPartitionAllocationGpuVa = rootCsr->getWorkPartitionAllocationGpuAddress();
    testArgs.dcFlushEnable = rootCsr->getDcFlushSupport();

    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(
        taskStreamCpu,
        nullptr,
        taskStreamGpu,
        &walkerCmd,
        totalBytesProgrammed,
        testArgs,
        this->rootDevice->getDevice());
    taskStream->getSpace(totalBytesProgrammed);
    flushTaskStream(*taskStream);

    const auto controlSectionAddress = taskStreamGpu + WalkerPartition::computeStaticPartitioningControlSectionOffset<FamilyType, DefaultWalkerType>(testArgs);
    const auto preWalkerSyncAddress = controlSectionAddress + offsetof(WalkerPartition::StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = controlSectionAddress + offsetof(WalkerPartition::StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    uint32_t expectedValue = 0x4;
    expectMemoryOnRootCsr<FamilyType>(reinterpret_cast<void *>(preWalkerSyncAddress), &expectedValue, sizeof(expectedValue));
    expectMemoryOnRootCsr<FamilyType>(reinterpret_cast<void *>(postWalkerSyncAddress), &expectedValue, sizeof(expectedValue));

    destroyTaskStream(*taskStream);
}

HWTEST2_F(StaticWalkerPartitionFourTilesTests, whenNoPreWalkerSyncThenAtomicsAreIncrementedCorrectly, SupportsMultiTile) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    auto taskStream = createTaskStream();
    auto taskStreamCpu = taskStream->getSpace(0);
    auto taskStreamGpu = taskStream->getGraphicsAllocation()->getGpuAddress();

    uint32_t totalBytesProgrammed = 0u;
    DefaultWalkerType walkerCmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();
    walkerCmd.setPartitionType(DefaultWalkerType::PARTITION_TYPE::PARTITION_TYPE_X);
    walkerCmd.getInterfaceDescriptor().setNumberOfThreadsInGpgpuThreadGroup(1u);

    WalkerPartition::WalkerPartitionArgs testArgs = {};
    testArgs.initializeWparidRegister = true;
    testArgs.crossTileAtomicSynchronization = true;
    testArgs.emitPipeControlStall = true;
    testArgs.tileCount = static_cast<uint32_t>(rootDevice->getDeviceBitfield().count());
    testArgs.partitionCount = testArgs.tileCount;
    testArgs.synchronizeBeforeExecution = false;
    testArgs.secondaryBatchBuffer = false;
    testArgs.emitSelfCleanup = false;
    testArgs.staticPartitioning = true;
    testArgs.workPartitionAllocationGpuVa = rootCsr->getWorkPartitionAllocationGpuAddress();
    testArgs.dcFlushEnable = rootCsr->getDcFlushSupport();

    WalkerPartition::constructStaticallyPartitionedCommandBuffer<FamilyType>(
        taskStreamCpu,
        nullptr,
        taskStreamGpu,
        &walkerCmd,
        totalBytesProgrammed,
        testArgs,
        this->rootDevice->getDevice());
    taskStream->getSpace(totalBytesProgrammed);
    flushTaskStream(*taskStream);

    const auto controlSectionAddress = taskStreamGpu + WalkerPartition::computeStaticPartitioningControlSectionOffset<FamilyType, DefaultWalkerType>(testArgs);
    const auto preWalkerSyncAddress = controlSectionAddress + offsetof(WalkerPartition::StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
    const auto postWalkerSyncAddress = controlSectionAddress + offsetof(WalkerPartition::StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
    uint32_t expectedValue = 0x0;
    expectMemoryOnRootCsr<FamilyType>(reinterpret_cast<void *>(preWalkerSyncAddress), &expectedValue, sizeof(expectedValue));
    expectedValue = 0x4;
    expectMemoryOnRootCsr<FamilyType>(reinterpret_cast<void *>(postWalkerSyncAddress), &expectedValue, sizeof(expectedValue));

    destroyTaskStream(*taskStream);
}

// 2 Tiles
using TwoTilesAllContextsTest = MultitileMulticontextTests<2, MulticontextOclAubFixture::EnabledCommandStreamers::all>;
HWTEST2_F(TwoTilesAllContextsTest, HEAVY_givenTwoTilesAndAllContextsWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using TwoTilesDualContextTest = MultitileMulticontextTests<2, MulticontextOclAubFixture::EnabledCommandStreamers::dual>;
HWTEST2_F(TwoTilesDualContextTest, givenTwoTilesAndDualContextWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

using TwoTilesSingleContextTest = MultitileMulticontextTests<2, MulticontextOclAubFixture::EnabledCommandStreamers::single>;
HWTEST2_F(TwoTilesSingleContextTest, givenTwoTilesAndSingleContextWhenSubmittingThenDataIsValid, SupportsMultiTile) {
    runAubTest<FamilyType>();
}

// 1 Tile
using SingleTileAllContextsTest = MultitileMulticontextTests<1, MulticontextOclAubFixture::EnabledCommandStreamers::all>;
HWTEST2_F(SingleTileAllContextsTest, GENERATEONLY_givenSingleTileAndAllContextsWhenSubmittingThenDataIsValid, SupportsMultiTile) {
    runAubTest<FamilyType>();
}

using SingleTileDualContextTest = MultitileMulticontextTests<1, MulticontextOclAubFixture::EnabledCommandStreamers::dual>;
HWTEST2_F(SingleTileDualContextTest, givenSingleTileAndDualContextWhenSubmittingThenDataIsValid, IsPVC) {
    runAubTest<FamilyType>();
}

HWTEST2_F(SingleTileDualContextTest, givenSingleAllocationWhenUpdatedFromDifferentContextThenDataIsValid, IsPVC) {
    cl_int retVal = CL_SUCCESS;
    const uint32_t bufferSize = 256;
    const uint32_t halfBufferSize = bufferSize / 2;
    uint8_t initPattern[bufferSize];
    std::fill(initPattern, initPattern + sizeof(initPattern), 0);
    std::unique_ptr<Buffer> buffer(Buffer::create(context.get(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bufferSize, initPattern, retVal));
    buffer->forceDisallowCPUCopy = true;

    auto svmManager = context->getSVMAllocsManager();
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, context->rootDeviceIndices, context->deviceBitfields);
    auto writePattern1 = static_cast<uint8_t *>(svmManager->createUnifiedMemoryAllocation(halfBufferSize, unifiedMemoryProperties));
    auto writePattern2 = static_cast<uint8_t *>(svmManager->createUnifiedMemoryAllocation(halfBufferSize, unifiedMemoryProperties));

    std::fill(writePattern1, writePattern1 + halfBufferSize, 1);
    std::fill(writePattern2, writePattern2 + halfBufferSize, 2);

    auto simulatedCsr0 = getSimulatedCsr<FamilyType>(0, 0);
    simulatedCsr0->overrideDispatchPolicy(DispatchMode::batchedDispatch);
    auto simulatedCsr1 = getSimulatedCsr<FamilyType>(0, 1);
    simulatedCsr1->overrideDispatchPolicy(DispatchMode::batchedDispatch);

    commandQueues[0][0]->enqueueWriteBuffer(buffer.get(), CL_FALSE, 0, halfBufferSize, writePattern1, nullptr, 0, nullptr, nullptr);
    commandQueues[0][1]->enqueueWriteBuffer(buffer.get(), CL_FALSE, halfBufferSize, halfBufferSize, writePattern2, nullptr, 0, nullptr, nullptr);

    commandQueues[0][1]->finish(false); // submit second enqueue first to make sure that residency flow is correct
    commandQueues[0][0]->finish(false);

    auto gpuPtr = reinterpret_cast<void *>(buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress() + buffer->getOffset());
    expectMemory<FamilyType>(gpuPtr, writePattern1, halfBufferSize, 0, 0);
    expectMemory<FamilyType>(ptrOffset(gpuPtr, halfBufferSize), writePattern2, halfBufferSize, 0, 1);

    svmManager->freeSVMAlloc(writePattern1);
    svmManager->freeSVMAlloc(writePattern2);
}

// 1 Tile
using SingleTileDualContextTest = MultitileMulticontextTests<1, MulticontextOclAubFixture::EnabledCommandStreamers::dual>;
HWTEST2_F(SingleTileDualContextTest, givenSingleTileAndDualContextWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

using SingleTileAllContextsTest = MultitileMulticontextTests<1, MulticontextOclAubFixture::EnabledCommandStreamers::all>;
HWTEST2_F(SingleTileAllContextsTest, HEAVY_givenSingleTileAndAllContextsWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

// 2 Tiles
using TwoTilesSingleContextTest = MultitileMulticontextTests<2, MulticontextOclAubFixture::EnabledCommandStreamers::single>;
HWTEST2_F(TwoTilesSingleContextTest, givenTwoTilesAndSingleContextWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

using TwoTilesDualContextTest = MultitileMulticontextTests<2, MulticontextOclAubFixture::EnabledCommandStreamers::dual>;
HWTEST2_F(TwoTilesDualContextTest, givenTwoTilesAndDualContextWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

using TwoTilesAllContextsTest = MultitileMulticontextTests<2, MulticontextOclAubFixture::EnabledCommandStreamers::all>;
HWTEST2_F(TwoTilesAllContextsTest, GENERATEONLY_givenTwoTilesAndAllContextsWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

// 4 Tiles
using FourTilesSingleContextTest = MultitileMulticontextTests<4, MulticontextOclAubFixture::EnabledCommandStreamers::single>;
HWTEST2_F(FourTilesSingleContextTest, givenFourTilesAndSingleContextWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

using FourTilesDualContextTest = MultitileMulticontextTests<4, MulticontextOclAubFixture::EnabledCommandStreamers::dual>;
HWTEST2_F(FourTilesDualContextTest, GENERATEONLY_givenFourTilesAndDualContextWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}

using FourTilesAllContextsTest = MultitileMulticontextTests<4, MulticontextOclAubFixture::EnabledCommandStreamers::all>;
HWTEST2_F(FourTilesAllContextsTest, GENERATEONLY_givenFourTilesAndAllContextsWhenWritingImageThenDataIsValid, SupportsMultiTile) {
    runAubWriteImageTest<FamilyType>();
}
