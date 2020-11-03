/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "shared/test/unit_test/mocks/mock_direct_submission_hw.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/cl_blit_properties.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/helpers/raii_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"
#include "opencl/test/unit_test/mocks/mock_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_image.h"
#include "opencl/test/unit_test/mocks/mock_internal_allocation_storage.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_timestamp_container.h"
#include "test.h"

using namespace NEO;

HWTEST_F(BcsTests, givenBltSizeWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    constexpr auto cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);
    size_t notAlignedBltSize = (3 * max2DBlitSize) + 1;
    size_t alignedBltSize = (3 * max2DBlitSize);
    uint32_t alignedNumberOfBlts = 3;
    uint32_t notAlignedNumberOfBlts = 4;

    auto expectedAlignedSize = cmdsSizePerBlit * alignedNumberOfBlts;
    auto expectedNotAlignedSize = cmdsSizePerBlit * notAlignedNumberOfBlts;
    auto alignedCopySize = Vec3<size_t>{alignedBltSize, 1, 1};
    auto notAlignedCopySize = Vec3<size_t>{notAlignedBltSize, 1, 1};

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        alignedCopySize, csrDependencies, false, false, pClDevice->getRootDeviceEnvironment());
    auto notAlignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        notAlignedCopySize, csrDependencies, false, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
    EXPECT_EQ(expectedNotAlignedSize, notAlignedEstimatedSize);
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(alignedCopySize, pClDevice->getRootDeviceEnvironment()));
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(notAlignedCopySize, pClDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BcsTests, givenDebugCapabilityWhenEstimatingCommandSizeThenAddAllRequiredCommands) {
    constexpr auto max2DBlitSize = BlitterConstants::maxBlitWidth * BlitterConstants::maxBlitHeight;
    constexpr auto cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);
    const size_t debugCommandsSize = (EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite() + EncodeSempahore<FamilyType>::getSizeMiSemaphoreWait()) * 2;

    constexpr uint32_t numberOfBlts = 3;
    constexpr size_t bltSize = (numberOfBlts * max2DBlitSize);

    auto expectedSize = (cmdsSizePerBlit * numberOfBlts) + debugCommandsSize + MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(pDevice->getHardwareInfo()) +
                        EncodeMiFlushDW<FamilyType>::getMiFlushDwCmdSizeForDataWrite() + sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    expectedSize = alignUp(expectedSize, MemoryConstants::cacheLineSize);

    BlitProperties blitProperties;
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
    constexpr auto cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);
    Vec3<size_t> notAlignedBltSize = {(3 * max2DBlitSize) + 1, 4, 2};
    Vec3<size_t> alignedBltSize = {(3 * max2DBlitSize), 4, 2};
    size_t alignedNumberOfBlts = 3 * alignedBltSize.y * alignedBltSize.z;
    size_t notAlignedNumberOfBlts = 4 * notAlignedBltSize.y * notAlignedBltSize.z;

    auto expectedAlignedSize = cmdsSizePerBlit * alignedNumberOfBlts;
    auto expectedNotAlignedSize = cmdsSizePerBlit * notAlignedNumberOfBlts;

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        alignedBltSize, csrDependencies, false, false, pClDevice->getRootDeviceEnvironment());
    auto notAlignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        notAlignedBltSize, csrDependencies, false, false, pClDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expectedAlignedSize, alignedEstimatedSize);
    EXPECT_EQ(expectedNotAlignedSize, notAlignedEstimatedSize);
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(notAlignedBltSize, pClDevice->getRootDeviceEnvironment()));
    EXPECT_FALSE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(alignedBltSize, pClDevice->getRootDeviceEnvironment()));
}

