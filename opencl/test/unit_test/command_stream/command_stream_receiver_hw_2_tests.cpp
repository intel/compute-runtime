/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_hw_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_hw_helper.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_blit_properties.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

HWTEST_F(BcsTests, givenBltSizeWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    }

    size_t notAlignedBltSize = (3 * max2DBlitSize) + 1;
    size_t alignedBltSize = (3 * max2DBlitSize);
    uint32_t alignedNumberOfBlts = 3;
    uint32_t notAlignedNumberOfBlts = 4;

    auto expectedAlignedSize = cmdsSizePerBlit * alignedNumberOfBlts;
    auto expectedNotAlignedSize = cmdsSizePerBlit * notAlignedNumberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedAlignedSize += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
        expectedNotAlignedSize += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    }

    auto alignedCopySize = Vec3<size_t>{alignedBltSize, 1, 1};
    auto notAlignedCopySize = Vec3<size_t>{notAlignedBltSize, 1, 1};

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        alignedCopySize, csrDependencies, false, false, false, pClDevice->getRootDeviceEnvironment());
    auto notAlignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        notAlignedCopySize, csrDependencies, false, false, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
    EXPECT_EQ(expectedNotAlignedSize, notAlignedEstimatedSize);
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(alignedCopySize, pClDevice->getRootDeviceEnvironment()));
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(notAlignedCopySize, pClDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BcsTests, givenDebugCapabilityWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    }

    const size_t debugCommandsSize = (EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite() + EncodeSempahore<FamilyType>::getSizeMiSemaphoreWait()) * 2;

    constexpr uint32_t numberOfBlts = 3;
    constexpr size_t bltSize = (numberOfBlts * max2DBlitSize);

    auto expectedSize = (cmdsSizePerBlit * numberOfBlts) + debugCommandsSize + (2 * MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(pDevice->getHardwareInfo())) +
                        EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite() + sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSize = alignUp(expectedSize, MemoryConstants::cacheLineSize);

    BlitProperties blitProperties{};
    blitProperties.copySize = {bltSize, 1, 1};
    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    auto estimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        blitPropertiesContainer, false, true, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedSize, estimatedSize);
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(blitProperties.copySize, pClDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BcsTests, givenBltSizeWhenEstimatingCommandSizeForReadBufferRectThenAddAllRequiredCommands) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    }

    Vec3<size_t> notAlignedBltSize = {(3 * max2DBlitSize) + 1, 4, 2};
    Vec3<size_t> alignedBltSize = {(3 * max2DBlitSize), 4, 2};
    size_t alignedNumberOfBlts = 3 * alignedBltSize.y * alignedBltSize.z;
    size_t notAlignedNumberOfBlts = 4 * notAlignedBltSize.y * notAlignedBltSize.z;

    auto expectedAlignedSize = cmdsSizePerBlit * alignedNumberOfBlts;
    auto expectedNotAlignedSize = cmdsSizePerBlit * notAlignedNumberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedAlignedSize += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
        expectedNotAlignedSize += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    }

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        alignedBltSize, csrDependencies, false, false, false, pClDevice->getRootDeviceEnvironment());
    auto notAlignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        notAlignedBltSize, csrDependencies, false, false, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
    EXPECT_EQ(expectedNotAlignedSize, notAlignedEstimatedSize);
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(notAlignedBltSize, pClDevice->getRootDeviceEnvironment()));
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(alignedBltSize, pClDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BcsTests, givenBltWithBigCopySizeWhenEstimatingCommandSizeForReadBufferRectThenAddAllRequiredCommands) {
    auto &rootDeviceEnvironment = pClDevice->getRootDeviceEnvironment();
    auto maxWidthToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(rootDeviceEnvironment));
    auto maxHeightToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment));

    size_t cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);

    if (BlitCommandsHelper<FamilyType>::miArbCheckWaRequired()) {
        cmdsSizePerBlit += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    }

    Vec3<size_t> alignedBltSize = {(3 * maxWidthToCopy), (4 * maxHeightToCopy), 2};
    Vec3<size_t> notAlignedBltSize = {(3 * maxWidthToCopy + 1), (4 * maxHeightToCopy), 2};

    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(alignedBltSize, rootDeviceEnvironment));

    size_t alignedNumberOfBlts = (3 * 4 * alignedBltSize.z);
    size_t notAlignedNumberOfBlts = (4 * 4 * notAlignedBltSize.z);

    auto expectedAlignedSize = cmdsSizePerBlit * alignedNumberOfBlts;
    auto expectedNotAlignedSize = cmdsSizePerBlit * notAlignedNumberOfBlts;

    if (BlitCommandsHelper<FamilyType>::preBlitCommandWARequired()) {
        expectedAlignedSize += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
        expectedNotAlignedSize += EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite();
    }

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        alignedBltSize, csrDependencies, false, false, false, rootDeviceEnvironment);
    auto notAlignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandSize(
        notAlignedBltSize, csrDependencies, false, false, false, rootDeviceEnvironment);

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
    EXPECT_EQ(expectedNotAlignedSize, notAlignedEstimatedSize);
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(notAlignedBltSize, rootDeviceEnvironment));
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(alignedBltSize, rootDeviceEnvironment));
}

HWTEST_F(BcsTests, WhenGetNumberOfBlitsIsCalledThenCorrectValuesAreReturned) {
    auto &rootDeviceEnvironment = pClDevice->getRootDeviceEnvironment();
    auto maxWidthToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(rootDeviceEnvironment));
    auto maxHeightToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment));

    {
        Vec3<size_t> copySize = {maxWidthToCopy * maxHeightToCopy, 1, 3};
        size_t expectednBlitsCopyRegion = maxHeightToCopy * 3;
        size_t expectednBlitsCopyPerRow = 3;
        auto nBlitsCopyRegion = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment);
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment);

        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
        EXPECT_EQ(expectednBlitsCopyRegion, nBlitsCopyRegion);
        EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySize, rootDeviceEnvironment));
    }
    {
        Vec3<size_t> copySize = {2 * maxWidthToCopy, 16, 3};
        size_t expectednBlitsCopyRegion = 2 * 3;
        size_t expectednBlitsCopyPerRow = 16 * 3;
        auto nBlitsCopyRegion = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment);
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment);

        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
        EXPECT_EQ(expectednBlitsCopyRegion, nBlitsCopyRegion);
        EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySize, rootDeviceEnvironment));
    }
    {
        Vec3<size_t> copySize = {2 * maxWidthToCopy, 3 * maxHeightToCopy, 4};
        size_t expectednBlitsCopyRegion = 2 * 3 * 4;
        size_t expectednBlitsCopyPerRow = 3 * maxHeightToCopy * 4;
        auto nBlitsCopyRegion = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyRegion(copySize, rootDeviceEnvironment);
        auto nBlitsCopyPerRow = BlitCommandsHelper<FamilyType>::getNumberOfBlitsForCopyPerRow(copySize, rootDeviceEnvironment);

        EXPECT_EQ(expectednBlitsCopyPerRow, nBlitsCopyPerRow);
        EXPECT_EQ(expectednBlitsCopyRegion, nBlitsCopyRegion);
        EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySize, rootDeviceEnvironment));
    }
}

HWTEST_F(BcsTests, givenCsrDependenciesWhenProgrammingCommandStreamThenAddSemaphore) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    uint32_t numberOfDependencyContainers = 2;
    size_t numberNodesPerContainer = 5;
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          csr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestamp0);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestamp1);

    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    auto &cmdList = hwParser.cmdList;
    bool xyCopyBltCmdFound = false;
    bool dependenciesFound = false;

    for (auto cmdIterator = cmdList.begin(); cmdIterator != cmdList.end(); cmdIterator++) {
        if (genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator)) {
            xyCopyBltCmdFound = true;
            continue;
        }
        auto miSemaphore = genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*cmdIterator);
        if (miSemaphore) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*miSemaphore)) {
                continue;
            }
            dependenciesFound = true;
            EXPECT_FALSE(xyCopyBltCmdFound);

            for (uint32_t i = 1; i < numberOfDependencyContainers * numberNodesPerContainer; i++) {
                EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*(++cmdIterator)));
            }
        }
    }
    EXPECT_TRUE(xyCopyBltCmdFound);
    EXPECT_TRUE(dependenciesFound);
}

