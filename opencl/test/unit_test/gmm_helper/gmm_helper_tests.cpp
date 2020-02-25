/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

#include "GL/gl.h"
#include "GL/glext.h"
#include "gmm_client_context.h"
#include "gtest/gtest.h"
#include "igfxfmid.h"

using namespace ::testing;

namespace NEO {

extern GMM_INIT_IN_ARGS passedInputArgs;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;

struct GmmTests : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    }
    ExecutionEnvironment *executionEnvironment = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
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

TEST_F(GmmTests, resourceCreation) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096, false));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer(1);
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, resourceCreationUncacheable) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096, true));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer(1);
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC, gmm->resourceParams.Usage);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, resourceCleanupOnDelete) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096, false));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, givenHostPointerWithHighestBitSetWhenGmmIsCreatedItHasTheSameAddress) {
    uintptr_t addressWithHighestBitSet = 0xffff0000;
    auto address = reinterpret_cast<void *>(addressWithHighestBitSet);
    auto expectedAddress = castToUint64(address);

    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), address, 4096, false));
    EXPECT_EQ(gmm->resourceParams.pExistingSysMem, expectedAddress);
}

TEST_F(GmmTests, GivenBufferSizeLargerThenMaxPitchWhenAskedForGmmCreationThenGMMResourceIsCreatedWithNoRestrictionsFlag) {
    auto maxSize = static_cast<size_t>(GmmHelper::maxPossiblePitch);

    MemoryManager *mm = new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment);
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    auto gmmRes = new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, maxSize, false);

    ASSERT_TRUE(gmmRes->gmmResourceInfo.get() != nullptr);

    EXPECT_EQ(gmmRes->resourceParams.Flags.Gpu.NoRestriction, 1u);

    mm->freeSystemMemory(pSysMem);
    delete gmmRes;
    delete mm;
}
TEST_F(GmmTests, givenGmmCreatedFromExistingGmmThenHelperDoesNotReleaseParentGmm) {
    auto size = 4096u;
    void *incomingPtr = (void *)0x1000;
    auto gmmRes = new Gmm(rootDeviceEnvironment->getGmmClientContext(), incomingPtr, size, false);
    auto gmmRes2 = new Gmm(rootDeviceEnvironment->getGmmClientContext(), gmmRes->gmmResourceInfo->peekHandle());

    //copy is being made
    EXPECT_NE(gmmRes2->gmmResourceInfo->peekHandle(), gmmRes->gmmResourceInfo->peekHandle());

    auto allocationSize = gmmRes->gmmResourceInfo->getSizeAllocation();
    EXPECT_NE(0u, allocationSize);
    EXPECT_EQ(allocationSize, gmmRes2->gmmResourceInfo->getSizeAllocation());

    //now delete parent GMM and query child, this shouldn't fail
    delete gmmRes;
    EXPECT_EQ(allocationSize, gmmRes2->gmmResourceInfo->getSizeAllocation());

    delete gmmRes2;
}

TEST_F(GmmTests, invalidImageTypeQuery) {
    cl_image_desc imgDesc{};
    imgDesc.image_width = 10;
    imgDesc.image_type = 0; // invalid
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    EXPECT_THROW(MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo), std::exception);
}

