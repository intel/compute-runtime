/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_alloc_dump.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/mem_obj/buffer.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"

using namespace OCLRT;

typedef Test<DeviceFixture> AubAllocDumpTests;

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
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    gfxAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_FALSE(gfxAllocation->isMemObjectsAllocationWithWritableFlags());
    EXPECT_FALSE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(true);
    EXPECT_TRUE(AubAllocDump::isWritableBuffer(*gfxAllocation));

    gfxAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
    gfxAllocation->setMemObjectsAllocationWithWritableFlags(false);
    EXPECT_FALSE(AubAllocDump::isWritableImage(*gfxAllocation));

    gfxAllocation->setAllocationType(GraphicsAllocation::AllocationType::IMAGE);
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
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenGraphicsAllocationWhenDumpAllocationIsCalledButDumpFormatIsUnspecifiedThenGraphicsAllocationShouldNotBeDumped) {
    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenNonWritableBufferWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenBufferShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BIN");

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenNonWritableImageWhenDumpAllocationIsCalledAndDumpFormatIsSpecifiedThenImageShouldNotBeDumped) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpBufferFormat.set("BMP");

    auto memoryManager = pDevice->getMemoryManager();
    auto gfxAllocation = MockGmm::allocateImage2d(*memoryManager);

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), 0);

    EXPECT_EQ(0u, mockAubFileStream->getSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

HWTEST_F(AubAllocDumpTests, givenWritableBufferWhenDumpAllocationIsCalledAndAubDumpBufferFormatIsNotSetThenBufferShouldNotBeDumped) {
    MockContext context;
    size_t bufferSize = 10;
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer);

    auto gfxAllocation = buffer->getGraphicsAllocation();
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);
    EXPECT_EQ(0u, mockAubFileStream->getSize());
}

HWTEST_F(AubAllocDumpTests, givenWritableImageWhenDumpAllocationIsCalledAndAubDumpImageFormatIsNotSetThenImageShouldNotBeDumped) {
    MockContext context;
    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation();
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);
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

    auto gfxAllocation = buffer->getGraphicsAllocation();
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);
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

    MockContext context;
    size_t bufferSize = 10;
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    ASSERT_NE(nullptr, buffer);

    auto gfxAllocation = buffer->getGraphicsAllocation();
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);
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

    MockContext context;
    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation();
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);
    ASSERT_EQ(sizeof(AubMemDump::AubCmdDumpBmpHd), mockAubFileStream->getSize());

    AubMemDump::AubCmdDumpBmpHd cmd;
    memcpy(&cmd, mockAubFileStream->getData(), mockAubFileStream->getSize());

    EXPECT_EQ(0x7u, cmd.Header.Type);
    EXPECT_EQ(0x1u, cmd.Header.Opcode);
    EXPECT_EQ(0x44u, cmd.Header.SubOp);
    EXPECT_EQ(((sizeof(cmd) - sizeof(cmd.Header)) / sizeof(uint32_t)) - 1, cmd.Header.DwordLength);

    EXPECT_EQ(0u, cmd.Xmin);
    EXPECT_EQ(0u, cmd.Ymin);
    auto gmm = gfxAllocation->gmm;
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

    MockContext context;
    std::unique_ptr<Image> image(ImageHelper<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation();
    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);
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
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->gmm->gmmResourceInfo->getBaseWidth()), cmd.surfaceWidth);
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->gmm->gmmResourceInfo->getBaseHeight()), cmd.surfaceHeight);
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->gmm->gmmResourceInfo->getRenderPitch()), cmd.surfacePitch);
    EXPECT_EQ(static_cast<uint32_t>(gfxAllocation->gmm->gmmResourceInfo->getResourceFormatSurfaceState()), cmd.surfaceFormat);
    EXPECT_EQ(AubMemDump::CmdServicesMemTraceDumpCompress::DumpTypeValues::Tre, cmd.dumpType);
    EXPECT_EQ(gfxAllocation->gmm->gmmResourceInfo->getTileModeSurfaceState(), cmd.surfaceTilingType);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, cmd.surfaceType);

    EXPECT_EQ(AubMemDump::CmdServicesMemTraceDumpCompress::AlgorithmValues::Uncompressed, cmd.algorithm);

    EXPECT_EQ(1u, cmd.gttType);
    EXPECT_EQ(handle, cmd.directoryHandle);
}

HWTEST_F(AubAllocDumpTests, givenCompressedImageWritableWhenDumpAllocationIsCalledAndAubDumpImageFormatIsSetToTreThenImageShouldBeDumpedInTreFormat) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpImageFormat.set("TRE");

    MockContext context;
    std::unique_ptr<Image> image(ImageHelper<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation();
    gfxAllocation->gmm->isRenderCompressed = true;

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);

    EXPECT_EQ(0u, mockAubFileStream->getSize());
}

HWTEST_F(AubAllocDumpTests, givenMultisampleImageWritableWhenDumpAllocationIsCalledAndAubDumpImageFormatIsSetToTreThenImageDumpIsNotSupported) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AUBDumpImageFormat.set("TRE");

    MockContext context;
    std::unique_ptr<Image> image(ImageHelper<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation();
    auto mockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(gfxAllocation->gmm->gmmResourceInfo.get());
    mockGmmResourceInfo->mockResourceCreateParams.MSAA.NumSamples = 2;

    std::unique_ptr<AubFileStreamMock> mockAubFileStream(new AubFileStreamMock());
    auto handle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    AubAllocDump::dumpAllocation<FamilyType>(*gfxAllocation, mockAubFileStream.get(), handle);

    EXPECT_EQ(0u, mockAubFileStream->getSize());
}