HWTEST_F(BcsTests, givenMultipleBlitPropertiesWhenDispatchingThenProgramCommandsInCorrectOrder) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr1 = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr1 = reinterpret_cast<void *>(hostAllocationPtr1.get());

    auto hostAllocationPtr2 = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr2 = reinterpret_cast<void *>(hostAllocationPtr2.get());

    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties1 = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                           csr, graphicsAllocation1, nullptr, hostPtr1,
                                                                           graphicsAllocation1->getGpuAddress(), 0,
                                                                           0, 0, {1, 1, 1}, 0, 0, 0, 0);
    auto blitProperties2 = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                           csr, graphicsAllocation2, nullptr, hostPtr2,
                                                                           graphicsAllocation2->getGpuAddress(), 0,
                                                                           0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*csr.getTimestampPacketAllocator(), 1);
    blitProperties1.csrDependencies.timestampPacketContainer.push_back(&timestamp1);
    blitProperties2.csrDependencies.timestampPacketContainer.push_back(&timestamp2);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties1);
    blitPropertiesContainer.push_back(blitProperties2);

    csr.flushBcsTask(blitPropertiesContainer, true, false, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    auto &cmdList = hwParser.cmdList;

    uint32_t xyCopyBltCmdFound = 0;
    uint32_t dependenciesFound = 0;

    for (auto cmdIterator = cmdList.begin(); cmdIterator != cmdList.end(); cmdIterator++) {
        if (genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator)) {
            xyCopyBltCmdFound++;
            EXPECT_EQ(xyCopyBltCmdFound, dependenciesFound);

            continue;
        }
        auto miSemaphore = genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*cmdIterator);
        if (miSemaphore) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWait(*miSemaphore)) {
                continue;
            }
            dependenciesFound++;
            EXPECT_EQ(xyCopyBltCmdFound, dependenciesFound - 1);
        }
    }
    EXPECT_EQ(2u, xyCopyBltCmdFound);
    EXPECT_EQ(2u, dependenciesFound);
}

HWTEST_F(BcsTests, whenBlitBufferThenCommandBufferHasProperTaskCount) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          csr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    csr.flushBcsTask(blitPropertiesContainer, true, false, *pDevice);

    EXPECT_EQ(csr.getCS(0u).getGraphicsAllocation()->getTaskCount(csr.getOsContext().getContextId()), csr.peekTaskCount());
    EXPECT_EQ(csr.getCS(0u).getGraphicsAllocation()->getResidencyTaskCount(csr.getOsContext().getContextId()), csr.peekTaskCount());
}

HWTEST_F(BcsTests, givenUpdateTaskCountFromWaitWhenBlitBufferThenCsrHasProperTaskCounts) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          csr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    auto taskCount = csr.peekTaskCount();

    csr.flushBcsTask(blitPropertiesContainer, false, false, *pDevice);

    EXPECT_EQ(csr.peekTaskCount(), taskCount + 1);
    EXPECT_EQ(csr.peekLatestFlushedTaskCount(), taskCount);
}

HWTEST_F(BcsTests, givenProfilingEnabledWhenBlitBufferThenCommandBufferIsConstructedProperly) {
    auto bcsOsContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0,
                                                                     EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, pDevice->getDeviceBitfield())));
    auto bcsCsr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    bcsCsr->setupContext(*bcsOsContext);
    bcsCsr->initializeTagAllocation();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          *bcsCsr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp(*bcsCsr->getTimestampPacketAllocator(), 1u);
    blitProperties.outputTimestampPacket = timestamp.getNode(0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    bcsCsr->flushBcsTask(blitPropertiesContainer, false, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(bcsCsr->commandStream);
    auto &cmdList = hwParser.cmdList;

    auto cmdIterator = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);
    cmdIterator = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);
    cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);

    cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);

    cmdIterator = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);
    cmdIterator = find<typename FamilyType::MI_STORE_REGISTER_MEM *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);
}

HWTEST_F(BcsTests, givenNotInitializedOsContextWhenBlitBufferIsCalledThenInitializeContext) {
    auto bcsOsContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, pDevice->getDeviceBitfield())));
    auto bcsCsr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    bcsCsr->setupContext(*bcsOsContext);
    bcsCsr->initializeTagAllocation();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          *bcsCsr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp(*bcsCsr->getTimestampPacketAllocator(), 1u);
    blitProperties.outputTimestampPacket = timestamp.getNode(0);
    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    EXPECT_FALSE(bcsOsContext->isInitialized());
    bcsCsr->flushBcsTask(blitPropertiesContainer, false, true, *pDevice);
    EXPECT_TRUE(bcsOsContext->isInitialized());
}

HWTEST_F(BcsTests, givenInputAllocationsWhenBlitDispatchedThenMakeAllAllocationsResident) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr1 = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr1 = reinterpret_cast<void *>(hostAllocationPtr1.get());

    auto hostAllocationPtr2 = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr2 = reinterpret_cast<void *>(hostAllocationPtr2.get());

    EXPECT_EQ(0u, csr.makeSurfacePackNonResidentCalled);
    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties1 = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                           csr, graphicsAllocation1, nullptr, hostPtr1,
                                                                           graphicsAllocation1->getGpuAddress(), 0,
                                                                           0, 0, {1, 1, 1}, 0, 0, 0, 0);

    auto blitProperties2 = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                           csr, graphicsAllocation2, nullptr, hostPtr2,
                                                                           graphicsAllocation2->getGpuAddress(), 0,
                                                                           0, 0, {1, 1, 1}, 0, 0, 0, 0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties1);
    blitPropertiesContainer.push_back(blitProperties2);

    csr.flushBcsTask(blitPropertiesContainer, false, false, *pDevice);

    uint32_t residentAllocationsNum = 5u;
    EXPECT_TRUE(csr.isMadeResident(graphicsAllocation1));
    EXPECT_TRUE(csr.isMadeResident(graphicsAllocation2));
    EXPECT_TRUE(csr.isMadeResident(csr.getTagAllocation()));
    EXPECT_EQ(1u, csr.makeSurfacePackNonResidentCalled);
    if (csr.clearColorAllocation) {
        EXPECT_TRUE(csr.isMadeResident(csr.clearColorAllocation));
        residentAllocationsNum++;
    }
    if (csr.globalFenceAllocation) {
        EXPECT_TRUE(csr.isMadeResident(csr.globalFenceAllocation));
        residentAllocationsNum++;
    }

    EXPECT_EQ(residentAllocationsNum, csr.makeResidentAllocations.size());
}