TEST_F(GmmTests, validImageTypeQuery) {
    const HardwareInfo *hwinfo = *platformDevices;
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imgDesc.image_width = 17;
    imgDesc.image_height = 17;
    imgDesc.image_depth = 17;
    size_t pixelSize = 4;
    size_t minSize = imgDesc.image_width * imgDesc.image_height * imgDesc.image_depth * pixelSize;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto queryGmm = MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);

    EXPECT_GT(imgInfo.size, minSize);
    EXPECT_GT(imgInfo.rowPitch, 0u);
    EXPECT_GT(imgInfo.slicePitch, 0u);
    if (hwinfo->platform.eRenderCoreFamily == IGFX_GEN8_CORE) {
        EXPECT_EQ(imgInfo.qPitch, 0u);
    } else {
        EXPECT_GT(imgInfo.qPitch, 0u);
    }

    auto &hwHelper = HwHelper::get(hwinfo->platform.eRenderCoreFamily);

    EXPECT_EQ(queryGmm->resourceParams.Type, GMM_RESOURCE_TYPE::RESOURCE_3D);
    EXPECT_EQ(queryGmm->resourceParams.NoGfxMemory, 1u);

    EXPECT_EQ(queryGmm->resourceParams.Usage, GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);
    EXPECT_EQ(static_cast<uint32_t>(queryGmm->resourceParams.Format),
              static_cast<uint32_t>(GMM_RESOURCE_FORMAT::GMM_FORMAT_R8G8B8A8_UNORM));
    EXPECT_EQ(queryGmm->resourceParams.Flags.Gpu.Texture, 1u);
    EXPECT_EQ(queryGmm->resourceParams.BaseWidth64, 17u);
    EXPECT_EQ(queryGmm->resourceParams.BaseHeight, 17u);
    EXPECT_EQ(queryGmm->resourceParams.Depth, 17u);
    EXPECT_EQ(queryGmm->resourceParams.ArraySize, 1u);
    EXPECT_EQ(!!queryGmm->resourceParams.Flags.Wa.__ForceOtherHVALIGN4, hwHelper.hvAlign4Required());
}

TEST_F(GmmTests, givenWidthWhenCreatingResourceThenSetWidth64Field) {
    const void *dummyPtr = reinterpret_cast<void *>(0x123);
    size_t allocationSize = std::numeric_limits<size_t>::max();
    Gmm gmm(rootDeviceEnvironment->getGmmClientContext(), dummyPtr, allocationSize, false);
    EXPECT_EQ(static_cast<uint64_t>(allocationSize), gmm.resourceParams.BaseWidth64);
}

TEST_F(GmmTests, givenNullptrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = nullptr;
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096, false));

    EXPECT_EQ(gmm->resourceParams.NoGfxMemory, 1u);
}

TEST_F(GmmTests, givenPtrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = reinterpret_cast<void *>(0x1111);
    std::unique_ptr<Gmm> gmm(new Gmm(rootDeviceEnvironment->getGmmClientContext(), pSysMem, 4096, false));

    EXPECT_EQ(gmm->resourceParams.NoGfxMemory, 0u);
}

TEST_F(GmmTests, given2DimageFromBufferParametersWhenGmmResourceIsCreatedThenItHasDesiredPitchAndSize) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 329;
    imgDesc.image_height = 349;
    imgDesc.image_depth = 1;
    imgDesc.image_row_pitch = 5312;
    imgDesc.buffer = (cl_mem)0x1000;

    ClSurfaceFormatInfo surfaceFormat = {{CL_RGBA, CL_FLOAT}, {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, (GFX3DSTATE_SURFACEFORMAT)0, 0, 4, 4, 16}};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    auto queryGmm = MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);
    auto renderSize = queryGmm->gmmResourceInfo->getSizeAllocation();

    size_t expectedSize = imgDesc.image_row_pitch * imgDesc.image_height;
    EXPECT_GE(renderSize, expectedSize);
    EXPECT_EQ(imgDesc.image_row_pitch, queryGmm->gmmResourceInfo->getRenderPitch());
}

TEST_F(GmmTests, given2DimageFromBufferParametersWhenGmmResourceIsCreatedAndPitchIsOverridedThenItHasDesiredPitchAndSize) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 329;
    imgDesc.image_height = 349;
    imgDesc.image_depth = 1;
    imgDesc.image_row_pitch = 5376;
    imgDesc.buffer = (cl_mem)0x1000;

    ClSurfaceFormatInfo surfaceFormat = {{CL_RGBA, CL_FLOAT}, {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, (GFX3DSTATE_SURFACEFORMAT)0, 0, 4, 4, 16}};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    EXPECT_EQ(imgInfo.imgDesc.imageRowPitch, imgDesc.image_row_pitch);
    auto queryGmm = MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);
    auto renderSize = queryGmm->gmmResourceInfo->getSizeAllocation();

    size_t expectedSize = imgDesc.image_row_pitch * imgDesc.image_height;
    EXPECT_GE(renderSize, expectedSize);
    EXPECT_EQ(imgDesc.image_row_pitch, queryGmm->gmmResourceInfo->getRenderPitch());
}

