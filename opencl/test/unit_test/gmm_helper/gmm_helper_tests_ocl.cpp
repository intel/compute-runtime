/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/mem_obj/image.h"

#include "GL/gl.h"
#include "GL/glext.h"

using MockExecutionEnvironmentGmmFixtureTest = Test<NEO::MockExecutionEnvironmentGmmFixture>;

namespace NEO {

extern GMM_INIT_IN_ARGS passedInputArgs;
extern GT_SYSTEM_INFO passedGtSystemInfo;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;

struct GmmTests : public MockExecutionEnvironmentGmmFixtureTest {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        localPlatformDevice = rootDeviceEnvironment->getMutableHardwareInfo();
        gmmHelper = rootDeviceEnvironment->getGmmHelper();
    }
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    HardwareInfo *localPlatformDevice = nullptr;
    GmmHelper *gmmHelper = nullptr;
};

TEST(GmmGlTests, givenGmmWhenAskedforCubeFaceIndexThenProperValueIsReturned) {
    std::vector<std::pair<GMM_CUBE_FACE_ENUM, uint32_t>> v = {{__GMM_CUBE_FACE_NEG_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X},
                                                              {__GMM_CUBE_FACE_POS_X, GL_TEXTURE_CUBE_MAP_POSITIVE_X},
                                                              {__GMM_CUBE_FACE_NEG_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y},
                                                              {__GMM_CUBE_FACE_POS_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Y},
                                                              {__GMM_CUBE_FACE_NEG_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z},
                                                              {__GMM_CUBE_FACE_POS_Z, GL_TEXTURE_CUBE_MAP_POSITIVE_Z}};

    uint32_t maxVal = 0;
    for (auto p : v) {
        EXPECT_TRUE(p.first == GmmTypesConverter::getCubeFaceIndex(p.second));
        maxVal = std::max(maxVal, p.second);
    }
    maxVal++;
    EXPECT_TRUE(__GMM_NO_CUBE_MAP == GmmTypesConverter::getCubeFaceIndex(maxVal));
}

TEST_F(GmmTests, givenZeroRowPitchWhenQueryImgFromBufferParamsThenCalculate) {
    MockGraphicsAllocation bufferAllocation(nullptr, 4096);

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 5;
    imgDesc.imageHeight = 5;
    imgDesc.imageRowPitch = 0;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    size_t expectedRowPitch = imgDesc.imageWidth * imgInfo.surfaceFormat->imageElementSizeInBytes;
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &bufferAllocation);

    EXPECT_EQ(imgInfo.rowPitch, expectedRowPitch);
}

TEST_F(GmmTests, givenNonZeroRowPitchWhenQueryImgFromBufferParamsThenUseUserValue) {
    MockGraphicsAllocation bufferAllocation(nullptr, 4096);

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 5;
    imgDesc.imageHeight = 5;
    imgDesc.imageRowPitch = 123;
    size_t expectedRowPitch = imgDesc.imageRowPitch;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &bufferAllocation);

    EXPECT_EQ(imgInfo.rowPitch, expectedRowPitch);
}

TEST_F(GmmTests, givenNumSamplesWhenAskedForMultisamplesCountThenReturnValue) {
    uint32_t numSamples[5][2] = {{0, 0}, {2, 1}, {4, 2}, {8, 3}, {16, 4}}; //{given, expected}
    for (int i = 0; i < 5; i++) {
        auto result = GmmTypesConverter::getRenderMultisamplesCount(numSamples[i][0]);
        EXPECT_EQ(numSamples[i][1], result);
    }
}

TEST_F(GmmTests, given2DimageFromBufferParametersWhenGmmResourceIsCreatedThenItHasDesiredPitchAndSize) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 329;
    imgDesc.imageHeight = 349;
    imgDesc.imageDepth = 1;
    imgDesc.imageRowPitch = 5312;
    imgDesc.fromParent = true;

    SurfaceFormatInfo surfaceFormat = {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, (SurfaceFormat)0, 0, 4, 4, 16};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);
    auto renderSize = queryGmm->gmmResourceInfo->getSizeAllocation();

    size_t expectedSize = imgDesc.imageRowPitch * imgDesc.imageHeight;
    EXPECT_GE(renderSize, expectedSize);
    EXPECT_EQ(imgDesc.imageRowPitch, queryGmm->gmmResourceInfo->getRenderPitch());
}