HWTEST_F(BcsTests, givenFenceAllocationIsRequiredWhenBlitDispatchedThenMakeAllAllocationsResident) {
    RAIIHwHelperFactory<MockHwHelperWithFenceAllocation<FamilyType>> hwHelperBackup{pDevice->getHardwareInfo().platform.eRenderCoreFamily};

    auto bcsOsContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, pDevice->getDeviceBitfield())));
    auto bcsCsr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    bcsCsr->setupContext(*bcsOsContext);
    bcsCsr->initializeTagAllocation();
    bcsCsr->createGlobalFenceAllocation();
    bcsCsr->storeMakeResidentAllocations = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr1 = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr1 = reinterpret_cast<void *>(hostAllocationPtr1.get());

    auto hostAllocationPtr2 = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr2 = reinterpret_cast<void *>(hostAllocationPtr2.get());

    EXPECT_EQ(0u, bcsCsr->makeSurfacePackNonResidentCalled);
    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties1 = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                           *bcsCsr, graphicsAllocation1, nullptr, hostPtr1,
                                                                           graphicsAllocation1->getGpuAddress(), 0,
                                                                           0, 0, {1, 1, 1}, 0, 0, 0, 0);

    auto blitProperties2 = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                           *bcsCsr, graphicsAllocation2, nullptr, hostPtr2,
                                                                           graphicsAllocation2->getGpuAddress(), 0,
                                                                           0, 0, {1, 1, 1}, 0, 0, 0, 0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties1);
    blitPropertiesContainer.push_back(blitProperties2);

    bcsCsr->flushBcsTask(blitPropertiesContainer, false, false, *pDevice);

    uint32_t residentAllocationsNum = 6u;
    EXPECT_TRUE(bcsCsr->isMadeResident(graphicsAllocation1));
    EXPECT_TRUE(bcsCsr->isMadeResident(graphicsAllocation2));
    EXPECT_TRUE(bcsCsr->isMadeResident(bcsCsr->getTagAllocation()));
    EXPECT_TRUE(bcsCsr->isMadeResident(bcsCsr->globalFenceAllocation));
    if (bcsCsr->clearColorAllocation) {
        EXPECT_TRUE(bcsCsr->isMadeResident(bcsCsr->clearColorAllocation));
        residentAllocationsNum++;
    }
    EXPECT_EQ(1u, bcsCsr->makeSurfacePackNonResidentCalled);

    EXPECT_EQ(residentAllocationsNum, bcsCsr->makeResidentAllocations.size());
}

HWTEST_F(BcsTests, givenBufferWhenBlitCalledThenFlushCommandBuffer) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.recordFlusheBatchBuffer = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto &commandStream = csr.getCS(MemoryConstants::pageSize);
    size_t commandStreamOffset = 4;
    commandStream.getSpace(commandStreamOffset);

    uint32_t newTaskCount = 17;
    csr.taskCount = newTaskCount - 1;

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          csr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    flushBcsTask(&csr, blitProperties, true, *pDevice);

    EXPECT_EQ(commandStream.getGraphicsAllocation(), csr.latestFlushedBatchBuffer.commandBufferAllocation);
    EXPECT_EQ(commandStreamOffset, csr.latestFlushedBatchBuffer.startOffset);
    EXPECT_EQ(0u, csr.latestFlushedBatchBuffer.chainedBatchBufferStartOffset);
    EXPECT_EQ(nullptr, csr.latestFlushedBatchBuffer.chainedBatchBuffer);
    EXPECT_FALSE(csr.latestFlushedBatchBuffer.requiresCoherency);
    EXPECT_FALSE(csr.latestFlushedBatchBuffer.low_priority);
    EXPECT_EQ(QueueThrottle::MEDIUM, csr.latestFlushedBatchBuffer.throttle);
    EXPECT_EQ(commandStream.getUsed(), csr.latestFlushedBatchBuffer.usedSize);
    EXPECT_EQ(&commandStream, csr.latestFlushedBatchBuffer.stream);

    EXPECT_EQ(newTaskCount, csr.latestWaitForCompletionWithTimeoutTaskCount.load());
}

template <typename FamilyType>
class MyMockCsr : public UltCommandStreamReceiver<FamilyType> {
  public:
    using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                                     bool useQuickKmdSleep, QueueThrottle throttle) override {
        waitForTaskCountWithKmdNotifyFallbackCalled++;
        taskCountToWaitPassed = taskCountToWait;
        flushStampToWaitPassed = flushStampToWait;
        useQuickKmdSleepPassed = useQuickKmdSleep;
        throttlePassed = throttle;
        return waitForTaskCountWithKmdNotifyFallbackReturnValue;
    }

    FlushStamp flushStampToWaitPassed = 0;
    uint32_t taskCountToWaitPassed = 0;
    uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
    bool useQuickKmdSleepPassed = false;
    QueueThrottle throttlePassed = QueueThrottle::MEDIUM;
    WaitStatus waitForTaskCountWithKmdNotifyFallbackReturnValue{WaitStatus::Ready};
};

HWTEST_F(BcsTests, GivenNoneGpuHangWhenBlitFromHostPtrCalledThenCallWaitWithKmdFallbackAndNewTaskCountIsReturned) {
    auto myMockCsr = std::make_unique<MyMockCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto &bcsOsContext = pDevice->getUltCommandStreamReceiver<FamilyType>().getOsContext();
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(bcsOsContext);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          *myMockCsr, graphicsAllocation, nullptr,
                                                                          hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    const auto taskCount1 = flushBcsTask(myMockCsr.get(), blitProperties, false, *pDevice);
    EXPECT_TRUE(taskCount1.has_value());

    EXPECT_EQ(0u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);

    const auto taskCount2 = flushBcsTask(myMockCsr.get(), blitProperties, true, *pDevice);
    EXPECT_TRUE(taskCount2.has_value());

    EXPECT_EQ(1u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_EQ(myMockCsr->taskCount, myMockCsr->taskCountToWaitPassed);
    EXPECT_EQ(myMockCsr->flushStamp->peekStamp(), myMockCsr->flushStampToWaitPassed);
    EXPECT_FALSE(myMockCsr->useQuickKmdSleepPassed);
    EXPECT_EQ(myMockCsr->throttlePassed, QueueThrottle::MEDIUM);
    EXPECT_EQ(1u, myMockCsr->activePartitions);
}

HWTEST_F(BcsTests, GivenGpuHangWhenBlitFromHostPtrCalledThenCallWaitWithKmdFallbackAndDoNotReturnNewTaskCount) {
    auto myMockCsr = std::make_unique<MyMockCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto &bcsOsContext = pDevice->getUltCommandStreamReceiver<FamilyType>().getOsContext();
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(bcsOsContext);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          *myMockCsr, graphicsAllocation, nullptr,
                                                                          hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    const auto taskCount1 = flushBcsTask(myMockCsr.get(), blitProperties, false, *pDevice);
    EXPECT_TRUE(taskCount1.has_value());

    EXPECT_EQ(0u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);

    myMockCsr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::GpuHang;

    const auto taskCount2 = flushBcsTask(myMockCsr.get(), blitProperties, true, *pDevice);
    EXPECT_FALSE(taskCount2.has_value());

    EXPECT_EQ(1u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_EQ(myMockCsr->taskCount, myMockCsr->taskCountToWaitPassed);
    EXPECT_EQ(myMockCsr->flushStamp->peekStamp(), myMockCsr->flushStampToWaitPassed);
    EXPECT_FALSE(myMockCsr->useQuickKmdSleepPassed);
    EXPECT_EQ(myMockCsr->throttlePassed, QueueThrottle::MEDIUM);
    EXPECT_EQ(1u, myMockCsr->activePartitions);
}

HWTEST_F(BcsTests, whenBlitFromHostPtrCalledThenCleanTemporaryAllocations) {
    auto &bcsCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockInternalAllocationsStorage = new MockInternalAllocationStorage(bcsCsr);
    bcsCsr.internalAllocationStorage.reset(mockInternalAllocationsStorage);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    bcsCsr.taskCount = 17;

    EXPECT_EQ(0u, mockInternalAllocationsStorage->cleanAllocationsCalled);

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          bcsCsr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    flushBcsTask(&bcsCsr, blitProperties, false, *pDevice);

    EXPECT_EQ(0u, mockInternalAllocationsStorage->cleanAllocationsCalled);

    flushBcsTask(&bcsCsr, blitProperties, true, *pDevice);

    EXPECT_EQ(1u, mockInternalAllocationsStorage->cleanAllocationsCalled);
    EXPECT_EQ(bcsCsr.taskCount, mockInternalAllocationsStorage->lastCleanAllocationsTaskCount);
    EXPECT_TRUE(TEMPORARY_ALLOCATION == mockInternalAllocationsStorage->lastCleanAllocationUsage);
}

HWTEST_F(BcsTests, givenBufferWhenBlitOperationCalledThenProgramCorrectGpuAddresses) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 100, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 100, nullptr, retVal));
    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize * 2;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());
    const size_t hostPtrOffset = 0x1234;

    const size_t subBuffer1Offset = 0x23;
    cl_buffer_region subBufferRegion1 = {subBuffer1Offset, 1};
    auto subBuffer1 = clUniquePtr<Buffer>(buffer1->createSubBuffer(CL_MEM_READ_WRITE, 0, &subBufferRegion1, retVal));

    Vec3<size_t> copySizes[2] = {{1, 1, 1},
                                 {1, 2, 1}};
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySizes[0], pDevice->getRootDeviceEnvironment()));
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySizes[1], pDevice->getRootDeviceEnvironment()));

    for (auto &copySize : copySizes) {
        {
            // from hostPtr
            HardwareParse hwParser;
            auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                  csr, graphicsAllocation1,
                                                                                  nullptr, hostPtr,
                                                                                  graphicsAllocation1->getGpuAddress() +
                                                                                      subBuffer1->getOffset(),
                                                                                  0, {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);

            flushBcsTask(&csr, blitProperties, true, *pDevice);

            hwParser.parseCommands<FamilyType>(csr.commandStream);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            ASSERT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(hostPtr, hostPtrOffset)), bltCmd->getSourceBaseAddress());
            }
            EXPECT_EQ(graphicsAllocation1->getGpuAddress() + subBuffer1Offset, bltCmd->getDestinationBaseAddress());
        }
        {
            // to hostPtr
            HardwareParse hwParser;
            auto offset = csr.commandStream.getUsed();
            auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                  csr, graphicsAllocation1,
                                                                                  nullptr, hostPtr,
                                                                                  graphicsAllocation1->getGpuAddress() +
                                                                                      subBuffer1->getOffset(),
                                                                                  0, {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);

            flushBcsTask(&csr, blitProperties, true, *pDevice);

            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            ASSERT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(hostPtr, hostPtrOffset)), bltCmd->getDestinationBaseAddress());
            }
            EXPECT_EQ(graphicsAllocation1->getGpuAddress() + subBuffer1Offset, bltCmd->getSourceBaseAddress());
        }

        {
            // Buffer to Buffer
            HardwareParse hwParser;
            auto offset = csr.commandStream.getUsed();
            auto blitProperties = BlitProperties::constructPropertiesForCopy(graphicsAllocation1,
                                                                             graphicsAllocation2, 0, 0, copySize, 0, 0, 0, 0, csr.getClearColorAllocation());

            flushBcsTask(&csr, blitProperties, true, *pDevice);

            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            ASSERT_NE(nullptr, bltCmd);
            EXPECT_EQ(graphicsAllocation1->getGpuAddress(), bltCmd->getDestinationBaseAddress());
            EXPECT_EQ(graphicsAllocation2->getGpuAddress(), bltCmd->getSourceBaseAddress());
        }

        {
            // Buffer to Buffer - with object offset
            const size_t subBuffer2Offset = 0x20;
            cl_buffer_region subBufferRegion2 = {subBuffer2Offset, 1};
            auto subBuffer2 = clUniquePtr<Buffer>(buffer2->createSubBuffer(CL_MEM_READ_WRITE, 0, &subBufferRegion2, retVal));

            BuiltinOpParams builtinOpParams = {};
            builtinOpParams.dstMemObj = subBuffer2.get();
            builtinOpParams.srcMemObj = subBuffer1.get();
            builtinOpParams.size.x = copySize.x;
            builtinOpParams.size.y = copySize.y;

            auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::BufferToBuffer, csr, builtinOpParams);

            auto offset = csr.commandStream.getUsed();
            flushBcsTask(&csr, blitProperties, true, *pDevice);

            HardwareParse hwParser;
            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            EXPECT_NE(nullptr, bltCmd);
            EXPECT_EQ(graphicsAllocation2->getGpuAddress() + subBuffer2Offset, bltCmd->getDestinationBaseAddress());
            EXPECT_EQ(graphicsAllocation1->getGpuAddress() + subBuffer1Offset, bltCmd->getSourceBaseAddress());
        }
    }
}