TEST_F(GmmTests, givenPlanarFormatsWhenQueryingImageParamsThenUVOffsetIsQueried) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_depth = 1;

    ClSurfaceFormatInfo surfaceFormatNV12 = {{CL_NV12_INTEL, CL_UNORM_INT8}, {GMM_FORMAT_NV12, GFX3DSTATE_SURFACEFORMAT_NV12, 0, 1, 1, 1}};
    ClSurfaceFormatInfo surfaceFormatP010 = {{CL_R, CL_UNORM_INT16}, {GMM_FORMAT_P010, GFX3DSTATE_SURFACEFORMAT_NV12, 0, 1, 2, 2}};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormatNV12);
    imgInfo.yOffsetForUVPlane = 0;
    MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);

    EXPECT_NE(0u, imgInfo.yOffsetForUVPlane);

    imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormatP010);
    imgInfo.yOffsetForUVPlane = 0;

    MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);
    EXPECT_NE(0u, imgInfo.yOffsetForUVPlane);
}

TEST_F(GmmTests, givenTilingModeSetToTileYWhenHwSupportsTilingThenTileYFlagIsSet) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_depth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.linearStorage = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), imgInfo, StorageInfo{});

    EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 0u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
}

TEST_F(GmmTests, givenTilingModeSetToNonTiledWhenCreatingGmmThenLinearFlagIsSet) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_depth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.linearStorage = true;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), imgInfo, StorageInfo{});

    EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 1u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
}

TEST_F(GmmTests, givenZeroRowPitchWhenQueryImgFromBufferParamsThenCalculate) {
    MockGraphicsAllocation bufferAllocation(nullptr, 4096);

    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    imgDesc.image_row_pitch = 0;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    size_t expectedRowPitch = imgDesc.image_width * imgInfo.surfaceFormat->ImageElementSizeInBytes;
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &bufferAllocation);

    EXPECT_EQ(imgInfo.rowPitch, expectedRowPitch);
}

TEST_F(GmmTests, givenNonZeroRowPitchWhenQueryImgFromBufferParamsThenUseUserValue) {
    MockGraphicsAllocation bufferAllocation(nullptr, 4096);

    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 5;
    imgDesc.image_height = 5;
    imgDesc.image_row_pitch = 123;
    size_t expectedRowPitch = imgDesc.image_row_pitch;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &bufferAllocation);

    EXPECT_EQ(imgInfo.rowPitch, expectedRowPitch);
}

TEST_F(GmmTests, canonize) {
    auto hwInfo = *platformDevices[0];

    // 48 bit - canonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); // 0x0000FFFFFFFFFFFF;
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    uint64_t testAddr1 = 0x7777777777777777;
    uint64_t goodAddr1 = 0x0000777777777777;
    EXPECT_EQ(GmmHelper::canonize(testAddr1), goodAddr1);

    uint64_t testAddr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t goodAddr2 = 0xFFFFFFFFFFFFFFFF;
    EXPECT_EQ(GmmHelper::canonize(testAddr2), goodAddr2);

    // 36 bit - also canonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36); // 0x0000000FFFFFFFFF;
    gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    EXPECT_EQ(GmmHelper::canonize(testAddr1), goodAddr1);
    EXPECT_EQ(GmmHelper::canonize(testAddr2), goodAddr2);
}