namespace GmmTestConst {
static const cl_mem_object_type imgTypes[6] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D};
} // namespace GmmTestConst

TEST_F(GmmTests, WhenConvertingPlanesThenCorrectPlaneIsReturned) {
    std::vector<std::pair<ImagePlane, GMM_YUV_PLANE>> v = {{ImagePlane::noPlane, GMM_YUV_PLANE::GMM_NO_PLANE},
                                                           {ImagePlane::planeY, GMM_YUV_PLANE::GMM_PLANE_Y},
                                                           {ImagePlane::planeU, GMM_YUV_PLANE::GMM_PLANE_U},
                                                           {ImagePlane::planeUV, GMM_YUV_PLANE::GMM_PLANE_U},
                                                           {ImagePlane::planeV, GMM_YUV_PLANE::GMM_PLANE_V}};

    for (auto p : v) {
        EXPECT_TRUE(p.second == GmmTypesConverter::convertPlane(p.first));
    }
}

class GmmImgTest : public GmmTests,
                   public ::testing::WithParamInterface<uint32_t /*cl_mem_object_type*/> {};

INSTANTIATE_TEST_SUITE_P(
    GmmImgTests,
    GmmImgTest,
    testing::ValuesIn(GmmTestConst::imgTypes));

TEST_P(GmmImgTest, WhenUpdatingImgInfoAndDescThenInformationIsCorrect) {
    struct MyMockGmmResourceInfo : MockGmmResourceInfo {
        MyMockGmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams) : MockGmmResourceInfo(resourceCreateParams) {}
        GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) override {
            givenReqInfo[getOffsetCalled] = reqOffsetInfo;
            getOffsetCalled++;
            return MockGmmResourceInfo::getOffset(reqOffsetInfo);
        }

        uint32_t getOffsetCalled = 0u;
        GMM_REQ_OFFSET_INFO givenReqInfo[2] = {};
    };

    ImageInfo updateImgInfo = {};
    NEO::ImagePlane yuvPlane = NEO::ImagePlane::noPlane;
    if (Image::convertType(GetParam()) == ImageType::image2D) {
        updateImgInfo.plane = GMM_YUV_PLANE::GMM_PLANE_U;
        yuvPlane = NEO::ImagePlane::planeUV;
    }

    uint32_t expectCalls = 1u;
    GMM_REQ_OFFSET_INFO expectedReqInfo[2] = {};
    expectedReqInfo[0].ReqLock = 1;
    expectedReqInfo[1].ReqRender = 1;
    expectedReqInfo[1].Plane = updateImgInfo.plane;

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = Image::convertType(GetParam());
    imgDesc.imageWidth = 60;
    imgDesc.imageHeight = 1;
    imgDesc.imageDepth = 1;
    imgDesc.imageArraySize = 1;
    cl_uint arrayIndex = 0;

    if (imgDesc.imageType == ImageType::image2D || imgDesc.imageType == ImageType::image2DArray || imgDesc.imageType == ImageType::image3D) {
        imgDesc.imageHeight = 40;
    }
    if (imgDesc.imageType == ImageType::image3D) {
        imgDesc.imageDepth = 5;
        expectCalls = 2u;
        expectedReqInfo[0].Slice = 1;
    }

    if (imgDesc.imageType == ImageType::image2DArray ||
        imgDesc.imageType == ImageType::image1DArray) {
        imgDesc.imageArraySize = 5;
        expectCalls = 2u;
        arrayIndex = 2;
        expectedReqInfo[0].ArrayIndex = 1;
        expectedReqInfo[1].ArrayIndex = arrayIndex;
    }

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

    auto mockResInfo = new MyMockGmmResourceInfo(&queryGmm->resourceParams);
    queryGmm->gmmResourceInfo.reset(mockResInfo);

    queryGmm->updateImgInfoAndDesc(updateImgInfo, arrayIndex, yuvPlane);
    EXPECT_EQ(expectCalls, mockResInfo->getOffsetCalled);

    if (yuvPlane == NEO::ImagePlane::planeUV) {
        EXPECT_EQ(imgDesc.imageWidth / 2, updateImgInfo.imgDesc.imageWidth);
        EXPECT_EQ(imgDesc.imageHeight / 2, updateImgInfo.imgDesc.imageHeight);
    } else {
        EXPECT_EQ(imgDesc.imageWidth, updateImgInfo.imgDesc.imageWidth);
        EXPECT_EQ(imgDesc.imageHeight, updateImgInfo.imgDesc.imageHeight);
    }
    EXPECT_EQ(imgDesc.imageDepth, updateImgInfo.imgDesc.imageDepth);
    EXPECT_EQ(imgDesc.imageArraySize, updateImgInfo.imgDesc.imageArraySize);
    EXPECT_GT(updateImgInfo.imgDesc.imageRowPitch, 0u);
    EXPECT_GT(updateImgInfo.imgDesc.imageSlicePitch, 0u);

    if (expectCalls == 1) {
        EXPECT_TRUE(memcmp(&expectedReqInfo[1], &mockResInfo->givenReqInfo[0], sizeof(GMM_REQ_OFFSET_INFO)) == 0);
    } else if (expectCalls == 2u) {
        EXPECT_TRUE(memcmp(&expectedReqInfo[0], &mockResInfo->givenReqInfo[0], sizeof(GMM_REQ_OFFSET_INFO)) == 0);
        EXPECT_TRUE(memcmp(&expectedReqInfo[1], &mockResInfo->givenReqInfo[1], sizeof(GMM_REQ_OFFSET_INFO)) == 0);
    } else {
        EXPECT_TRUE(false);
    }

    if (yuvPlane == NEO::ImagePlane::planeUV) {
        yuvPlane = NEO::ImagePlane::planeV;
        auto uvRowPitch = updateImgInfo.imgDesc.imageRowPitch;
        queryGmm->updateImgInfoAndDesc(updateImgInfo, arrayIndex, yuvPlane);
        EXPECT_EQ(imgDesc.imageWidth / 2, updateImgInfo.imgDesc.imageWidth);
        EXPECT_EQ(imgDesc.imageHeight / 2, updateImgInfo.imgDesc.imageHeight);
        EXPECT_EQ(uvRowPitch / 2, updateImgInfo.imgDesc.imageRowPitch);
    }
}

