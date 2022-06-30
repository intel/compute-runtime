/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

using namespace NEO;

typedef Test<ClDeviceFixture> AubAllocDumpTests;

struct AubFileStreamMock : public AubMemDump::AubFileStream {
    void write(const char *data, size_t size) override {
        buffer.resize(size);
        memcpy(buffer.data(), data, size);
    }
    char *getData() {
        return buffer.data();
    }
    size_t getSize() {
        return buffer.size();
    }
    std::vector<char> buffer;
};

HWTEST_F(AubAllocDumpTests, givenBufferOrImageWhenGraphicsAllocationIsKnownThenItsTypeCanBeCheckedIfItIsWritable) {
    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    gfxAllocation->setAllocationType(AllocationType::BUFFER);
    EXPECT_FALSE(gfxAllocation->isMemObjectsAllocationWithWritableFlags());
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::BUFFER);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::EXTERNAL_HOST_PTR);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::EXTERNAL_HOST_PTR);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::MAP_ALLOCATION);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::MAP_ALLOCATION);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::IMAGE);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableImage(*gfxAllocation));

    gfxAllocation->setAllocationType(AllocationType::IMAGE);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableImage(*gfxAllocation));

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenImageResourceWhenGmmResourceInfoIsAvailableThenImageSurfaceTypeCanBeDeducedFromGmmResourceType) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_1D));
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_2D));
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_3D, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_3D));
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(GMM_RESOURCE_TYPE::RESOURCE_INVALID));
}

HWTEST_F(AubAllocDumpTests, givenGraphicsAllocationWhenDumpAllocationIsCalledInDefaultModeThenGraphicsAllocationShouldNotBeDumped) {
    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenGraphicsAllocationWhenDumpAllocationIsCalledButDumpFormatIsUnspecifiedThenGraphicsAllocationShouldNotBeDumped) {
    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenNonWritableBufferWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenBufferShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, pDevice->getDeviceBitfield()});

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenNonWritableImageWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenImageShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BMP");

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = MockGmm::allocateImage2d(*memoryManager);

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenWritableBufferWhenDumpAllocationIsCalledAndAubDumpBufferFormatIsNotSetThenBufferShouldNotBeDumped) {
    MockContext context;
    size_t bufferSize = 10;
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer);

    auto gfxAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);
    EXPECT_EQ(0u, mockAubFileStream->getSize());
}

HWTEST_F(AubAllocDumpTests, givenWritableImageWhenDumpAllocationIsCalledAndAubDumpImageFormatIsNotSetThenImageShouldNotBeDumped) {
    MockContext context;
    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);
    EXPECT_EQ(0u, mockAubFileStream->getSize());
}

HWTEST_F(AubAllocDumpTests, givenWritableBufferWhenDumpAllocationIsCalledAndAubDumpBufferFormatIsSetToBinThenBufferShouldBeDumpedInBinFormat) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    MockContext context;
    size_t bufferSize = 10;
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer);

    auto gfxAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);
    ASSERT_EQ(sizeof(AubMemDump::AubCaptureBinaryDumpHD), mockAubFileStream->getSize());

    AubMemDump::AubCaptureBinaryDumpHD cmd;
    memcpy(&cmd, mockAubFileStream->getData(), mockAubFileStream->getSize());

    EXPECT_EQ(0x7u, cmd.Header.Type);
    EXPECT_EQ(0x1u, cmd.Header.Opcode);
    EXPECT_EQ(0x15u, cmd.Header.SubOp);
    EXPECT_EQ(((sizeof(cmd) - sizeof(cmd.Header)) / sizeof(uint32_t)) - 1, cmd.Header.DwordLength);

    EXPECT_EQ(gfxAllocation->getGpuAddress(), cmd.getBaseAddr());
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->getUnderlyingBufferSize()), cmd.getWidth());
    EXPECT_EQ(1u, cmd.getHeight());
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->getUnderlyingBufferSize()), cmd.getPitch());
    EXPECT_EQ(1u, cmd.GttType);
    EXPECT_EQ(handle, cmd.DirectoryHandle);
}