HWTEST_F(BcsTests, givenBltWithBigCopySizeWhenEstimatingCommandSizeForReadBufferRectThenAddAllRequiredCommands) {
    auto &rootDeviceEnvironment = pClDevice->getRootDeviceEnvironment();
    auto maxWidthToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitWidth(rootDeviceEnvironment));
    auto maxHeightToCopy = static_cast<size_t>(BlitCommandsHelper<FamilyType>::getMaxBlitHeight(rootDeviceEnvironment));

    constexpr auto cmdsSizePerBlit = sizeof(typename FamilyType::XY_COPY_BLT) + sizeof(typename FamilyType::MI_ARB_CHECK);
    Vec3<size_t> alignedBltSize = {(3 * maxWidthToCopy), (4 * maxHeightToCopy), 2};
    Vec3<size_t> notAlignedBltSize = {(3 * maxWidthToCopy + 1), (4 * maxHeightToCopy), 2};

    EXPECT_TRUE(BlitCommandsHelper<FamilyType>::isCopyRegionPreferred(alignedBltSize, rootDeviceEnvironment));

    size_t alignedNumberOfBlts = (3 * 4 * alignedBltSize.z);
    size_t notAlignedNumberOfBlts = (4 * 4 * notAlignedBltSize.z);

    auto expectedAlignedSize = cmdsSizePerBlit * alignedNumberOfBlts;
    auto expectedNotAlignedSize = cmdsSizePerBlit * notAlignedNumberOfBlts;

    auto alignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        alignedBltSize, csrDependencies, false, false, rootDeviceEnvironment);
    auto notAlignedEstimatedSize = BlitCommandsHelper<FamilyType>::estimateBlitCommandsSize(
        notAlignedBltSize, csrDependencies, false, false, rootDeviceEnvironment);

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

HWTEST_F(BcsTests, givenCsrDependenciesWhenProgrammingCommandStreamThenAddSemaphoreAndAtomic) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    uint32_t numberOfDependencyContainers = 2;
    size_t numberNodesPerContainer = 5;
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, graphicsAllocation, nullptr, hostPtr,
                                                                                graphicsAllocation->getGpuAddress(), 0,
                                                                                0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    blitProperties.csrDependencies.push_back(&timestamp0);
    blitProperties.csrDependencies.push_back(&timestamp1);

    blitBuffer(&csr, blitProperties, true);

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
            auto miAtomic = genCmdCast<typename FamilyType::MI_ATOMIC *>(*(++cmdIterator));
            EXPECT_NE(nullptr, miAtomic);

            for (uint32_t i = 1; i < numberOfDependencyContainers * numberNodesPerContainer; i++) {
                EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_SEMAPHORE_WAIT *>(*(++cmdIterator)));
                EXPECT_NE(nullptr, genCmdCast<typename FamilyType::MI_ATOMIC *>(*(++cmdIterator)));
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
    void *hostPtr1 = reinterpret_cast<void *>(0x12340000);
    void *hostPtr2 = reinterpret_cast<void *>(0x12340000);
    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties1 = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                 csr, graphicsAllocation1, nullptr, hostPtr1,
                                                                                 graphicsAllocation1->getGpuAddress(), 0,
                                                                                 0, 0, {1, 1, 1}, 0, 0, 0, 0);
    auto blitProperties2 = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                 csr, graphicsAllocation2, nullptr, hostPtr2,
                                                                                 graphicsAllocation2->getGpuAddress(), 0,
                                                                                 0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), 1);
    MockTimestampPacketContainer timestamp2(*csr.getTimestampPacketAllocator(), 1);
    blitProperties1.csrDependencies.push_back(&timestamp1);
    blitProperties2.csrDependencies.push_back(&timestamp2);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties1);
    blitPropertiesContainer.push_back(blitProperties2);

    csr.blitBuffer(blitPropertiesContainer, true, false);

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