HWTEST_F(BcsTests, givenMapAllocationWhenDispatchReadWriteOperationThenSetValidGpuAddress) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto memoryManager = csr.getMemoryManager();

    constexpr size_t mapAllocationSize = MemoryConstants::pageSize * 2;
    auto mapAllocationPtr = allocateAlignedMemory(mapAllocationSize, MemoryConstants::pageSize);

    AllocationProperties properties{csr.getRootDeviceIndex(), false, mapAllocationSize, AllocationType::MAP_ALLOCATION, false, pDevice->getDeviceBitfield()};
    GraphicsAllocation *mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, mapAllocationPtr.get());

    auto mapAllocationOffset = 0x1234;
    auto mapPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + mapAllocationOffset);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 100, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    const size_t hostPtrOffset = 0x1234;

    Vec3<size_t> copySizes[2] = {{4, 1, 1},
                                 {4, 2, 1}};

    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySizes[0], pDevice->getRootDeviceEnvironment()));
    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(copySizes[1], pDevice->getRootDeviceEnvironment()));

    for (auto &copySize : copySizes) {
        {
            // from hostPtr
            HardwareParse hwParser;
            auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                  csr, graphicsAllocation,
                                                                                  mapAllocation, mapPtr,
                                                                                  graphicsAllocation->getGpuAddress(),
                                                                                  castToUint64(mapPtr),
                                                                                  {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);

            flushBcsTask(&csr, blitProperties, true, *pDevice);
            hwParser.parseCommands<FamilyType>(csr.commandStream);
            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            EXPECT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(mapPtr, hostPtrOffset)), bltCmd->getSourceBaseAddress());
            }
            EXPECT_EQ(graphicsAllocation->getGpuAddress(), bltCmd->getDestinationBaseAddress());
        }

        {
            // to hostPtr
            HardwareParse hwParser;
            auto offset = csr.commandStream.getUsed();
            auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                  csr, graphicsAllocation,
                                                                                  mapAllocation, mapPtr,
                                                                                  graphicsAllocation->getGpuAddress(),
                                                                                  castToUint64(mapPtr), {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);
            flushBcsTask(&csr, blitProperties, true, *pDevice);
            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            EXPECT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(mapPtr, hostPtrOffset)), bltCmd->getDestinationBaseAddress());
            }
            EXPECT_EQ(graphicsAllocation->getGpuAddress(), bltCmd->getSourceBaseAddress());
        }

        {
            // bufferRect to hostPtr
            HardwareParse hwParser;
            auto offset = csr.commandStream.getUsed();
            auto copySize = Vec3<size_t>(4, 2, 1);
            auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                  csr, graphicsAllocation,
                                                                                  mapAllocation, mapPtr,
                                                                                  graphicsAllocation->getGpuAddress(),
                                                                                  castToUint64(mapPtr), {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);
            flushBcsTask(&csr, blitProperties, true, *pDevice);
            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            EXPECT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(mapPtr, hostPtrOffset)), bltCmd->getDestinationBaseAddress());
            }
            EXPECT_EQ(graphicsAllocation->getGpuAddress(), bltCmd->getSourceBaseAddress());
        }
        {
            // bufferWrite from hostPtr
            HardwareParse hwParser;
            auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                  csr, graphicsAllocation,
                                                                                  mapAllocation, mapPtr,
                                                                                  graphicsAllocation->getGpuAddress(),
                                                                                  castToUint64(mapPtr),
                                                                                  {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);
            flushBcsTask(&csr, blitProperties, true, *pDevice);
            hwParser.parseCommands<FamilyType>(csr.commandStream);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
            EXPECT_NE(nullptr, bltCmd);
            if (pDevice->isFullRangeSvm()) {
                EXPECT_EQ(reinterpret_cast<uint64_t>(ptrOffset(mapPtr, hostPtrOffset)), bltCmd->getSourceBaseAddress());
            }
            EXPECT_EQ(graphicsAllocation->getGpuAddress(), bltCmd->getDestinationBaseAddress());
        }
    }

    memoryManager->freeGraphicsMemory(mapAllocation);
}