HWTEST_F(AubAllocDumpTests, givenWritableBufferWhenDumpAllocationIsCalledAndAubDumpBufferFormatIsSetToTreThenBufferShouldBeDumpedInTreFormat) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("TRE");

    MockContext context(pClDevice);
    size_t bufferSize = 10;
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer);

    auto gfxAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);
    ASSERT_EQ(sizeof(AubMemDump::CmdServicesMemTraceDumpCompress), mockAubFileStream->getSize());

    AubMemDump::CmdServicesMemTraceDumpCompress cmd;
    memcpy(&cmd, mockAubFileStream->getData(), mockAubFileStream->getSize());

    EXPECT_EQ((sizeof(AubMemDump::CmdServicesMemTraceDumpCompress) - 1) / 4, cmd.dwordCount);
    EXPECT_EQ(0x7u, cmd.instructionType);
    EXPECT_EQ(0x10u, cmd.instructionSubOpcode);
    EXPECT_EQ(0x2eu, cmd.instructionOpcode);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    EXPECT_EQ(gfxAllocation->getGpuAddress(), cmd.getSurfaceAddress());
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->getUnderlyingBufferSize()), cmd.surfaceWidth);
    EXPECT_EQ(1u, cmd.surfaceHeight);
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->getUnderlyingBufferSize()), cmd.surfacePitch);
    EXPECT_EQ(SURFACE_FORMAT::SURFACE_FORMAT_RAW, cmd.surfaceFormat);
    EXPECT_EQ(AubMemDump::CmdServicesMemTraceDumpCompress::DumpTypeValues::Tre, cmd.dumpType);
    EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_LINEAR, cmd.surfaceTilingType);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, cmd.surfaceType);

    EXPECT_EQ(AubMemDump::CmdServicesMemTraceDumpCompress::AlgorithmValues::Uncompressed, cmd.algorithm);

    EXPECT_EQ(1u, cmd.gttType);
    EXPECT_EQ(handle, cmd.directoryHandle);
}

HWTEST_F(AubAllocDumpTests, givenWritableImageWhenDumpAllocationIsCalledAndAubDumpImageFormatIsSetToBmpThenImageShouldBeDumpedInBmpFormat) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpImageFormat.set("BMP");

    MockContext context(pClDevice);
    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);
    ASSERT_EQ(sizeof(AubMemDump::AubCmdDumpBmpHd), mockAubFileStream->getSize());

    AubMemDump::AubCmdDumpBmpHd cmd;
    memcpy(&cmd, mockAubFileStream->getData(), mockAubFileStream->getSize());

    EXPECT_EQ(0x7u, cmd.Header.Type);
    EXPECT_EQ(0x1u, cmd.Header.Opcode);
    EXPECT_EQ(0x44u, cmd.Header.SubOp);
    EXPECT_EQ(((sizeof(cmd) - sizeof(cmd.Header)) / sizeof(uint32_t)) - 1, cmd.Header.DwordLength);

    EXPECT_EQ(0u, cmd.Xmin);
    EXPECT_EQ(0u, cmd.Ymin);
    auto gmm = gfxAllocation->getDefaultGmm();
    EXPECT_EQ((8 * gmm->gmmResourceInfo->getRenderPitch()) / gmm->gmmResourceInfo->getBitsPerPixel(), cmd.BufferPitch);
    EXPECT_EQ(gmm->gmmResourceInfo->getBitsPerPixel(), cmd.BitsPerPixel);
    EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getResourceFormatSurfaceState()), cmd.Format);
    EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth()), cmd.Xsize);
    EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight()), cmd.Ysize);
    EXPECT_EQ(gfxAllocation->getGpuAddress(), cmd.getBaseAddr());
    EXPECT_EQ(0u, cmd.Secure);
    EXPECT_EQ(0u, cmd.UseFence);
    auto flagInfo = gmm->gmmResourceInfo->getResourceFlags()->Info;
    EXPECT_EQ(static_cast<uint32_t>(flagInfo.TiledW || flagInfo.TiledX || flagInfo.TiledY || flagInfo.TiledYf || flagInfo.TiledYs), cmd.TileOn);
    EXPECT_EQ(flagInfo.TiledY, cmd.WalkY);
    EXPECT_EQ(1u, cmd.UsePPGTT);
    EXPECT_EQ(1u, cmd.Use32BitDump);
    EXPECT_EQ(1u, cmd.UseFullFormat);
    EXPECT_EQ(handle, cmd.DirectoryHandle);
}