HWTEST_F(BcsTests, givenProfilingEnabledWhenBlitBufferThenCommandBufferIsConstructedProperly) {
    auto bcsOsContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, pDevice->getDeviceBitfield(), aub_stream::ENGINE_BCS, PreemptionMode::Disabled,
                                                                     false, false, false));
    auto bcsCsr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    bcsCsr->setupContext(*bcsOsContext);
    bcsCsr->initializeTagAllocation();

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                *bcsCsr, graphicsAllocation, nullptr, hostPtr,
                                                                                graphicsAllocation->getGpuAddress(), 0,
                                                                                0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp(*bcsCsr->getTimestampPacketAllocator(), 1u);
    blitProperties.outputTimestampPacket = timestamp.getNode(0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    bcsCsr->blitBuffer(blitPropertiesContainer, false, true);

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

HWTEST_F(BcsTests, givenInputAllocationsWhenBlitDispatchedThenMakeAllAllocationsResident) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr1 = reinterpret_cast<void *>(0x12340000);
    void *hostPtr2 = reinterpret_cast<void *>(0x43210000);

    EXPECT_EQ(0u, csr.makeSurfacePackNonResidentCalled);
    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties1 = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                 csr, graphicsAllocation1, nullptr, hostPtr1,
                                                                                 graphicsAllocation1->getGpuAddress(), 0,
                                                                                 0, 0, {1, 1, 1}, 0, 0, 0, 0);

    auto blitProperties2 = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                 csr, graphicsAllocation2, nullptr, hostPtr2,
                                                                                 graphicsAllocation2->getGpuAddress(), 0,
                                                                                 0, 0, {1, 1, 1}, 0, 0, 0, 0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties1);
    blitPropertiesContainer.push_back(blitProperties2);

    csr.blitBuffer(blitPropertiesContainer, false, false);

    EXPECT_TRUE(csr.isMadeResident(graphicsAllocation1));
    EXPECT_TRUE(csr.isMadeResident(graphicsAllocation2));
    EXPECT_TRUE(csr.isMadeResident(csr.getTagAllocation()));
    EXPECT_EQ(1u, csr.makeSurfacePackNonResidentCalled);

    EXPECT_EQ(csr.globalFenceAllocation ? 6u : 5u, csr.makeResidentAllocations.size());
}

HWTEST_F(BcsTests, givenFenceAllocationIsRequiredWhenBlitDispatchedThenMakeAllAllocationsResident) {
    RAIIHwHelperFactory<MockHwHelperWithFenceAllocation<FamilyType>> hwHelperBackup{pDevice->getHardwareInfo().platform.eRenderCoreFamily};

    auto bcsOsContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, 0, pDevice->getDeviceBitfield(), aub_stream::ENGINE_BCS, PreemptionMode::Disabled,
                                                                     false, false, false));
    auto bcsCsr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    bcsCsr->setupContext(*bcsOsContext);
    bcsCsr->initializeTagAllocation();
    bcsCsr->createGlobalFenceAllocation();
    bcsCsr->storeMakeResidentAllocations = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer1 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto buffer2 = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr1 = reinterpret_cast<void *>(0x12340000);
    void *hostPtr2 = reinterpret_cast<void *>(0x43210000);

    EXPECT_EQ(0u, bcsCsr->makeSurfacePackNonResidentCalled);
    auto graphicsAllocation1 = buffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    auto graphicsAllocation2 = buffer2->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties1 = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                 *bcsCsr, graphicsAllocation1, nullptr, hostPtr1,
                                                                                 graphicsAllocation1->getGpuAddress(), 0,
                                                                                 0, 0, {1, 1, 1}, 0, 0, 0, 0);

    auto blitProperties2 = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                 *bcsCsr, graphicsAllocation2, nullptr, hostPtr2,
                                                                                 graphicsAllocation2->getGpuAddress(), 0,
                                                                                 0, 0, {1, 1, 1}, 0, 0, 0, 0);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties1);
    blitPropertiesContainer.push_back(blitProperties2);

    bcsCsr->blitBuffer(blitPropertiesContainer, false, false);

    EXPECT_TRUE(bcsCsr->isMadeResident(graphicsAllocation1));
    EXPECT_TRUE(bcsCsr->isMadeResident(graphicsAllocation2));
    EXPECT_TRUE(bcsCsr->isMadeResident(bcsCsr->getTagAllocation()));
    EXPECT_TRUE(bcsCsr->isMadeResident(bcsCsr->globalFenceAllocation));
    EXPECT_EQ(1u, bcsCsr->makeSurfacePackNonResidentCalled);

    EXPECT_EQ(6u, bcsCsr->makeResidentAllocations.size());
}

HWTEST_F(BcsTests, givenBufferWhenBlitCalledThenFlushCommandBuffer) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.recordFlusheBatchBuffer = true;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto &commandStream = csr.getCS(MemoryConstants::pageSize);
    size_t commandStreamOffset = 4;
    commandStream.getSpace(commandStreamOffset);

    uint32_t newTaskCount = 17;
    csr.taskCount = newTaskCount - 1;

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, graphicsAllocation, nullptr, hostPtr,
                                                                                graphicsAllocation->getGpuAddress(), 0,
                                                                                0, 0, {1, 1, 1}, 0, 0, 0, 0);

    blitBuffer(&csr, blitProperties, true);

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