TEST_F(GmmImgTest, givenImgInfoWhenUpdatingOffsetsThenGmmIsCalledToGetOffsets) {
    struct GmmGetOffsetOutput {
        uint32_t offset;
        uint32_t xOffset;
        uint32_t yOffset;
    };

    struct MyMockGmmResourceInfo : MockGmmResourceInfo {
        MyMockGmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams) : MockGmmResourceInfo(resourceCreateParams) {}
        GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) override {
            EXPECT_EQ(1u, reqOffsetInfo.ReqRender);
            EXPECT_EQ(0u, reqOffsetInfo.Slice);
            EXPECT_EQ(expectedArrayIndex, reqOffsetInfo.ArrayIndex);
            EXPECT_EQ(expectedGmmPlane, reqOffsetInfo.Plane);

            reqOffsetInfo.Render.Offset = gmmGetOffsetOutput.offset;
            reqOffsetInfo.Render.XOffset = gmmGetOffsetOutput.xOffset;
            reqOffsetInfo.Render.YOffset = gmmGetOffsetOutput.yOffset;
            return GMM_SUCCESS;
        }

        uint32_t getBitsPerPixel() override {
            return gmmGetBitsPerPixelOutput;
        }

        cl_uint expectedArrayIndex;
        GMM_YUV_PLANE_ENUM expectedGmmPlane;
        GmmGetOffsetOutput gmmGetOffsetOutput;
        uint32_t gmmGetBitsPerPixelOutput;
    };

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2DArray;
    imgDesc.imageWidth = 60;
    imgDesc.imageHeight = 1;
    imgDesc.imageDepth = 1;
    imgDesc.imageArraySize = 10;

    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    std::unique_ptr<Gmm> gmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);
    MyMockGmmResourceInfo *mockGmmResourceInfo = new MyMockGmmResourceInfo(&gmm->resourceParams);
    gmm->gmmResourceInfo.reset(mockGmmResourceInfo);

    mockGmmResourceInfo->expectedArrayIndex = 7;
    mockGmmResourceInfo->expectedGmmPlane = imgInfo.plane;
    mockGmmResourceInfo->gmmGetOffsetOutput = {10, 111, 120};
    mockGmmResourceInfo->gmmGetBitsPerPixelOutput = 24;
    gmm->updateOffsetsInImgInfo(imgInfo, mockGmmResourceInfo->expectedArrayIndex);
    EXPECT_EQ(mockGmmResourceInfo->gmmGetOffsetOutput.offset, imgInfo.offset);
    const auto expectedXOffset = mockGmmResourceInfo->gmmGetOffsetOutput.xOffset / (mockGmmResourceInfo->gmmGetBitsPerPixelOutput / 8);
    EXPECT_EQ(expectedXOffset, imgInfo.xOffset);
    EXPECT_EQ(mockGmmResourceInfo->gmmGetOffsetOutput.yOffset, imgInfo.yOffset);
}