HWTEST_F(AubAllocDumpTests, givenWritableImageWhenDumpAllocationIsCalledAndAubDumpImageFormatIsSetToTreThenImageShouldBeDumpedInTreFormat) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpImageFormat.set("TRE");

    MockContext context(pClDevice);
    std::unique_ptr<Image> image(ImageHelper<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);
    ASSERT_EQ(sizeof(AubMemDump::CmdServicesMemTraceDumpCompress), mockAubFileStream->getSize());

    AubMemDump::CmdServicesMemTraceDumpCompress cmd;
    memcpy(&cmd, mockAubFileStream->getData(), mockAubFileStream->getSize());

    EXPECT_EQ((sizeof(AubMemDump::CmdServicesMemTraceDumpCompress) - 1) / 4, cmd.dwordCount);
    EXPECT_EQ(0x7u, cmd.instructionType);
    EXPECT_EQ(0x10u, cmd.instructionSubOpcode);
    EXPECT_EQ(0x2eu, cmd.instructionOpcode);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    EXPECT_EQ(gfxAllocation->getGpuAddress(), cmd.getSurfaceAddress());
    auto gmm = gfxAllocation->getDefaultGmm();
    EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth()), cmd.surfaceWidth);
    EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight()), cmd.surfaceHeight);
    EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getRenderPitch()), cmd.surfacePitch);
    EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getResourceFormatSurfaceState()), cmd.surfaceFormat);
    EXPECT_EQ(AubMemDump::CmdServicesMemTraceDumpCompress::DumpTypeValues::Tre, cmd.dumpType);
    EXPECT_EQ(gmm->gmmResourceInfo->getTileModeSurfaceState(), cmd.surfaceTilingType);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, cmd.surfaceType);

    EXPECT_EQ(AubMemDump::CmdServicesMemTraceDumpCompress::AlgorithmValues::Uncompressed, cmd.algorithm);

    EXPECT_EQ(1u, cmd.gttType);
    EXPECT_EQ(handle, cmd.directoryHandle);
}

HWTEST_F(AubAllocDumpTests, givenCompressedImageWritableWhenDumpAllocationIsCalledAndAubDumpImageFormatIsSetToTreThenImageShouldBeDumpedInTreFormat) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpImageFormat.set("TRE");

    MockContext context(pClDevice);
    std::unique_ptr<Image> image(ImageHelper<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    gfxAllocation->getDefaultGmm()->isCompressionEnabled = true;

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);

    EXPECT_EQ(0u, mockAubFileStream->getSize());
}

HWTEST_F(AubAllocDumpTests, givenMultisampleImageWritableWhenDumpAllocationIsCalledAndAubDumpImageFormatIsSetToTreThenImageDumpIsNotSupported) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpImageFormat.set("TRE");

    MockContext context(pClDevice);
    std::unique_ptr<Image> image(ImageHelper<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto mockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(gfxAllocation->getDefaultGmm()->gmmResourceInfo.get());
    mockGmmResourceInfo->mockResourceCreateParams.MSAA.NumSamples = 2;

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    auto format = AubAllocDump::getDumpFormat(*gfxAllocation);
    AubAllocDump::dumpAllocation<FamilyType>(format, *gfxAllocation, mockAubFileStream.get(), handle);

    EXPECT_EQ(0u, mockAubFileStream->getSize());
}