HWTEST_F(BcsTests, whenBlitFromHostPtrCalledThenCallWaitWithKmdFallback) {
    class MyMockCsr : public UltCommandStreamReceiver<FamilyType> {
      public:
        using UltCommandStreamReceiver<FamilyType>::UltCommandStreamReceiver;

        void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait,
                                                   bool useQuickKmdSleep, bool forcePowerSavingMode) override {
            waitForTaskCountWithKmdNotifyFallbackCalled++;
            taskCountToWaitPassed = taskCountToWait;
            flushStampToWaitPassed = flushStampToWait;
            useQuickKmdSleepPassed = useQuickKmdSleep;
            forcePowerSavingModePassed = forcePowerSavingMode;
        }

        uint32_t taskCountToWaitPassed = 0;
        FlushStamp flushStampToWaitPassed = 0;
        bool useQuickKmdSleepPassed = false;
        bool forcePowerSavingModePassed = false;
        uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
    };

    auto myMockCsr = std::make_unique<::testing::NiceMock<MyMockCsr>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    auto &bcsOsContext = pDevice->getUltCommandStreamReceiver<FamilyType>().getOsContext();
    myMockCsr->initializeTagAllocation();
    myMockCsr->setupContext(bcsOsContext);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                *myMockCsr, graphicsAllocation, nullptr,
                                                                                hostPtr,
                                                                                graphicsAllocation->getGpuAddress(), 0,
                                                                                0, 0, {1, 1, 1}, 0, 0, 0, 0);

    blitBuffer(myMockCsr.get(), blitProperties, false);

    EXPECT_EQ(0u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);

    blitBuffer(myMockCsr.get(), blitProperties, true);

    EXPECT_EQ(1u, myMockCsr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_EQ(myMockCsr->taskCount, myMockCsr->taskCountToWaitPassed);
    EXPECT_EQ(myMockCsr->flushStamp->peekStamp(), myMockCsr->flushStampToWaitPassed);
    EXPECT_FALSE(myMockCsr->useQuickKmdSleepPassed);
    EXPECT_FALSE(myMockCsr->forcePowerSavingModePassed);
}

HWTEST_F(BcsTests, whenBlitFromHostPtrCalledThenCleanTemporaryAllocations) {
    auto &bcsCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto mockInternalAllocationsStorage = new MockInternalAllocationStorage(bcsCsr);
    bcsCsr.internalAllocationStorage.reset(mockInternalAllocationsStorage);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    bcsCsr.taskCount = 17;

    EXPECT_EQ(0u, mockInternalAllocationsStorage->cleanAllocationsCalled);

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                bcsCsr, graphicsAllocation, nullptr, hostPtr,
                                                                                graphicsAllocation->getGpuAddress(), 0,
                                                                                0, 0, {1, 1, 1}, 0, 0, 0, 0);

    blitBuffer(&bcsCsr, blitProperties, false);

    EXPECT_EQ(0u, mockInternalAllocationsStorage->cleanAllocationsCalled);

    blitBuffer(&bcsCsr, blitProperties, true);

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

    void *hostPtr = reinterpret_cast<void *>(0x12340000);
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
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                        csr, graphicsAllocation1,
                                                                                        nullptr, hostPtr,
                                                                                        graphicsAllocation1->getGpuAddress() +
                                                                                            subBuffer1->getOffset(),
                                                                                        0, {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);

            blitBuffer(&csr, blitProperties, true);

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
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                        csr, graphicsAllocation1,
                                                                                        nullptr, hostPtr,
                                                                                        graphicsAllocation1->getGpuAddress() +
                                                                                            subBuffer1->getOffset(),
                                                                                        0, {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);

            blitBuffer(&csr, blitProperties, true);

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
            auto blitProperties = BlitProperties::constructPropertiesForCopyBuffer(graphicsAllocation1,
                                                                                   graphicsAllocation2, 0, 0, copySize, 0, 0, 0, 0);

            blitBuffer(&csr, blitProperties, true);

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
            blitBuffer(&csr, blitProperties, true);

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

    AllocationProperties properties{csr.getRootDeviceIndex(), false, 1234, GraphicsAllocation::AllocationType::MAP_ALLOCATION, false, pDevice->getDeviceBitfield()};
    GraphicsAllocation *mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, reinterpret_cast<void *>(0x12340000));

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
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                        csr, graphicsAllocation,
                                                                                        mapAllocation, mapPtr,
                                                                                        graphicsAllocation->getGpuAddress(),
                                                                                        castToUint64(mapPtr),
                                                                                        {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);

            blitBuffer(&csr, blitProperties, true);
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
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                        csr, graphicsAllocation,
                                                                                        mapAllocation, mapPtr,
                                                                                        graphicsAllocation->getGpuAddress(),
                                                                                        castToUint64(mapPtr), {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);
            blitBuffer(&csr, blitProperties, true);
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
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                        csr, graphicsAllocation,
                                                                                        mapAllocation, mapPtr,
                                                                                        graphicsAllocation->getGpuAddress(),
                                                                                        castToUint64(mapPtr), {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);
            blitBuffer(&csr, blitProperties, true);
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
            auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                        csr, graphicsAllocation,
                                                                                        mapAllocation, mapPtr,
                                                                                        graphicsAllocation->getGpuAddress(),
                                                                                        castToUint64(mapPtr),
                                                                                        {hostPtrOffset, 0, 0}, 0, copySize, 0, 0, 0, 0);
            blitBuffer(&csr, blitProperties, true);
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
    auto memoryManager = csr.getMemoryManager();

    AllocationProperties properties{csr.getRootDeviceIndex(), false, 1234, GraphicsAllocation::AllocationType::MAP_ALLOCATION, false, pDevice->getDeviceBitfield()};
    GraphicsAllocation *mapAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, reinterpret_cast<void *>(0x12340000));

    auto mapAllocationOffset = 0x1234;
    auto mapPtr = reinterpret_cast<void *>(mapAllocation->getGpuAddress() + mapAllocationOffset);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 100, nullptr, retVal));

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

    memoryManager->freeGraphicsMemory(mapAllocation);
}

