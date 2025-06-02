/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"

#include "opencl/source/helpers/cl_blit_properties.h"
#include "opencl/test/unit_test/command_stream/command_stream_receiver_hw_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"

using namespace NEO;
struct MockScratchSpaceController : ScratchSpaceControllerBase {
    using ScratchSpaceControllerBase::scratchSlot1Allocation;
    using ScratchSpaceControllerBase::ScratchSpaceControllerBase;
};

using ScratchSpaceControllerTest = Test<ClDeviceFixture>;

TEST_F(ScratchSpaceControllerTest, whenScratchSpaceControllerIsDestroyedThenItReleasePrivateScratchSpaceAllocation) {
    MockScratchSpaceController scratchSpaceController(pDevice->getRootDeviceIndex(), *pDevice->getExecutionEnvironment(), *pDevice->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    scratchSpaceController.scratchSlot1Allocation = pDevice->getExecutionEnvironment()->memoryManager->allocateGraphicsMemoryInPreferredPool(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize}, nullptr);
    EXPECT_NE(nullptr, scratchSpaceController.scratchSlot1Allocation);
    // no memory leak is expected
}

HWTEST_F(BcsTests, given3dImageWhenBlitBufferIsCalledThenBlitCmdIsFoundZtimes) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    std::unique_ptr<Image> image(Image3dHelperUlt<>::create(context.get()));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {1, 1, 10};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::hostPtrToImage,
                                                                csr,
                                                                builtinOpParams);
    flushBcsTask(&csr, blitProperties, true, *pDevice);
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    uint32_t xyCopyBltCmdFound = 0;

    for (auto &cmd : hwParser.cmdList) {
        if (auto bltCmd = genCmdCast<typename FamilyType::XY_BLOCK_COPY_BLT *>(cmd)) {
            ++xyCopyBltCmdFound;
        }
    }
    EXPECT_EQ(static_cast<uint32_t>(builtinOpParams.size.z), xyCopyBltCmdFound);
}

HWTEST_F(BcsTests, givenImageToHostPtrWhenBlitBufferIsCalledThenBlitCmdIsFound) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(context.get()));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.dstPtr = hostPtr;
    builtinOpParams.srcMemObj = image.get();
    builtinOpParams.size = {1, 1, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::imageToHostPtr,
                                                                csr,
                                                                builtinOpParams);
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_BLOCK_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), cmdIterator);
}

HWTEST_F(BcsTests, givenHostPtrToImageWhenBlitBufferIsCalledThenBlitCmdIsCorrectlyProgrammed) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.image_width = 10;
    imgDesc.image_height = 12;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcPtr = hostPtr;
    builtinOpParams.srcMemObj = nullptr;
    builtinOpParams.dstMemObj = image.get();
    builtinOpParams.size = {6, 8, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::hostPtrToImage,
                                                                csr,
                                                                builtinOpParams);
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_BLOCK_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    auto bltCmd = genCmdCast<typename FamilyType::XY_BLOCK_COPY_BLT *>(*cmdIterator);

    auto dstPtr = builtinOpParams.dstMemObj->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    EXPECT_EQ(blitProperties.srcGpuAddress, bltCmd->getSourceBaseAddress());
    EXPECT_EQ(dstPtr, bltCmd->getDestinationBaseAddress());
}

HWTEST_F(BcsTests, givenImageToHostPtrWhenBlitBufferIsCalledThenBlitCmdIsCorrectlyProgrammed) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    constexpr size_t hostAllocationSize = MemoryConstants::pageSize;
    auto hostAllocationPtr = allocateAlignedMemory(hostAllocationSize, MemoryConstants::pageSize);
    void *hostPtr = reinterpret_cast<void *>(hostAllocationPtr.get());

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.image_width = 10u;
    imgDesc.image_height = 12u;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(context.get(), &imgDesc));
    BuiltinOpParams builtinOpParams{};
    builtinOpParams.dstPtr = hostPtr;
    builtinOpParams.srcMemObj = image.get();
    builtinOpParams.dstMemObj = nullptr;
    builtinOpParams.size = {2, 3, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::imageToHostPtr,
                                                                csr,
                                                                builtinOpParams);
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_BLOCK_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    auto bltCmd = genCmdCast<typename FamilyType::XY_BLOCK_COPY_BLT *>(*cmdIterator);

    auto srcPtr = builtinOpParams.srcMemObj->getGraphicsAllocation(csr.getRootDeviceIndex())->getGpuAddress();
    EXPECT_EQ(srcPtr, bltCmd->getSourceBaseAddress());
    EXPECT_EQ(blitProperties.dstGpuAddress, bltCmd->getDestinationBaseAddress());
}

HWTEST_F(BcsTests, givenImageToImageWhenBlitBufferIsCalledThenBlitCmdIsCorrectlyProgrammed) {
    if (!pDevice->getHardwareInfo().capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.image_width = 10u;
    imgDesc.image_height = 12u;
    std::unique_ptr<Image> srcImage(Image2dHelperUlt<>::create(context.get(), &imgDesc));
    std::unique_ptr<Image> dstImage(Image2dHelperUlt<>::create(context.get(), &imgDesc));

    BuiltinOpParams builtinOpParams{};
    builtinOpParams.srcMemObj = srcImage.get();
    builtinOpParams.dstMemObj = dstImage.get();
    builtinOpParams.size = {2, 3, 1};

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto blitProperties = ClBlitProperties::constructProperties(BlitterConstants::BlitDirection::imageToImage,
                                                                csr,
                                                                builtinOpParams);
    flushBcsTask(&csr, blitProperties, true, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr.commandStream, 0);
    auto cmdIterator = find<typename FamilyType::XY_BLOCK_COPY_BLT *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), cmdIterator);
    auto bltCmd = genCmdCast<typename FamilyType::XY_BLOCK_COPY_BLT *>(*cmdIterator);

    EXPECT_EQ(blitProperties.srcGpuAddress, bltCmd->getSourceBaseAddress());
    EXPECT_EQ(blitProperties.dstGpuAddress, bltCmd->getDestinationBaseAddress());
}

HWTEST_F(BcsTests, givenBlitBufferCalledWhenClearColorAllocationIseSetThenItIsMadeResident) {
    MockGraphicsAllocation graphicsAllocation1;
    MockGraphicsAllocation graphicsAllocation2;
    MockGraphicsAllocation clearColorAllocation;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.storeMakeResidentAllocations = true;
    Vec3<size_t> copySize = {1, 1, 1};

    auto blitProperties = BlitProperties::constructPropertiesForCopy(&graphicsAllocation1,
                                                                     &graphicsAllocation2, 0, 0, copySize, 0, 0, 0, 0, &clearColorAllocation);
    flushBcsTask(&csr, blitProperties, false, *pDevice);
    auto iter = csr.makeResidentAllocations.find(&clearColorAllocation);
    ASSERT_NE(iter, csr.makeResidentAllocations.end());
    EXPECT_EQ(&clearColorAllocation, iter->first);
    EXPECT_EQ(1u, iter->second);
}

TEST(BcsConstantsTests, givenBlitConstantsThenTheyHaveDesiredValues) {
    EXPECT_EQ(BlitterConstants::maxBlitWidth, 0x4000u);
    EXPECT_EQ(BlitterConstants::maxBlitHeight, 0x4000u);
    EXPECT_EQ(BlitterConstants::maxBlitSetWidth, 0x1FF80u);
    EXPECT_EQ(BlitterConstants::maxBlitSetHeight, 0x1FFC0u);
}