HWTEST_F(AubAllocDumpTests, givenMultisampleImageWritableWheGetDumpSurfaceIsCalledAndDumpFormatIsSpecifiedThenNullSurfaceInfoIsReturned) {
    MockContext context(pClDevice);
    std::unique_ptr<Image> image(ImageHelper<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto mockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(gfxAllocation->getDefaultGmm()->gmmResourceInfo.get());
    mockGmmResourceInfo->mockResourceCreateParams.MSAA.NumSamples = 2;

    EXPECT_EQ(nullptr, AubAllocDump::getDumpSurfaceInfo<FamilyType>(*gfxAllocation, AubAllocDump::DumpFormat::IMAGE_BMP));
    EXPECT_EQ(nullptr, AubAllocDump::getDumpSurfaceInfo<FamilyType>(*gfxAllocation, AubAllocDump::DumpFormat::IMAGE_TRE));
}

struct AubSurfaceDumpTests : public AubAllocDumpTests,
                             public ::testing::WithParamInterface<std::tuple<bool /*isCompressed*/, AubAllocDump::DumpFormat>> {
    void SetUp() override {
        AubAllocDumpTests::SetUp();
        isCompressed = std::get<0>(GetParam());
        dumpFormat = std::get<1>(GetParam());
    }
    void TearDown() override {
        AubAllocDumpTests::TearDown();
    }

    bool isCompressed = true;
    AubAllocDump::DumpFormat dumpFormat = AubAllocDump::DumpFormat::NONE;
};

HWTEST_P(AubSurfaceDumpTests, givenGraphicsAllocationWhenGetDumpSurfaceIsCalledAndDumpFormatIsSpecifiedThenSurfaceInfoIsReturned) {
    ExecutionEnvironment *executionEnvironment = pDevice->executionEnvironment;
    MockMemoryManager memoryManager(*executionEnvironment);
    auto gmmHelper = pDevice->getRootDeviceEnvironment().getGmmHelper();

    if (AubAllocDump::isBufferDumpFormat(dumpFormat)) {
        auto bufferAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
        bufferAllocation->setDefaultGmm(new Gmm(pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

        ASSERT_NE(nullptr, bufferAllocation);

        MockBuffer::setAllocationType(bufferAllocation, gmmHelper, isCompressed);

        std::unique_ptr<aub_stream::SurfaceInfo> surfaceInfo(AubAllocDump::getDumpSurfaceInfo<FamilyType>(*bufferAllocation, dumpFormat));
        if (nullptr != surfaceInfo) {
            using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
            using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
            EXPECT_EQ(gmmHelper->decanonize(bufferAllocation->getGpuAddress()), surfaceInfo->address);
            EXPECT_EQ(static_cast<uint32_t>(bufferAllocation->getUnderlyingBufferSize()), surfaceInfo->width);
            EXPECT_EQ(1u, surfaceInfo->height);
            EXPECT_EQ(static_cast<uint32_t>(bufferAllocation->getUnderlyingBufferSize()), surfaceInfo->pitch);
            EXPECT_EQ(SURFACE_FORMAT::SURFACE_FORMAT_RAW, surfaceInfo->format);
            EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfaceInfo->surftype);
            EXPECT_EQ(RENDER_SURFACE_STATE::TILE_MODE_LINEAR, surfaceInfo->tilingType);
            EXPECT_EQ(bufferAllocation->isCompressionEnabled(), surfaceInfo->compressed);
            EXPECT_EQ((AubAllocDump::DumpFormat::BUFFER_TRE == dumpFormat) ? aub_stream::dumpType::tre : aub_stream::dumpType::bin, surfaceInfo->dumpType);
        }
        memoryManager.freeGraphicsMemory(bufferAllocation);
    }

    if (AubAllocDump::isImageDumpFormat(dumpFormat)) {
        ImageDescriptor imgDesc = {};
        imgDesc.imageWidth = 512;
        imgDesc.imageHeight = 1;
        imgDesc.imageType = ImageType::Image2D;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        MockGmm::queryImgParams(pDevice->getGmmHelper(), imgInfo, false);
        AllocationData allocationData;
        allocationData.imgInfo = &imgInfo;
        auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
        ASSERT_NE(nullptr, imageAllocation);

        auto gmm = imageAllocation->getDefaultGmm();
        gmm->isCompressionEnabled = isCompressed;

        std::unique_ptr<aub_stream::SurfaceInfo> surfaceInfo(AubAllocDump::getDumpSurfaceInfo<FamilyType>(*imageAllocation, dumpFormat));
        if (nullptr != surfaceInfo) {
            EXPECT_EQ(gmmHelper->decanonize(imageAllocation->getGpuAddress()), surfaceInfo->address);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth()), surfaceInfo->width);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight()), surfaceInfo->height);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getRenderPitch()), surfaceInfo->pitch);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getResourceFormatSurfaceState()), surfaceInfo->format);
            EXPECT_EQ(AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(gmm->gmmResourceInfo->getResourceType()), surfaceInfo->surftype);
            EXPECT_EQ(gmm->gmmResourceInfo->getTileModeSurfaceState(), surfaceInfo->tilingType);
            EXPECT_EQ(gmm->isCompressionEnabled, surfaceInfo->compressed);
            EXPECT_EQ((AubAllocDump::DumpFormat::IMAGE_TRE == dumpFormat) ? aub_stream::dumpType::tre : aub_stream::dumpType::bmp, surfaceInfo->dumpType);
        }
        memoryManager.freeGraphicsMemory(imageAllocation);
    }
}

INSTANTIATE_TEST_CASE_P(GetDumpSurfaceTest,
                        AubSurfaceDumpTests,
                        ::testing::Combine(
                            ::testing::Bool(), // isCompressed
                            ::testing::Values( // dumpFormat
                                AubAllocDump::DumpFormat::NONE,
                                AubAllocDump::DumpFormat::BUFFER_BIN,
                                AubAllocDump::DumpFormat::BUFFER_TRE,
                                AubAllocDump::DumpFormat::IMAGE_BMP,
                                AubAllocDump::DumpFormat::IMAGE_TRE)));