HWTEST_F(BcsTests, givenMapAllocationInBuiltinOpParamsWhenConstructingThenUseItAsSourceOrDstAllocation) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto memoryManager = static_cast<MockMemoryManager *>(csr.getMemoryManager());

    constexpr size_t mapAllocationSize = MemoryConstants::pageSize * 2;
    auto mapAllocationPtr = allocateAlignedMemory(mapAllocationSize, MemoryConstants::pageSize);

    AllocationProperties properties{csr.getRootDeviceIndex(), false, mapAllocationSize, AllocationType::MAP_ALLOCATION, false, pDevice->getDeviceBitfield()};
    GraphicsAllocation *mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, reinterpret_cast<void *>(mapAllocationPtr.get()));

    auto mapAllocationOffset = 0x1234;
    auto mapPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + mapAllocationOffset);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 100, nullptr, retVal));
    memoryManager->returnFakeAllocation = true;
    {
        // from hostPtr
        BuiltinOpParams builtinOpParams = {};
        builtinOpParams.dstMemObj = buffer.get();
        builtinOpParams.srcPtr = mapPtr;
        builtinOpParams.size = {1, 1, 1};
        builtinOpParams.transferAllocation = mapAllocation;

        auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                    csr, builtinOpParams);
        EXPECT_EQ(mapAllocation, blitProperties.srcAllocation);
    }
    {
        // to hostPtr
        BuiltinOpParams builtinOpParams = {};
        builtinOpParams.srcMemObj = buffer.get();
        builtinOpParams.dstPtr = mapPtr;
        builtinOpParams.size = {1, 1, 1};
        builtinOpParams.transferAllocation = mapAllocation;

        auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                    csr, builtinOpParams);
        EXPECT_EQ(mapAllocation, blitProperties.dstAllocation);
    }

    memoryManager->returnFakeAllocation = false;
    memoryManager->freeGraphicsMemory(mapAllocation);
}

HWTEST_F(BcsTests, givenNonZeroCopySvmAllocationWhenConstructingBlitPropertiesForReadWriteBufferCallThenSetValidAllocations) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    MockMemoryManager mockMemoryManager(true, true);
    SVMAllocsManager svmAllocsManager(&mockMemoryManager, false);

    auto svmAllocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_WRITE);
    auto svmAlloc = svmAllocsManager.createSVMAlloc(1, svmAllocationProperties, context->getRootDeviceIndices(), context->getDeviceBitfields());
    auto svmData = svmAllocsManager.getSVMAlloc(svmAlloc);

    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());

    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_NE(nullptr, svmData->cpuAllocation);
    EXPECT_NE(gpuAllocation, svmData->cpuAllocation);

    {
        // from hostPtr
        BuiltinOpParams builtinOpParams = {};
        builtinOpParams.dstSvmAlloc = gpuAllocation;
        builtinOpParams.srcSvmAlloc = svmData->cpuAllocation;
        builtinOpParams.srcPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddress());
        builtinOpParams.size = {1, 1, 1};

        auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                    csr, builtinOpParams);
        EXPECT_EQ(svmData->cpuAllocation, blitProperties.srcAllocation);
        EXPECT_EQ(gpuAllocation, blitProperties.dstAllocation);
    }
    {
        // to hostPtr
        BuiltinOpParams builtinOpParams = {};
        builtinOpParams.srcSvmAlloc = gpuAllocation;
        builtinOpParams.dstSvmAlloc = svmData->cpuAllocation;
        builtinOpParams.dstPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddress());
        builtinOpParams.size = {1, 1, 1};

        auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                    csr, builtinOpParams);
        EXPECT_EQ(svmData->cpuAllocation, blitProperties.dstAllocation);
        EXPECT_EQ(gpuAllocation, blitProperties.srcAllocation);
    }

    svmAllocsManager.freeSVMAlloc(svmAlloc);
}

HWTEST_F(BcsTests, givenSvmAllocationWhenBlitCalledThenUsePassedPointers) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    MockMemoryManager mockMemoryManager(true, true);
    SVMAllocsManager svmAllocsManager(&mockMemoryManager, false);

    auto svmAllocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_WRITE);
    auto svmAlloc = svmAllocsManager.createSVMAlloc(1, svmAllocationProperties, context->getRootDeviceIndices(), context->getDeviceBitfields());
    auto svmData = svmAllocsManager.getSVMAlloc(svmAlloc);
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());

    EXPECT_NE(nullptr, gpuAllocation);
    EXPECT_NE(nullptr, svmData->cpuAllocation);
    EXPECT_NE(gpuAllocation, svmData->cpuAllocation);

    uint64_t srcOffset = 2;
    uint64_t dstOffset = 3;

    Vec3<size_t> copySizes[2] = {{1, 1, 1},
                                 {1, 2, 1}};

    for (auto &copySize : copySizes) {
        {
            // from hostPtr
            BuiltinOpParams builtinOpParams = {};
            builtinOpParams.dstSvmAlloc = svmData->cpuAllocation;
            builtinOpParams.srcSvmAlloc = gpuAllocation;
            builtinOpParams.srcPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddress() + srcOffset);
            builtinOpParams.dstPtr = reinterpret_cast<void *>(svmData->cpuAllocation->getGpuAddress() + dstOffset);
            builtinOpParams.size = copySize;

            auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                        csr, builtinOpParams);
            EXPECT_EQ(gpuAllocation, blitProperties.srcAllocation);
            EXPECT_EQ(svmData->cpuAllocation, blitProperties.dstAllocation);

            flushBcsTask(&csr, blitProperties, true, *pDevice);

            HardwareParse hwParser;
            hwParser.parseCommands<FamilyType>(csr.commandStream, 0);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);

            EXPECT_EQ(castToUint64(builtinOpParams.dstPtr), bltCmd->getDestinationBaseAddress());
            EXPECT_EQ(castToUint64(builtinOpParams.srcPtr), bltCmd->getSourceBaseAddress());
        }
        {
            // to hostPtr
            BuiltinOpParams builtinOpParams = {};
            builtinOpParams.srcSvmAlloc = gpuAllocation;
            builtinOpParams.dstSvmAlloc = svmData->cpuAllocation;
            builtinOpParams.dstPtr = reinterpret_cast<void *>(svmData->cpuAllocation + dstOffset);
            builtinOpParams.srcPtr = reinterpret_cast<void *>(gpuAllocation + srcOffset);
            builtinOpParams.size = copySize;

            auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                        csr, builtinOpParams);

            auto offset = csr.commandStream.getUsed();
            flushBcsTask(&csr, blitProperties, true, *pDevice);

            HardwareParse hwParser;
            hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

            auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
            ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

            auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);

            EXPECT_EQ(castToUint64(builtinOpParams.dstPtr), bltCmd->getDestinationBaseAddress());
            EXPECT_EQ(castToUint64(builtinOpParams.srcPtr), bltCmd->getSourceBaseAddress());
        }
    }

    svmAllocsManager.freeSVMAlloc(svmAlloc);
}