TEST_F(GmmTests, decanonize) {
    auto hwInfo = *platformDevices[0];

    // 48 bit - decanonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); //0x0000FFFFFFFFFFFF;
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    uint64_t testAddr1 = 0x7777777777777777;
    uint64_t goodAddr1 = 0x0000777777777777;
    EXPECT_EQ(GmmHelper::decanonize(testAddr1), goodAddr1);

    uint64_t testAddr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t goodAddr2 = 0x0000FFFFFFFFFFFF;
    EXPECT_EQ(GmmHelper::decanonize(testAddr2), goodAddr2);

    // 36 bit - also decanonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36); // 0x0000000FFFFFFFFF;
    gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    EXPECT_EQ(GmmHelper::decanonize(testAddr1), goodAddr1);
    EXPECT_EQ(GmmHelper::decanonize(testAddr2), goodAddr2);
}

TEST_F(GmmTests, givenMipmapedInputWhenAskedForHalingThenNonDefaultValueIsReturned) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 60;
    imgDesc.image_height = 40;
    imgDesc.image_depth = 1;
    int mipLevel = 5;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, mipLevel, nullptr);

    auto queryGmm = MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);

    EXPECT_EQ(static_cast<int>(queryGmm->resourceParams.MaxLod), mipLevel);
}

TEST_F(GmmTests, givenNumSamplesWhenAskedForMultisamplesCountThenReturnValue) {
    uint32_t numSamples[5][2] = {{0, 0}, {2, 1}, {4, 2}, {8, 3}, {16, 4}}; //{given, expected}
    for (int i = 0; i < 5; i++) {
        auto result = GmmTypesConverter::getRenderMultisamplesCount(numSamples[i][0]);
        EXPECT_EQ(numSamples[i][1], result);
    }
}

struct GmmMediaCompressedTests : public GmmTests {
    void SetUp() override {
        GmmTests::SetUp();
        StorageInfo info;
        gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), nullptr, 4, false, true, true, info);
        flags = gmm->gmmResourceInfo->getResourceFlags();
        flags->Gpu.CCS = true;
        flags->Gpu.UnifiedAuxSurface = true;
    }
    std::unique_ptr<Gmm> gmm;
    GMM_RESOURCE_FLAG *flags;
};

