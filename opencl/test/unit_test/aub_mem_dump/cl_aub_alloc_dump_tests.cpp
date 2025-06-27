/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "aubstream/aubstream.h"

namespace NEO {
class ExecutionEnvironment;
} // namespace NEO

using namespace NEO;

typedef Test<ClDeviceFixture> AubAllocDumpTests;

HWTEST_F(AubAllocDumpTests, givenMultisampleImageWritableWheGetDumpSurfaceIsCalledAndDumpFormatIsSpecifiedThenNullSurfaceInfoIsReturned) {
    MockContext context(pClDevice);
    std::unique_ptr<Image> image(ImageHelperUlt<Image2dDefaults>::create(&context));
    ASSERT_NE(nullptr, image);

    auto gfxAllocation = image->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto mockGmmResourceInfo = reinterpret_cast<MockGmmResourceInfo *>(gfxAllocation->getDefaultGmm()->gmmResourceInfo.get());
    mockGmmResourceInfo->mockResourceCreateParams.MSAA.NumSamples = 2;

    EXPECT_EQ(nullptr, AubAllocDump::getDumpSurfaceInfo<FamilyType>(*gfxAllocation, *pDevice->getGmmHelper(), AubAllocDump::DumpFormat::IMAGE_BMP));
    EXPECT_EQ(nullptr, AubAllocDump::getDumpSurfaceInfo<FamilyType>(*gfxAllocation, *pDevice->getGmmHelper(), AubAllocDump::DumpFormat::IMAGE_TRE));
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

        ASSERT_NE(nullptr, bufferAllocation);

        MockBuffer::setAllocationType(bufferAllocation, gmmHelper, isCompressed);

        std::unique_ptr<aub_stream::SurfaceInfo> surfaceInfo(AubAllocDump::getDumpSurfaceInfo<FamilyType>(*bufferAllocation, *pDevice->getGmmHelper(), dumpFormat));
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
        imgDesc.imageType = ImageType::image2D;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        MockGmm::queryImgParams(pDevice->getGmmHelper(), imgInfo, false);
        AllocationData allocationData;
        allocationData.imgInfo = &imgInfo;
        auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
        ASSERT_NE(nullptr, imageAllocation);

        auto gmm = imageAllocation->getDefaultGmm();
        gmm->setCompressionEnabled(isCompressed);

        std::unique_ptr<aub_stream::SurfaceInfo> surfaceInfo(AubAllocDump::getDumpSurfaceInfo<FamilyType>(*imageAllocation, *pDevice->getGmmHelper(), dumpFormat));
        if (nullptr != surfaceInfo) {
            EXPECT_EQ(gmmHelper->decanonize(imageAllocation->getGpuAddress()), surfaceInfo->address);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseWidth()), surfaceInfo->width);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getBaseHeight()), surfaceInfo->height);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getRenderPitch()), surfaceInfo->pitch);
            EXPECT_EQ(static_cast<uint32_t>(gmm->gmmResourceInfo->getResourceFormatSurfaceState()), surfaceInfo->format);
            EXPECT_EQ(AubAllocDump::getImageSurfaceTypeFromGmmResourceType<FamilyType>(gmm->gmmResourceInfo->getResourceType()), surfaceInfo->surftype);
            EXPECT_EQ(gmm->gmmResourceInfo->getTileModeSurfaceState(), surfaceInfo->tilingType);
            EXPECT_EQ(gmm->isCompressionEnabled(), surfaceInfo->compressed);
            EXPECT_EQ((AubAllocDump::DumpFormat::IMAGE_TRE == dumpFormat) ? aub_stream::dumpType::tre : aub_stream::dumpType::bmp, surfaceInfo->dumpType);
        }
        memoryManager.freeGraphicsMemory(imageAllocation);
    }
}

INSTANTIATE_TEST_SUITE_P(GetDumpSurfaceTest,
                         AubSurfaceDumpTests,
                         ::testing::Combine(
                             ::testing::Bool(), // isCompressed
                             ::testing::Values( // dumpFormat
                                 AubAllocDump::DumpFormat::NONE,
                                 AubAllocDump::DumpFormat::BUFFER_BIN,
                                 AubAllocDump::DumpFormat::BUFFER_TRE,
                                 AubAllocDump::DumpFormat::IMAGE_BMP,
                                 AubAllocDump::DumpFormat::IMAGE_TRE)));