HWTEST_F(BcsTests, givenBufferWithOffsetWhenBlitOperationCalledThenProgramCorrectGpuAddresses) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    size_t addressOffsets[] = {0, 1, 1234};
    Vec3<size_t> copySizes[2] = {{1, 1, 1},
                                 {1, 2, 1}};

    for (auto &copySize : copySizes) {

        for (auto buffer1Offset : addressOffsets) {
            {
                // from hostPtr
                HardwareParse hwParser;
                auto offset = csr.commandStream.getUsed();
                auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                      csr, graphicsAllocation1,
                                                                                      nullptr, hostPtr,
                                                                                      graphicsAllocation1->getGpuAddress(),
                                                                                      0, 0, {buffer1Offset, 0, 0}, copySize, 0, 0, 0, 0);

                flushBcsTask(&csr, blitProperties, true, *pDevice);

                hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

                auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
                ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

                auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
                EXPECT_NE(nullptr, bltCmd);
                if (pDevice->isFullRangeSvm()) {
                    EXPECT_EQ(reinterpret_cast<uint64_t>(hostPtr), bltCmd->getSourceBaseAddress());
                }
                EXPECT_EQ(ptrOffset(graphicsAllocation1->getGpuAddress(), buffer1Offset), bltCmd->getDestinationBaseAddress());
            }
            {
                // to hostPtr
                HardwareParse hwParser;
                auto offset = csr.commandStream.getUsed();
                auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                      csr, graphicsAllocation1, nullptr,
                                                                                      hostPtr,
                                                                                      graphicsAllocation1->getGpuAddress(),
                                                                                      0, 0, {buffer1Offset, 0, 0}, copySize, 0, 0, 0, 0);

                flushBcsTask(&csr, blitProperties, true, *pDevice);

                hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

                auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
                ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

                auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
                EXPECT_NE(nullptr, bltCmd);
                if (pDevice->isFullRangeSvm()) {
                    EXPECT_EQ(reinterpret_cast<uint64_t>(hostPtr), bltCmd->getDestinationBaseAddress());
                }
                EXPECT_EQ(ptrOffset(graphicsAllocation1->getGpuAddress(), buffer1Offset), bltCmd->getSourceBaseAddress());
            }
            for (auto buffer2Offset : addressOffsets) {
                // Buffer to Buffer
                HardwareParse hwParser;
                auto offset = csr.commandStream.getUsed();
                auto blitProperties = BlitProperties::constructPropertiesForCopy(graphicsAllocation1,
                                                                                 graphicsAllocation2,
                                                                                 {buffer1Offset, 0, 0}, {buffer2Offset, 0, 0}, copySize, 0, 0, 0, 0, csr.getClearColorAllocation());

                flushBcsTask(&csr, blitProperties, true, *pDevice);

                hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

                auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
                ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

                auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
                EXPECT_NE(nullptr, bltCmd);
                EXPECT_EQ(ptrOffset(graphicsAllocation1->getGpuAddress(), buffer1Offset), bltCmd->getDestinationBaseAddress());
                EXPECT_EQ(ptrOffset(graphicsAllocation2->getGpuAddress(), buffer2Offset), bltCmd->getSourceBaseAddress());
            }
        }
    }
}

HWTEST_F(BcsTests, givenBufferWithBigSizesWhenBlitOperationCalledThenProgramCorrectGpuAddresses) {
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    auto maxWidthToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(rootDeviceEnvironment));
    auto maxHeightToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment));
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));

    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    auto graphicsAllocation = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    size_t srcOrigin[] = {1, 2, 0};
    size_t dstOrigin[] = {4, 3, 1};
    size_t region[] = {maxWidthToCopy + 16, maxHeightToCopy + 16, 2};
    size_t srcRowPitch = region[0] + 34;
    size_t srcSlicePitch = srcRowPitch * region[1] + 36;
    size_t dstRowPitch = region[0] + 40;
    size_t dstSlicePitch = dstRowPitch * region[1] + 44;
    auto srcAddressOffset = srcOrigin[0] + (srcOrigin[1] * srcRowPitch) + (srcOrigin[2] * srcSlicePitch);
    auto dstAddressOffset = dstOrigin[0] + (dstOrigin[1] * dstRowPitch) + (dstOrigin[2] * dstSlicePitch);

    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(region, rootDeviceEnvironment));

    auto memoryManager = static_cast<MockMemoryManager *>(pDevice->getMemoryManager());
    memoryManager->returnFakeAllocation = true;
    // from hostPtr
    HardwareParse hwParser;
    auto offset = csr.commandStream.getUsed();
    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          csr, graphicsAllocation,
                                                                          nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(),
                                                                          0, srcOrigin, dstOrigin, region,
                                                                          srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch);

    memoryManager->returnFakeAllocation = false;
    flushBcsTask(&csr, blitProperties, true, *pDevice);
    hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

    //1st rectangle  xCopy = maxWidthToCopy, yCopy = maxHeightToCopy, zCopy = 1
    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
    EXPECT_NE(nullptr, bltCmd);
    if (pDevice->isFullRangeSvm()) {
        EXPECT_EQ(ptrOffset(reinterpret_cast<uint64_t>(hostPtr), srcAddressOffset), bltCmd->getSourceBaseAddress());
    }
    EXPECT_EQ(ptrOffset(graphicsAllocation->getGpuAddress(), dstAddressOffset), bltCmd->getDestinationBaseAddress());

    srcAddressOffset += maxWidthToCopy;
    dstAddressOffset += maxWidthToCopy;

    // 2nd rectangle xCopy = (region[0] - maxWidthToCopy), yCopy = (region[0] - maxHeightToCopy), zCopy = 1
    cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(++cmdIterator, hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
    EXPECT_NE(nullptr, bltCmd);
    if (pDevice->isFullRangeSvm()) {
        EXPECT_EQ(ptrOffset(reinterpret_cast<uint64_t>(hostPtr), srcAddressOffset), bltCmd->getSourceBaseAddress());
    }
    EXPECT_EQ(ptrOffset(graphicsAllocation->getGpuAddress(), dstAddressOffset), bltCmd->getDestinationBaseAddress());

    srcAddressOffset += (region[0] - maxWidthToCopy);
    srcAddressOffset += (srcRowPitch - region[0]);
    srcAddressOffset += (srcRowPitch * (maxHeightToCopy - 1));
    dstAddressOffset += (region[0] - maxWidthToCopy);
    dstAddressOffset += (dstRowPitch - region[0]);
    dstAddressOffset += (dstRowPitch * (maxHeightToCopy - 1));

    // 3rd rectangle xCopy = maxWidthToCopy, yCopy = maxHeightToCopy, zCopy = 1
    cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(++cmdIterator, hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
    EXPECT_NE(nullptr, bltCmd);
    if (pDevice->isFullRangeSvm()) {
        EXPECT_EQ(ptrOffset(reinterpret_cast<uint64_t>(hostPtr), srcAddressOffset), bltCmd->getSourceBaseAddress());
    }
    EXPECT_EQ(ptrOffset(graphicsAllocation->getGpuAddress(), dstAddressOffset), bltCmd->getDestinationBaseAddress());

    srcAddressOffset += maxWidthToCopy;
    dstAddressOffset += maxWidthToCopy;

    //4th rectangle  xCopy = (region[0] - maxWidthToCopy), yCopy = (region[0] - maxHeightToCopy), zCopy = 1
    cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(++cmdIterator, hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
    EXPECT_NE(nullptr, bltCmd);
    if (pDevice->isFullRangeSvm()) {
        EXPECT_EQ(ptrOffset(reinterpret_cast<uint64_t>(hostPtr), srcAddressOffset), bltCmd->getSourceBaseAddress());
    }
    EXPECT_EQ(ptrOffset(graphicsAllocation->getGpuAddress(), dstAddressOffset), bltCmd->getDestinationBaseAddress());

    srcAddressOffset += (region[0] - maxWidthToCopy);
    srcAddressOffset += (srcRowPitch - region[0]);
    srcAddressOffset += (srcRowPitch * (region[1] - maxHeightToCopy - 1));
    srcAddressOffset += (srcSlicePitch - (srcRowPitch * region[1]));
    dstAddressOffset += (region[0] - maxWidthToCopy);
    dstAddressOffset += (dstRowPitch - region[0]);
    dstAddressOffset += (dstRowPitch * (region[1] - maxHeightToCopy - 1));
    dstAddressOffset += (dstSlicePitch - (dstRowPitch * region[1]));

    //5th rectangle xCopy = maxWidthToCopy, yCopy = maxHeightToCopy, zCopy = 1
    cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(++cmdIterator, hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);
    EXPECT_NE(nullptr, bltCmd);
    if (pDevice->isFullRangeSvm()) {
        EXPECT_EQ(ptrOffset(reinterpret_cast<uint64_t>(hostPtr), srcAddressOffset), bltCmd->getSourceBaseAddress());
    }
    EXPECT_EQ(ptrOffset(graphicsAllocation->getGpuAddress(), dstAddressOffset), bltCmd->getDestinationBaseAddress());
}

HWTEST_F(BcsTests, givenAuxTranslationRequestWhenBlitCalledThenProgramCommandCorrectly) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 123, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto allocationGpuAddress = graphicsAllocation->getGpuAddress();
    auto allocationSize = graphicsAllocation->getUnderlyingBufferSize();

    AuxTranslationDirection translationDirection[] = {AuxTranslationDirection::AuxToNonAux, AuxTranslationDirection::NonAuxToAux};

    for (int i = 0; i < 2; i++) {
        auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(translationDirection[i],
                                                                                   graphicsAllocation, csr.getClearColorAllocation());

        auto offset = csr.commandStream.getUsed();
        flushBcsTask(&csr, blitProperties, false, *pDevice);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.commandStream, offset);
        uint32_t xyCopyBltCmdFound = 0;

        for (auto &cmd : hwParser.cmdList) {
            if (auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(cmd)) {
                xyCopyBltCmdFound++;
                EXPECT_EQ(static_cast<uint32_t>(allocationSize), bltCmd->getDestinationX2CoordinateRight());
                EXPECT_EQ(1u, bltCmd->getDestinationY2CoordinateBottom());

                EXPECT_EQ(allocationGpuAddress, bltCmd->getDestinationBaseAddress());
                EXPECT_EQ(allocationGpuAddress, bltCmd->getSourceBaseAddress());
            }
        }
        EXPECT_EQ(1u, xyCopyBltCmdFound);
    }
}