TEST_F(GmmMediaCompressedTests, givenMediaCompressedGmmUnifiedAuxTranslationCapableReturnsTrue) {
    flags->Info.MediaCompressed = true;
    flags->Info.RenderCompressed = false;

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST_F(GmmMediaCompressedTests, givenRenderCompressedGmmUnifiedAuxTranslationCapableReturnsTrue) {
    flags->Info.MediaCompressed = false;
    flags->Info.RenderCompressed = true;

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST_F(GmmMediaCompressedTests, givenMediaAndRenderCompressedGmmUnifiedAuxTranslationCapableThrowsException) {
    flags->Info.MediaCompressed = true;
    flags->Info.RenderCompressed = true;

    EXPECT_THROW(gmm->unifiedAuxTranslationCapable(), std::exception);
}

TEST_F(GmmMediaCompressedTests, givenNotMediaAndNotRenderCompressedGmmUnifiedAuxTranslationCapableReturnsFalse) {
    flags->Info.MediaCompressed = false;
    flags->Info.RenderCompressed = false;

    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable());
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

TEST_F(GmmTests, converNeoPlaneToGmmPlane) {
    std::vector<std::pair<ImagePlane, GMM_YUV_PLANE>> v = {{ImagePlane::NO_PLANE, GMM_YUV_PLANE::GMM_NO_PLANE},
                                                           {ImagePlane::PLANE_Y, GMM_YUV_PLANE::GMM_PLANE_Y},
                                                           {ImagePlane::PLANE_U, GMM_YUV_PLANE::GMM_PLANE_U},
                                                           {ImagePlane::PLANE_UV, GMM_YUV_PLANE::GMM_PLANE_U},
                                                           {ImagePlane::PLANE_V, GMM_YUV_PLANE::GMM_PLANE_V}};

    for (auto p : v) {
        EXPECT_TRUE(p.second == GmmTypesConverter::convertPlane(p.first));
    }
}

class GmmImgTest : public GmmTests,
                   public ::testing::WithParamInterface<uint32_t /*cl_mem_object_type*/> {};

INSTANTIATE_TEST_CASE_P(
    GmmImgTests,
    GmmImgTest,
    testing::ValuesIn(GmmTestConst::imgTypes));

TEST_P(GmmImgTest, updateImgInfoAndDesc) {
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
    updateImgInfo.plane = GMM_YUV_PLANE::GMM_PLANE_U;

    uint32_t expectCalls = 1u;
    GMM_REQ_OFFSET_INFO expectedReqInfo[2] = {};
    expectedReqInfo[0].ReqLock = 1;
    expectedReqInfo[1].ReqRender = 1;
    expectedReqInfo[1].Plane = updateImgInfo.plane;

    cl_image_desc imgDesc{};
    imgDesc.image_type = GetParam();
    imgDesc.image_width = 60;
    imgDesc.image_height = 1;
    imgDesc.image_depth = 1;
    imgDesc.image_array_size = 1;
    cl_uint arrayIndex = 0;

    if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE2D || imgDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY || imgDesc.image_type == CL_MEM_OBJECT_IMAGE3D) {
        imgDesc.image_height = 40;
    }
    if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE3D) {
        imgDesc.image_depth = 5;
        expectCalls = 2u;
        expectedReqInfo[0].Slice = 1;
    }

    if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY ||
        imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        imgDesc.image_array_size = 5;
        expectCalls = 2u;
        arrayIndex = 2;
        expectedReqInfo[0].ArrayIndex = 1;
        expectedReqInfo[1].ArrayIndex = arrayIndex;
    }

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto queryGmm = MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);

    auto mockResInfo = new NiceMock<MyMockGmmResourceInfo>(&queryGmm->resourceParams);
    queryGmm->gmmResourceInfo.reset(mockResInfo);

    queryGmm->updateImgInfoAndDesc(updateImgInfo, arrayIndex);
    EXPECT_EQ(expectCalls, mockResInfo->getOffsetCalled);

    EXPECT_EQ(imgDesc.image_width, updateImgInfo.imgDesc.imageWidth);
    EXPECT_EQ(imgDesc.image_height, updateImgInfo.imgDesc.imageHeight);
    EXPECT_EQ(imgDesc.image_depth, updateImgInfo.imgDesc.imageDepth);
    EXPECT_EQ(imgDesc.image_array_size, updateImgInfo.imgDesc.imageArraySize);
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
}

TEST(GmmImgTest, givenImgInfoWhenUpdatingOffsetsCallGmmToGetOffsets) {
    struct GmmGetOffsetOutput {
        uint32_t Offset;
        uint32_t XOffset;
        uint32_t YOffset;
    };

    struct MyMockGmmResourceInfo : MockGmmResourceInfo {
        MyMockGmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams) : MockGmmResourceInfo(resourceCreateParams) {}
        GMM_STATUS getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) override {
            EXPECT_EQ(1u, reqOffsetInfo.ReqRender);
            EXPECT_EQ(0u, reqOffsetInfo.Slice);
            EXPECT_EQ(expectedArrayIndex, reqOffsetInfo.ArrayIndex);
            EXPECT_EQ(expectedGmmPlane, reqOffsetInfo.Plane);

            reqOffsetInfo.Render.Offset = gmmGetOffsetOutput.Offset;
            reqOffsetInfo.Render.XOffset = gmmGetOffsetOutput.XOffset;
            reqOffsetInfo.Render.YOffset = gmmGetOffsetOutput.YOffset;
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

    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imgDesc.image_width = 60;
    imgDesc.image_height = 1;
    imgDesc.image_depth = 1;
    imgDesc.image_array_size = 10;

    ImageInfo imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    std::unique_ptr<Gmm> gmm = MockGmm::queryImgParams(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), imgInfo);
    MyMockGmmResourceInfo *mockGmmResourceInfo = new MyMockGmmResourceInfo(&gmm->resourceParams);
    gmm->gmmResourceInfo.reset(mockGmmResourceInfo);

    mockGmmResourceInfo->expectedArrayIndex = 7;
    mockGmmResourceInfo->expectedGmmPlane = imgInfo.plane;
    mockGmmResourceInfo->gmmGetOffsetOutput = {10, 111, 120};
    mockGmmResourceInfo->gmmGetBitsPerPixelOutput = 24;
    gmm->updateOffsetsInImgInfo(imgInfo, mockGmmResourceInfo->expectedArrayIndex);
    EXPECT_EQ(mockGmmResourceInfo->gmmGetOffsetOutput.Offset, imgInfo.offset);
    const auto expectedXOffset = mockGmmResourceInfo->gmmGetOffsetOutput.XOffset / (mockGmmResourceInfo->gmmGetBitsPerPixelOutput / 8);
    EXPECT_EQ(expectedXOffset, imgInfo.xOffset);
    EXPECT_EQ(mockGmmResourceInfo->gmmGetOffsetOutput.YOffset, imgInfo.yOffset);
}