struct GmmCompressionTests : public MockExecutionEnvironmentGmmFixtureTest {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixtureTest::SetUp();
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        localPlatformDevice = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

        localPlatformDevice->capabilityTable.ftrRenderCompressedImages = true;
        localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;
        localPlatformDevice->featureTable.flags.ftrLocalMemory = true;

        setupImgInfo();
    }

    void setupImgInfo() {
        imgDesc.imageType = ImageType::image2D;
        imgDesc.imageWidth = 2;
        imgDesc.imageHeight = 2;
        imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        imgInfo.useLocalMemory = true;

        // allowed for render compression:
        imgInfo.plane = GMM_YUV_PLANE::GMM_NO_PLANE;
    }

    HardwareInfo *localPlatformDevice = nullptr;
    ImageDescriptor imgDesc = {};
    ImageInfo imgInfo = {};
};

TEST_F(GmmCompressionTests, givenNV12FormatWhenQueryingThenDisallow) {
    imgInfo.surfaceFormat = &SurfaceFormats::planarYuv()[0].surfaceFormat;
    EXPECT_TRUE(imgInfo.surfaceFormat->gmmSurfaceFormat == GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12);
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    EXPECT_GT(compressionFormat, 0u);

    EXPECT_FALSE(queryGmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenInvalidCompressionFormatAndFlatCcsFtrSetWhenQueryingThenDisallowOnGmmFlatCcsFormat) {
    auto mockGmmClient = static_cast<MockGmmClientContext *>(getGmmClientContext());
    imgInfo.surfaceFormat = &SurfaceFormats::readOnlyDepth()[2].surfaceFormat;

    localPlatformDevice->featureTable.flags.ftrFlatPhysCCS = true;
    uint8_t validFormat = static_cast<uint8_t>(GMM_E2ECOMP_FORMAT::GMM_E2ECOMP_FORMAT_INVALID);
    uint8_t invalidFormat = static_cast<uint8_t>(GMM_FLATCCS_FORMAT::GMM_FLATCCS_FORMAT_INVALID);

    mockGmmClient->compressionFormatToReturn = invalidFormat;
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    EXPECT_EQ(compressionFormat, invalidFormat);
    EXPECT_FALSE(queryGmm->isCompressionEnabled());

    mockGmmClient->compressionFormatToReturn = validFormat;
    queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    EXPECT_TRUE(queryGmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenInvalidCompressionFormatAndFlatCcsFtrNotSetWhenQueryingThenDisallowOnGmmE2CCompFormat) {
    auto mockGmmClient = static_cast<MockGmmClientContext *>(getGmmClientContext());
    imgInfo.surfaceFormat = &SurfaceFormats::readOnlyDepth()[2].surfaceFormat;

    localPlatformDevice->featureTable.flags.ftrFlatPhysCCS = false;
    uint8_t invalidFormat = static_cast<uint8_t>(GMM_E2ECOMP_FORMAT::GMM_E2ECOMP_FORMAT_INVALID);
    uint8_t validFormat = static_cast<uint8_t>(GMM_FLATCCS_FORMAT::GMM_FLATCCS_FORMAT_INVALID);
    mockGmmClient->compressionFormatToReturn = invalidFormat;

    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    EXPECT_EQ(compressionFormat, invalidFormat);
    EXPECT_FALSE(queryGmm->isCompressionEnabled());

    mockGmmClient->compressionFormatToReturn = validFormat;
    queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    EXPECT_TRUE(queryGmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenPackedYuvFormatWhenQueryingThenDisallow) {
    for (auto &surfaceFormat : SurfaceFormats::packedYuv()) {
        imgInfo.surfaceFormat = &surfaceFormat.surfaceFormat;
        auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

        EXPECT_FALSE(queryGmm->isCompressionEnabled());
    }
}

struct GmmLocalMemoryTests : public ::testing::Test, MockExecutionEnvironmentGmmFixture {
    GmmLocalMemoryTests() {
        localPlatformDevice = *defaultHwInfo;
        localPlatformDevice.featureTable.flags.ftrLocalMemory = true;
    }
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&localPlatformDevice);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    }

    HardwareInfo localPlatformDevice{};
};

struct MultiTileGmmTests : GmmLocalMemoryTests {
    MultiTileGmmTests() {
        localPlatformDevice.featureTable.flags.ftrMultiTileArch = true;
        localPlatformDevice.gtSystemInfo.MultiTileArchInfo.TileMask = customTileMask;
    }
    uint8_t customTileMask = 0xD;
};

TEST_F(GmmLocalMemoryTests, givenUseLocalMemoryInImageInfoTrueWhenGmmIsCreatedThenLocalAndNonLocalOnlyFlagIsNotSetAndNotLockableIsSet) {
    ImageInfo imgInfo = {};
    cl_image_desc desc = {0};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D;
    desc.image_width = 1;

    cl_image_format imageFormat = {CL_R, CL_UNSIGNED_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(desc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;

    imgInfo.useLocalMemory = true;
    StorageInfo storageInfo = {};
    storageInfo.systemMemoryPlacement = false;
    storageInfo.memoryBanks.set(1);

    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, storageInfo, false);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenUseCompressionAndLocalMemoryInImageInfoTrueWhenGmmIsCreatedThenNonLocalOnlyFlagIsNotSetAndNotLockableAndLocalOnlyIsSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RenderCompressedImagesEnabled.set(1);
    ImageInfo imgInfo = {};
    cl_image_desc desc = {0};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D;
    desc.image_width = 1;

    cl_image_format imageFormat = {CL_R, CL_UNSIGNED_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(desc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;

    imgInfo.useLocalMemory = true;

    StorageInfo storageInfo = {};
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;
    storageInfo.localOnlyRequired = true;

    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, storageInfo, true);
    EXPECT_TRUE(gmm->isCompressionEnabled());
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenUseLocalMemoryInImageInfoFalseWhenGmmIsCreatedThenLocalOnlyNotSet) {
    ImageInfo imgInfo = {};
    cl_image_desc desc = {0};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D;
    desc.image_width = 1;

    cl_image_format imageFormat = {CL_R, CL_UNSIGNED_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(desc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;

    imgInfo.useLocalMemory = false;

    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, StorageInfo{}, false);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
}

TEST_F(MultiTileGmmTests, whenCreateGmmWithImageInfoThenEnableMultiTileArch) {
    ImageInfo imgInfo = {};
    cl_image_desc desc = {0};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D;
    desc.image_width = 1;

    cl_image_format imageFormat = {CL_R, CL_UNSIGNED_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(desc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;

    imgInfo.useLocalMemory = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, StorageInfo{}, false);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.MultiTileArch.Enable);
    imgInfo.useLocalMemory = true;
    gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, StorageInfo{}, false);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.MultiTileArch.Enable);
}

} // namespace NEO