HWTEST_F(BcsTests, givenInvalidBlitDirectionWhenConstructPropertiesThenExceptionIsThrow) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_THROW(ClBlitProperties::constructProperties(static_cast<BlitterConstants::BlitDirection>(7), csr, {}), std::exception);
}

HWTEST_F(BcsTests, givenBlitterDirectSubmissionEnabledWhenProgrammingBlitterThenExpectRingBufferDispatched) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = true;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    using DirectSubmission = MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>;

    csr.blitterDirectSubmission = std::make_unique<DirectSubmission>(csr);
    csr.recordFlusheBatchBuffer = true;
    DirectSubmission *directSubmission = reinterpret_cast<DirectSubmission *>(csr.blitterDirectSubmission.get());
    bool initRet = directSubmission->initialize(true, false);
    EXPECT_TRUE(initRet);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    size_t numberNodesPerContainer = 5;
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                          csr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestamp0);
    blitProperties.csrDependencies.timestampPacketContainer.push_back(&timestamp1);

    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0u);
    ASSERT_NE(nullptr, csr.latestFlushedBatchBuffer.endCmdPtr);

    MI_BATCH_BUFFER_START *bbStart = hwParser.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    EXPECT_EQ(csr.latestFlushedBatchBuffer.endCmdPtr, bbStart);
    EXPECT_NE(0ull, bbStart->getBatchBufferStartAddress());
}

HWTEST_F(BcsTests, givenBlitterDirectSubmissionEnabledWhenFlushTagUpdateThenBatchBufferStartIsProgrammed) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.csrBaseCallBlitterDirectSubmissionAvailable = true;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    using DirectSubmission = MockDirectSubmissionHw<FamilyType, BlitterDispatcher<FamilyType>>;

    csr.blitterDirectSubmission = std::make_unique<DirectSubmission>(csr);
    csr.recordFlusheBatchBuffer = true;
    DirectSubmission *directSubmission = reinterpret_cast<DirectSubmission *>(csr.blitterDirectSubmission.get());
    bool initRet = directSubmission->initialize(true, false);
    EXPECT_TRUE(initRet);

    csr.flushTagUpdate();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0u);

    MI_BATCH_BUFFER_START *bbStart = hwParser.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
}

struct BcsTestsImages : public BcsTests {
    size_t rowPitch = 0;
    size_t slicePitch = 0;
};

HWTEST_F(BcsTestsImages, givenImage1DWhenAdjustBlitPropertiesForImageIsCalledThenValuesAreSetCorrectly) {
    cl_image_desc imgDesc = Image1dDefaults::imageDesc;
    imgDesc.image_width = 10u;
    imgDesc.image_height = 0u;
    imgDesc.image_depth = 0u;
    std::unique_ptr<Image> image(Image1dHelper<>::create(context.get(), &imgDesc));
    size_t expectedBytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    size_t expectedRowPitch = image->getImageDesc().image_row_pitch;
    size_t expectedSlicePitch = image->getImageDesc().image_slice_pitch;
    BlitProperties blitProperties{};
    blitProperties.dstGpuAddress = image->getGraphicsAllocation(0)->getGpuAddress();

    ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, false);

    EXPECT_EQ(imgDesc.image_width, blitProperties.dstSize.x);
    EXPECT_EQ(1u, blitProperties.dstSize.y);
    EXPECT_EQ(1u, blitProperties.dstSize.z);
    EXPECT_EQ(expectedBytesPerPixel, blitProperties.bytesPerPixel);
    EXPECT_EQ(expectedRowPitch, rowPitch);
    EXPECT_EQ(expectedSlicePitch, slicePitch);
}

HWTEST_F(BcsTestsImages, givenImage1DBufferWhenAdjustBlitPropertiesForImageIsCalledThenValuesAreSetCorrectly) {
    using BlitterConstants::BlitDirection;
    std::array<std::pair<BlitDirection, BlitDirection>, 3> testParams = {{{BlitDirection::HostPtrToImage, BlitDirection::HostPtrToBuffer},
                                                                          {BlitDirection::ImageToHostPtr, BlitDirection::BufferToHostPtr},
                                                                          {BlitDirection::ImageToImage, BlitDirection::BufferToBuffer}}};

    cl_image_desc imgDesc = Image1dBufferDefaults::imageDesc;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    cl_image_format imgFormat{};
    imgFormat.image_channel_order = CL_RGBA;
    imgFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    std::unique_ptr<Image> image(Image1dHelper<>::create(context.get(), &imgDesc, &imgFormat));
    size_t originalBytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    size_t expectedBytesPerPixel = 1;
    BlitProperties blitProperties{};
    blitProperties.srcGpuAddress = image->getGraphicsAllocation(0)->getGpuAddress();

    for (auto &[blitDirection, expectedBlitDirection] : testParams) {
        blitProperties.blitDirection = blitDirection;
        blitProperties.copySize = {1, 1, 1};
        blitProperties.srcSize = {imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth};

        ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, true);

        EXPECT_EQ(expectedBlitDirection, blitProperties.blitDirection);
        EXPECT_EQ(expectedBytesPerPixel, blitProperties.bytesPerPixel);
        EXPECT_EQ(imgDesc.image_width, blitProperties.srcSize.x / originalBytesPerPixel);
        EXPECT_EQ(imgDesc.image_height, blitProperties.srcSize.y);
        EXPECT_EQ(imgDesc.image_depth, blitProperties.srcSize.z);
        EXPECT_EQ(1u, blitProperties.copySize.x / originalBytesPerPixel);
        EXPECT_EQ(1u, blitProperties.copySize.y);
        EXPECT_EQ(1u, blitProperties.copySize.z);
    }
}

HWTEST_F(BcsTestsImages, givenImage2DArrayWhenAdjustBlitPropertiesForImageIsCalledThenValuesAreSetCorrectly) {
    cl_image_desc imgDesc = Image1dDefaults::imageDesc;
    imgDesc.image_width = 10u;
    imgDesc.image_height = 3u;
    imgDesc.image_depth = 0u;
    imgDesc.image_array_size = 4u;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;

    std::unique_ptr<Image> image(Image2dArrayHelper<>::create(context.get(), &imgDesc));
    size_t expectedBytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    size_t expectedRowPitch = image->getImageDesc().image_row_pitch;
    size_t expectedSlicePitch = image->getImageDesc().image_slice_pitch;
    BlitProperties blitProperties{};
    blitProperties.dstGpuAddress = image->getGraphicsAllocation(0)->getGpuAddress();

    ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, false);

    EXPECT_EQ(imgDesc.image_width, blitProperties.dstSize.x);
    EXPECT_EQ(imgDesc.image_height, blitProperties.dstSize.y);
    EXPECT_EQ(imgDesc.image_array_size, blitProperties.dstSize.z);
    EXPECT_EQ(expectedBytesPerPixel, blitProperties.bytesPerPixel);
    EXPECT_EQ(expectedRowPitch, rowPitch);
    EXPECT_EQ(expectedSlicePitch, slicePitch);
}