TEST_F(GmmTests, copyResourceBlt) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imgDesc.image_width = 17;
    imgDesc.image_height = 17;
    imgDesc.image_depth = 17;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto gmm = MockGmm::queryImgParams(rootDeviceEnvironment->getGmmClientContext(), imgInfo);
    auto mockResInfo = reinterpret_cast<NiceMock<MockGmmResourceInfo> *>(gmm->gmmResourceInfo.get());

    GMM_RES_COPY_BLT requestedCpuBlt = {};
    GMM_RES_COPY_BLT expectedCpuBlt = {};
    char sys(0), gpu(0);
    uint32_t pitch = 300;
    uint32_t height = 400;
    uint8_t upload = 1u;

    expectedCpuBlt.Sys.pData = &sys;
    expectedCpuBlt.Gpu.pData = &gpu;
    expectedCpuBlt.Sys.RowPitch = pitch;
    expectedCpuBlt.Blt.Upload = upload;
    expectedCpuBlt.Sys.BufferSize = pitch * height;

    auto invokeParamsCopy = [&](GMM_RES_COPY_BLT *resCopyBlt) {
        requestedCpuBlt = *resCopyBlt;
        return 1;
    };

    // plane Y
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    auto retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_Y);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    // no-plane
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::NO_PLANE);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    //plane UV
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_UV);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    //plane V
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_V);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    //plane U
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u + height * pitch / 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_U);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
}

TEST(GmmTest, givenAllValidFlagsWhenAskedForUnifiedAuxTranslationCapabilityThenReturnTrue) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 1, false));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

    mockResource->setUnifiedAuxTranslationCapable();
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Info.RenderCompressed);

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST(GmmTest, givenInvalidFlagsSetWhenAskedForUnifiedAuxTranslationCapabilityThenReturnFalse) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 1, false));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

    mockResource->mockResourceCreateParams.Flags.Gpu.CCS = 0;
    mockResource->mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface = 1;
    mockResource->mockResourceCreateParams.Flags.Info.RenderCompressed = 1;
    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable()); // CCS == 0

    mockResource->mockResourceCreateParams.Flags.Gpu.CCS = 1;
    mockResource->mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface = 0;
    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable()); // UnifiedAuxSurface == 0

    mockResource->mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface = 1;
    mockResource->mockResourceCreateParams.Flags.Info.RenderCompressed = 0;
    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable()); // RenderCompressed == 0
}

TEST(GmmTest, givenHwInfoWhenDeviceIsCreatedTheSetThisHwInfoToGmmHelper) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_EQ(&device->getHardwareInfo(), device->getGmmHelper()->getHardwareInfo());
}