HWTEST_F(BcsTests, givenNonZeroCopySvmAllocationWhenConstructingBlitPropertiesForReadWriteBufferCallThenSetValidAllocations) {
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    MockMemoryManager mockMemoryManager(true, true);
    SVMAllocsManager svmAllocsManager(&mockMemoryManager);

    auto svmAllocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_WRITE);
    auto svmAlloc = svmAllocsManager.createSVMAlloc(csr.getRootDeviceIndex(), 1, svmAllocationProperties, pDevice->getDeviceBitfield());
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
    SVMAllocsManager svmAllocsManager(&mockMemoryManager);

    auto svmAllocationProperties = MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_WRITE);
    auto svmAlloc = svmAllocsManager.createSVMAlloc(csr.getRootDeviceIndex(), 1, svmAllocationProperties, pDevice->getDeviceBitfield());
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

            blitBuffer(&csr, blitProperties, true);

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
            blitBuffer(&csr, blitProperties, true);

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
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
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
                auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                            csr, graphicsAllocation1,
                                                                                            nullptr, hostPtr,
                                                                                            graphicsAllocation1->getGpuAddress(),
                                                                                            0, 0, {buffer1Offset, 0, 0}, copySize, 0, 0, 0, 0);

                blitBuffer(&csr, blitProperties, true);

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
                auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::BufferToHostPtr,
                                                                                            csr, graphicsAllocation1, nullptr,
                                                                                            hostPtr,
                                                                                            graphicsAllocation1->getGpuAddress(),
                                                                                            0, 0, {buffer1Offset, 0, 0}, copySize, 0, 0, 0, 0);

                blitBuffer(&csr, blitProperties, true);

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
                auto blitProperties = BlitProperties::constructPropertiesForCopyBuffer(graphicsAllocation1,
                                                                                       graphicsAllocation2,
                                                                                       {buffer1Offset, 0, 0}, {buffer2Offset, 0, 0}, copySize, 0, 0, 0, 0);

                blitBuffer(&csr, blitProperties, true);

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
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
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

    // from hostPtr
    HardwareParse hwParser;
    auto offset = csr.commandStream.getUsed();
    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, graphicsAllocation,
                                                                                nullptr, hostPtr,
                                                                                graphicsAllocation->getGpuAddress(),
                                                                                0, srcOrigin, dstOrigin, region,
                                                                                srcRowPitch, srcSlicePitch, dstRowPitch, dstSlicePitch);

    blitBuffer(&csr, blitProperties, true);
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
                                                                                   graphicsAllocation);

        auto offset = csr.commandStream.getUsed();
        blitBuffer(&csr, blitProperties, false);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr.commandStream, offset);
        uint32_t xyCopyBltCmdFound = 0;

        for (auto &cmd : hwParser.cmdList) {
            if (auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(cmd)) {
                xyCopyBltCmdFound++;
                EXPECT_EQ(static_cast<uint32_t>(allocationSize), bltCmd->getTransferWidth());
                EXPECT_EQ(1u, bltCmd->getTransferHeight());

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

    csr.blitterDirectSubmission = std::make_unique<DirectSubmission>(*pDevice, *csr.osContext);
    csr.recordFlusheBatchBuffer = true;
    DirectSubmission *directSubmission = reinterpret_cast<DirectSubmission *>(csr.blitterDirectSubmission.get());
    bool initRet = directSubmission->initialize(true);
    EXPECT_TRUE(initRet);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    size_t numberNodesPerContainer = 5;
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWriteBuffer(BlitterConstants::BlitDirection::HostPtrToBuffer,
                                                                                csr, graphicsAllocation, nullptr, hostPtr,
                                                                                graphicsAllocation->getGpuAddress(), 0,
                                                                                0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp0(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    MockTimestampPacketContainer timestamp1(*csr.getTimestampPacketAllocator(), numberNodesPerContainer);
    blitProperties.csrDependencies.push_back(&timestamp0);
    blitProperties.csrDependencies.push_back(&timestamp1);

    blitBuffer(&csr, blitProperties, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0u);
    ASSERT_NE(nullptr, csr.latestFlushedBatchBuffer.endCmdPtr);

    MI_BATCH_BUFFER_START *bbStart = hwParser.getCommand<MI_BATCH_BUFFER_START>();
    ASSERT_NE(nullptr, bbStart);
    EXPECT_EQ(csr.latestFlushedBatchBuffer.endCmdPtr, bbStart);
    EXPECT_EQ(0ull, bbStart->getBatchBufferStartAddressGraphicsaddress472());
}

HWTEST_F(BcsTests, givenHostPtrToImageWhenConstructPropertiesIsCalledThenValuesAreSetCorrectly) {
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
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
    auto expectedDstPtr = image.get()->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    auto expectedBytesPerPixel = image.get()->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    auto srcRowPitchExpected = expectedBytesPerPixel * builtinOpParams.size.x;
    auto dstRowPitchExpected = expectedBytesPerPixel * image.get()->getImageDesc().image_width;
    auto srcSlicePitchExpected = srcRowPitchExpected * builtinOpParams.size.y;
    auto dstSlicePitchExpected = dstRowPitchExpected * image.get()->getImageDesc().image_height;

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
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
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
    auto expectedSrcPtr = image.get()->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    auto expectedBytesPerPixel = image.get()->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    auto srcRowPitchExpected = expectedBytesPerPixel * image.get()->getImageDesc().image_width;
    auto dstRowPitchExpected = expectedBytesPerPixel * builtinOpParams.size.x;
    auto srcSlicePitchExpected = srcRowPitchExpected * image.get()->getImageDesc().image_height;
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
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.srcMemObj = nullptr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {2, 3, 1};
    auto inputRowPitch = 0x20u;
    auto inputSlicePitch = 0x400u;
    builtinOpParams.dstRowPitch = inputRowPitch;
    builtinOpParams.dstSlicePitch = inputSlicePitch;

    auto expectedBytesPerPixel = image.get()->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    auto dstRowPitchExpected = expectedBytesPerPixel * image.get()->getImageDesc().image_width;
    auto dstSlicePitchExpected = dstRowPitchExpected * image.get()->getImageDesc().image_height;

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
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.dstPtr = hostPtr;
    builtinOpParams.srcMemObj = image.get();
    builtinOpParams.dstMemObj = nullptr;
    builtinOpParams.size = {2, 3, 1};
    auto inputRowPitch = 0x20u;
    auto inputSlicePitch = 0x400u;
    builtinOpParams.srcRowPitch = inputRowPitch;
    builtinOpParams.srcSlicePitch = inputSlicePitch;

    auto expectedBytesPerPixel = image.get()->getSurfaceFormatInfo().surfaceFormat.ImageElementSizeInBytes;
    auto srcRowPitchExpected = expectedBytesPerPixel * image.get()->getImageDesc().image_width;
    auto srcSlicePitchExpected = srcRowPitchExpected * image.get()->getImageDesc().image_height;

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
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get()));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.dstMemObj = image.get();

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToImage,
                                                                csr,
                                                                builtinOpParams);
    blitBuffer(&csr, blitProperties, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);
}