HWTEST_F(BcsTestsImages, givenImageWithSurfaceOffsetWhenAdjustBlitPropertiesForImageIsCalledThenGpuAddressIsCorrect) {
    cl_image_desc imgDesc = Image1dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dArrayHelper<>::create(context.get(), &imgDesc));

    uint64_t surfaceOffset = 0x01000;
    image->setSurfaceOffsets(surfaceOffset, 0, 0, 0);
    BlitProperties blitProperties{};
    blitProperties.dstGpuAddress = image->getGraphicsAllocation(0)->getGpuAddress();
    uint64_t expectedGpuAddress = blitProperties.dstGpuAddress + surfaceOffset;

    ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, false);

    EXPECT_EQ(blitProperties.dstGpuAddress, expectedGpuAddress);
}

HWTEST_F(BcsTestsImages, givenImageWithPlaneSetWhenAdjustBlitPropertiesForImageIsCalledThenPlaneIsCorrect) {
    cl_image_desc imgDesc = Image1dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dArrayHelper<>::create(context.get(), &imgDesc));

    BlitProperties blitProperties{};

    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_NO_PLANE, blitProperties.dstPlane);
    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_NO_PLANE, blitProperties.srcPlane);

    ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, false);
    ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, true);
    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_NO_PLANE, blitProperties.dstPlane);
    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_NO_PLANE, blitProperties.srcPlane);

    image->setPlane(GMM_YUV_PLANE_ENUM::GMM_PLANE_Y);
    ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, false);
    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_PLANE_Y, blitProperties.dstPlane);
    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_NO_PLANE, blitProperties.srcPlane);

    image->setPlane(GMM_YUV_PLANE_ENUM::GMM_PLANE_U);
    ClBlitProperties::adjustBlitPropertiesForImage(image.get(), blitProperties, rowPitch, slicePitch, true);
    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_PLANE_Y, blitProperties.dstPlane);
    EXPECT_EQ(GMM_YUV_PLANE_ENUM::GMM_PLANE_U, blitProperties.srcPlane);
}

HWTEST_F(BcsTests, givenHostPtrToImageWhenConstructPropertiesIsCalledThenValuesAreSetCorrectly) {
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.image_width = 10u;
    imgDesc.image_height = 12u;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.srcMemObj = nullptr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {2, 3, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto expectedDstPtr = image->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    auto expectedBytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    auto srcRowPitchExpected = expectedBytesPerPixel * builtinOpParams.size.x;
    auto dstRowPitchExpected = image->getImageDesc().image_row_pitch;
    auto srcSlicePitchExpected = srcRowPitchExpected * builtinOpParams.size.y;
    auto dstSlicePitchExpected = image->getImageDesc().image_slice_pitch;

    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToImage,
                                                                csr,
                                                                builtinOpParams);

    EXPECT_EQ(builtinOpParams.size, blitProperties.copySize);
    EXPECT_EQ(expectedDstPtr, blitProperties.dstGpuAddress);
    EXPECT_EQ(builtinOpParams.srcOffset, blitProperties.srcOffset);
    EXPECT_EQ(builtinOpParams.dstOffset, blitProperties.dstOffset);
    EXPECT_EQ(expectedBytesPerPixel, blitProperties.bytesPerPixel);
    EXPECT_EQ(srcRowPitchExpected, blitProperties.srcRowPitch);
    EXPECT_EQ(dstRowPitchExpected, blitProperties.dstRowPitch);
    EXPECT_EQ(srcSlicePitchExpected, blitProperties.srcSlicePitch);
    EXPECT_EQ(dstSlicePitchExpected, blitProperties.dstSlicePitch);
}

HWTEST_F(BcsTests, givenImageToHostPtrWhenConstructPropertiesIsCalledThenValuesAreSetCorrectly) {
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.image_width = 10u;
    imgDesc.image_height = 12u;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.dstPtr = hostPtr;
    builtinOpParams.srcMemObj = image.get();
    builtinOpParams.dstMemObj = nullptr;
    builtinOpParams.size = {2, 3, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto expectedSrcPtr = image->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    auto expectedBytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    auto srcRowPitchExpected = image->getImageDesc().image_row_pitch;
    auto dstRowPitchExpected = expectedBytesPerPixel * builtinOpParams.size.x;
    auto srcSlicePitchExpected = image->getImageDesc().image_slice_pitch;
    auto dstSlicePitchExpected = dstRowPitchExpected * builtinOpParams.size.y;

    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::ImageToHostPtr,
                                                                csr,
                                                                builtinOpParams);

    EXPECT_EQ(builtinOpParams.size, blitProperties.copySize);
    EXPECT_EQ(expectedSrcPtr, blitProperties.srcGpuAddress);
    EXPECT_EQ(builtinOpParams.srcOffset, blitProperties.srcOffset);
    EXPECT_EQ(builtinOpParams.dstOffset, blitProperties.dstOffset);
    EXPECT_EQ(expectedBytesPerPixel, blitProperties.bytesPerPixel);
    EXPECT_EQ(srcRowPitchExpected, blitProperties.srcRowPitch);
    EXPECT_EQ(dstRowPitchExpected, blitProperties.dstRowPitch);
    EXPECT_EQ(srcSlicePitchExpected, blitProperties.srcSlicePitch);
    EXPECT_EQ(dstSlicePitchExpected, blitProperties.dstSlicePitch);
}

HWTEST_F(BcsTests, givenHostPtrToImageWithInputRowSlicePitchesWhenConstructPropertiesIsCalledThenValuesAreSetCorrectly) {
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.srcMemObj = nullptr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {2, 3, 1};
    auto inputRowPitch = 0x20u;
    auto inputSlicePitch = 0x400u;
    builtinOpParams.srcRowPitch = inputRowPitch;
    builtinOpParams.srcSlicePitch = inputSlicePitch;
    auto dstRowPitchExpected = image->getImageDesc().image_row_pitch;
    auto dstSlicePitchExpected = image->getImageDesc().image_slice_pitch;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToImage,
                                                                csr,
                                                                builtinOpParams);

    EXPECT_EQ(inputRowPitch, blitProperties.srcRowPitch);
    EXPECT_EQ(dstRowPitchExpected, blitProperties.dstRowPitch);
    EXPECT_EQ(inputSlicePitch, blitProperties.srcSlicePitch);
    EXPECT_EQ(dstSlicePitchExpected, blitProperties.dstSlicePitch);
}

HWTEST_F(BcsTests, givenImageToHostPtrWithInputRowSlicePitchesWhenConstructPropertiesIsCalledThenValuesAreSetCorrectly) {
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.dstPtr = hostPtr;
    builtinOpParams.srcMemObj = image.get();
    builtinOpParams.dstMemObj = nullptr;
    builtinOpParams.size = {2, 3, 1};
    auto inputRowPitch = 0x20u;
    auto inputSlicePitch = 0x400u;
    builtinOpParams.dstRowPitch = inputRowPitch;
    builtinOpParams.dstSlicePitch = inputSlicePitch;

    auto srcRowPitchExpected = image->getImageDesc().image_row_pitch;
    auto srcSlicePitchExpected = image->getImageDesc().image_slice_pitch;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::ImageToHostPtr,
                                                                csr,
                                                                builtinOpParams);

    EXPECT_EQ(srcRowPitchExpected, blitProperties.srcRowPitch);
    EXPECT_EQ(inputRowPitch, blitProperties.dstRowPitch);
    EXPECT_EQ(srcSlicePitchExpected, blitProperties.srcSlicePitch);
    EXPECT_EQ(inputSlicePitch, blitProperties.dstSlicePitch);
}

HWTEST_F(BcsTests, givenHostPtrToImageWhenBlitBufferIsCalledThenBlitCmdIsFound) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get()));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {1, 1, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToImage,
                                                                csr,
                                                                builtinOpParams);
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_BLOCK_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);
}