TEST(GmmTest, whenResourceIsCreatedThenHandleItsOwnership) {
    struct MyMockResourecInfo : public GmmResourceInfo {
        using GmmResourceInfo::resourceInfo;

        MyMockResourecInfo(GMM_RESCREATE_PARAMS *inputParams) : GmmResourceInfo(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), inputParams){};
        MyMockResourecInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo) : GmmResourceInfo(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), inputGmmResourceInfo){};
    };

    GMM_RESCREATE_PARAMS gmmParams = {};
    gmmParams.Type = RESOURCE_BUFFER;
    gmmParams.Format = GMM_FORMAT_GENERIC_8BIT;
    gmmParams.BaseWidth64 = 1;
    gmmParams.BaseHeight = 1;
    gmmParams.Depth = 1;
    gmmParams.Flags.Info.Linear = 1;
    gmmParams.Flags.Info.Cacheable = 1;
    gmmParams.Flags.Gpu.Texture = 1;
    gmmParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;

    MyMockResourecInfo myMockResourceInfo1(&gmmParams);
    EXPECT_NE(nullptr, myMockResourceInfo1.resourceInfo.get());

    MyMockResourecInfo myMockResourceInfo2(myMockResourceInfo1.resourceInfo.get());
    EXPECT_NE(nullptr, myMockResourceInfo2.resourceInfo.get());

    EXPECT_NE(myMockResourceInfo1.resourceInfo.get(), myMockResourceInfo2.resourceInfo.get());
}

TEST(GmmTest, givenGmmWithNotSetMCSInResourceInfoGpuFlagsWhenCallHasMultisampleControlSurfaceThenReturnFalse) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 1, false));
    EXPECT_FALSE(gmm->hasMultisampleControlSurface());
}

TEST(GmmTest, givenGmmWithSetMCSInResourceInfoGpuFlagsWhenCallhasMultisampleControlSurfaceThenReturnTrue) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 1, false));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResource->setMultisampleControlSurface();
    EXPECT_TRUE(gmm->hasMultisampleControlSurface());
}

TEST(GmmHelperTest, whenGmmHelperIsInitializedThenClientContextIsSet) {
    ASSERT_NE(nullptr, platform()->peekGmmClientContext());
    EXPECT_NE(nullptr, platform()->peekGmmClientContext()->getHandle());
}

TEST(GmmHelperTest, givenPlatformAlreadyDestroyedWhenResourceIsBeingDestroyedThenObserveNoExceptions) {
    struct MockGmmResourecInfo : public GmmResourceInfo {
        using GmmResourceInfo::resourceInfo;
        MockGmmResourecInfo(GMM_RESCREATE_PARAMS *inputParams) : GmmResourceInfo(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), inputParams){};
    };

    GMM_RESCREATE_PARAMS gmmParams = {};
    gmmParams.Type = RESOURCE_BUFFER;
    gmmParams.Format = GMM_FORMAT_GENERIC_8BIT;
    gmmParams.BaseWidth64 = 1;
    gmmParams.BaseHeight = 1;
    gmmParams.Depth = 1;
    gmmParams.Flags.Info.Linear = 1;
    gmmParams.Flags.Info.Cacheable = 1;
    gmmParams.Flags.Gpu.Texture = 1;
    gmmParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;

    auto gmmResourceInfo = new MockGmmResourecInfo(&gmmParams);

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->incRefInternal();
    platformsImpl.clear();
    EXPECT_EQ(nullptr, platform());

    EXPECT_NO_THROW(delete gmmResourceInfo);

    executionEnvironment->decRefInternal();
}

TEST(GmmHelperTest, givenValidGmmFunctionsWhenCreateGmmHelperWithInitializedOsInterfaceThenProperParametersArePassed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    size_t numDevices;
    DeviceFactory::getDevices(numDevices, *executionEnvironment);
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = platformDevices[0];
    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    gmmHelper.reset(new GmmHelper(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(), hwInfo));
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(&hwInfo->gtSystemInfo, passedInputArgs.pGtSysInfo);
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}

TEST(GmmHelperTest, givenValidGmmFunctionsWhenCreateGmmHelperWithoutOsInterfaceThenInitializationDoesntCrashAndProperParametersArePassed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = platformDevices[0];
    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    gmmHelper.reset(new GmmHelper(nullptr, hwInfo));
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(&hwInfo->gtSystemInfo, passedInputArgs.pGtSysInfo);
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}

} // namespace NEO