HWTEST_F(BcsTests, given3dImageWhenBlitBufferIsCalledThenBlitCmdIsFoundZtimes) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    std::unique_ptr<Image> image(Image3dHelper<>::create(context.get()));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {1, 1, 10};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToImage,
                                                                csr,
                                                                builtinOpParams);
    blitBuffer(&csr, blitProperties, true);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    uint32_t xyCopyBltCmdFound = 0;

    for (auto &cmd : hwParser.cmdList) {
        if (auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(cmd)) {
            ++xyCopyBltCmdFound;
        }
    }
    EXPECT_EQ(static_cast<uint32_t>(builtinOpParams.size.z), xyCopyBltCmdFound);
}

HWTEST_F(BcsTests, givenImageToHostPtrWhenBlitBufferIsCalledThenBlitCmdIsFound) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get()));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.dstPtr = hostPtr;
    builtinOpParams.srcMemObj = image.get();

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::ImageToHostPtr,
                                                                csr,
                                                                builtinOpParams);
    blitBuffer(&csr, blitProperties, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);
}

HWTEST_F(BcsTests, givenHostPtrToImageWhenBlitBufferIsCalledThenBlitCmdIsCorrectlyProgrammed) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.image_width = 10;
    imgDesc.image_height = 12;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.srcMemObj = nullptr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {6, 8, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::HostPtrToImage,
                                                                csr,
                                                                builtinOpParams);
    blitBuffer(&csr, blitProperties, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);

    auto dstPtr = builtinOpParams.dstMemObj->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    EXPECT_EQ(blitProperties.srcGpuAddress, bltCmd->getSourceBaseAddress());
    EXPECT_EQ(dstPtr, bltCmd->getDestinationBaseAddress());
}

HWTEST_F(BcsTests, givenImageToHostPtrWhenBlitBufferIsCalledThenBlitCmdIsCorrectlyProgrammed) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
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
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::ImageToHostPtr,
                                                                csr,
                                                                builtinOpParams);
    blitBuffer(&csr, blitProperties, true);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    auto bltCmd = genCmdCast<typename FamilyType::XY_COPY_BLT *>(*cmdIterator);

    auto srcPtr = builtinOpParams.srcMemObj->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    EXPECT_EQ(srcPtr, bltCmd->getSourceBaseAddress());
    EXPECT_EQ(blitProperties.dstGpuAddress, bltCmd->getDestinationBaseAddress());
}

struct MockScratchSpaceController : ScratchSpaceControllerBase {
    using ScratchSpaceControllerBase::privateScratchAllocation;
    using ScratchSpaceControllerBase::ScratchSpaceControllerBase;
};

using ScratchSpaceControllerTest = Test<ClDeviceFixture>;

TEST_F(ScratchSpaceControllerTest, whenScratchSpaceControllerIsDestroyedThenItReleasePrivateScratchSpaceAllocation) {
    MockScratchSpaceController scratchSpaceController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *pDevice->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    scratchSpaceController.privateScratchAllocation = pDevice->getExecutionEnvironment()->memoryManager->allocateGraphicsMemoryInPreferredPool(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize}, nullptr);
    EXPECT_NE(nullptr, scratchSpaceController.privateScratchAllocation);
    //no memory leak is expected
}

TEST(BcsConstantsTests, givenBlitConstantsThenTheyHaveDesiredValues) {
    EXPECT_EQ(BlitterConstants::maxBlitWidth, 0x4000u);
    EXPECT_EQ(BlitterConstants::maxBlitHeight, 0x4000u);
    EXPECT_EQ(BlitterConstants::maxBlitSetWidth, 0x1FF80u);
    EXPECT_EQ(BlitterConstants::maxBlitSetHeight, 0x1FFC0u);
}
