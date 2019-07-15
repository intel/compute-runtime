/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/platform/platform.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "GL/gl.h"
#include "GL/glext.h"
#include "gmm_client_context.h"
#include "gtest/gtest.h"
#include "igfxfmid.h"

using namespace ::testing;

namespace NEO {
struct GmmTests : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
    }
    ExecutionEnvironment *executionEnvironment;
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
        EXPECT_TRUE(p.first == NEO::GmmHelper::getCubeFaceIndex(p.second));
        maxVal = std::max(maxVal, p.second);
    }
    maxVal++;
    EXPECT_TRUE(__GMM_NO_CUBE_MAP == NEO::GmmHelper::getCubeFaceIndex(maxVal));
}

TEST_F(GmmTests, resourceCreation) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);
    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096, false));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer(1);
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, resourceCreationUncacheable) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096, true));

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

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096, false));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, GivenBufferSizeLargerThenMaxPitchWhenAskedForGmmCreationThenGMMResourceIsCreatedWithNoRestrictionsFlag) {
    auto maxSize = static_cast<size_t>(GmmHelper::maxPossiblePitch);

    MemoryManager *mm = new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment);
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    auto gmmRes = new Gmm(pSysMem, maxSize, false);

    ASSERT_TRUE(gmmRes->gmmResourceInfo.get() != nullptr);

    EXPECT_EQ(gmmRes->resourceParams.Flags.Gpu.NoRestriction, 1u);

    mm->freeSystemMemory(pSysMem);
    delete gmmRes;
    delete mm;
}
TEST_F(GmmTests, givenGmmCreatedFromExistingGmmThenHelperDoesNotReleaseParentGmm) {
    auto size = 4096u;
    void *incomingPtr = (void *)0x1000;
    auto gmmRes = new Gmm(incomingPtr, size, false);
    auto gmmRes2 = new Gmm(gmmRes->gmmResourceInfo->peekHandle());

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

    EXPECT_THROW(MockGmm::queryImgParams(imgInfo), std::exception);
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

    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_GT(imgInfo.size, minSize);
    EXPECT_GT(imgInfo.rowPitch, 0u);
    EXPECT_GT(imgInfo.slicePitch, 0u);
    if (hwinfo->platform.eRenderCoreFamily == IGFX_GEN8_CORE) {
        EXPECT_EQ(imgInfo.qPitch, 0u);
    } else {
        EXPECT_GT(imgInfo.qPitch, 0u);
    }

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
    EXPECT_EQ(queryGmm->resourceParams.Flags.Wa.__ForceOtherHVALIGN4, 1u);
}

TEST_F(GmmTests, givenWidthWhenCreatingResourceThenSetWidth64Field) {
    const void *dummyPtr = reinterpret_cast<void *>(0x123);
    size_t allocationSize = std::numeric_limits<size_t>::max();
    Gmm gmm(dummyPtr, allocationSize, false);
    EXPECT_EQ(static_cast<uint64_t>(allocationSize), gmm.resourceParams.BaseWidth64);
}

TEST_F(GmmTests, givenNullptrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = nullptr;
    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096, false));

    EXPECT_EQ(gmm->resourceParams.NoGfxMemory, 1u);
}

TEST_F(GmmTests, givenPtrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = reinterpret_cast<void *>(0x1111);
    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096, false));

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

    SurfaceFormatInfo surfaceFormat = {{CL_RGBA, CL_FLOAT}, GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, (GFX3DSTATE_SURFACEFORMAT)0, 0, 4, 4, 16};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);
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

    SurfaceFormatInfo surfaceFormat = {{CL_RGBA, CL_FLOAT}, GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, (GFX3DSTATE_SURFACEFORMAT)0, 0, 4, 4, 16};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    EXPECT_EQ(imgInfo.imgDesc->image_row_pitch, imgDesc.image_row_pitch);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);
    auto renderSize = queryGmm->gmmResourceInfo->getSizeAllocation();

    size_t expectedSize = imgDesc.image_row_pitch * imgDesc.image_height;
    EXPECT_GE(renderSize, expectedSize);
    EXPECT_EQ(imgDesc.image_row_pitch, queryGmm->gmmResourceInfo->getRenderPitch());
}

TEST_F(GmmTests, givenTilableImageWhenEnableForceLinearImagesThenYTilingIsDisabled) {
    DebugManagerStateRestore debugStateBackup;
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imgDesc.image_width = 17;
    imgDesc.image_height = 17;
    imgDesc.image_depth = 17;

    DebugManager.flags.ForceLinearImages.set(false);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    auto &hwHelper = HwHelper::get(GmmHelper::getInstance()->getHardwareInfo()->platform.eRenderCoreFamily);
    bool supportsYTiling = hwHelper.supportsYTiling();

    if (!supportsYTiling) {
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.Linear, 0u);
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.TiledY, 0u);
    } else {
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.Linear, 1u);
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.TiledY, 1u);
    }

    DebugManager.flags.ForceLinearImages.set(true);

    delete queryGmm.get();
    queryGmm.release();
    queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_EQ(queryGmm->resourceParams.Flags.Info.Linear, 1u);
    EXPECT_EQ(queryGmm->resourceParams.Flags.Info.TiledY, 0u);
}

TEST_F(GmmTests, givenPlanarFormatsWhenQueryingImageParamsThenUVOffsetIsQueried) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_depth = 1;

    SurfaceFormatInfo surfaceFormatNV12 = {{CL_NV12_INTEL, CL_UNORM_INT8}, GMM_FORMAT_NV12, GFX3DSTATE_SURFACEFORMAT_NV12, 0, 1, 1, 1};
    SurfaceFormatInfo surfaceFormatP010 = {{CL_R, CL_UNORM_INT16}, GMM_FORMAT_P010, GFX3DSTATE_SURFACEFORMAT_NV12, 0, 1, 2, 2};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormatNV12);
    imgInfo.yOffsetForUVPlane = 0;
    MockGmm::queryImgParams(imgInfo);

    EXPECT_NE(0u, imgInfo.yOffsetForUVPlane);

    imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormatP010);
    imgInfo.yOffsetForUVPlane = 0;

    MockGmm::queryImgParams(imgInfo);
    EXPECT_NE(0u, imgInfo.yOffsetForUVPlane);
}

TEST_F(GmmTests, givenTilingModeSetToTileYWhenHwSupportsTilingThenTileYFlagIsSet) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_depth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.tilingMode = TilingMode::TILE_Y;
    auto gmm = std::make_unique<Gmm>(imgInfo, StorageInfo{});

    auto &hwHelper = HwHelper::get(GmmHelper::getInstance()->getHardwareInfo()->platform.eRenderCoreFamily);
    bool supportsYTiling = hwHelper.supportsYTiling();

    if (!supportsYTiling) {
        EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 0u);
        EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
    } else {
        EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 1u);
        EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 1u);
    }
}

TEST_F(GmmTests, givenTilingModeSetToNonTiledWhenCreatingGmmThenLinearFlagIsSet) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_depth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.tilingMode = TilingMode::NON_TILED;
    auto gmm = std::make_unique<Gmm>(imgInfo, StorageInfo{});

    EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 1u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
}

TEST_F(GmmTests, givenTilingModeSetToTileXWhenCreatingGmmThenUnrecoverableIfIsCalled) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_depth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.tilingMode = TilingMode::TILE_X;

    EXPECT_THROW(new Gmm(imgInfo, {}), std::exception);
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
    GmmHelper::queryImgFromBufferParams(imgInfo, &bufferAllocation);

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
    GmmHelper::queryImgFromBufferParams(imgInfo, &bufferAllocation);

    EXPECT_EQ(imgInfo.rowPitch, expectedRowPitch);
}

TEST_F(GmmTests, canonize) {
    uint64_t addr1 = 0x7777777777777777;
    uint64_t addrExpected1 = 0x0000777777777777;
    EXPECT_EQ(GmmHelper::canonize(addr1), addrExpected1);

    uint64_t addr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t addrExpected2 = 0xFFFFFFFFFFFFFFFF;
    EXPECT_EQ(GmmHelper::canonize(addr2), addrExpected2);
}

TEST_F(GmmTests, decanonize) {
    uint64_t addr1 = 0x7777777777777777;
    uint64_t addrExpected1 = 0x0000777777777777;
    EXPECT_EQ(GmmHelper::decanonize(addr1), addrExpected1);

    uint64_t addr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t addrExpected2 = 0x0000FFFFFFFFFFFF;
    EXPECT_EQ(GmmHelper::decanonize(addr2), addrExpected2);
}

TEST_F(GmmTests, returnRenderTileMode) {
    uint32_t alignments[5][2] = {{0, 1}, {4, 1}, {8, 2}, {16, 3}, {20, 1}}; // {given, expected}
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(GmmHelper::getRenderAlignment(alignments[i][0]), alignments[i][1]);
    }
}

TEST_F(GmmTests, givenMipmapedInputWhenAskedForHalingThenNonDefaultValueIsReturned) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 60;
    imgDesc.image_height = 40;
    imgDesc.image_depth = 1;
    int mipLevel = 5;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, mipLevel, nullptr);

    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_EQ(static_cast<int>(queryGmm->resourceParams.MaxLod), mipLevel);
}

TEST_F(GmmTests, givenNumSamplesWhenAskedForMultisamplesCountThenReturnValue) {
    uint32_t numSamples[5][2] = {{0, 0}, {2, 1}, {4, 2}, {8, 3}, {16, 4}}; //{given, expected}
    for (int i = 0; i < 5; i++) {
        auto result = GmmHelper::getRenderMultisamplesCount(numSamples[i][0]);
        EXPECT_EQ(numSamples[i][1], result);
    }
}

namespace GmmTestConst {
static const cl_mem_object_type imgTypes[6] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D};

static const cl_mem_object_type imgFromBufferTypes[2] = {
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE2D};
} // namespace GmmTestConst

class GmmTiling : public GmmTests,
                  public ::testing::WithParamInterface<uint32_t /*cl_mem_object_type*/> {
  public:
    void checkTiling(cl_image_desc &imgDesc, bool forceLinear) {
        bool allowTiling = GmmHelper::allowTiling(imgDesc);
        if (forceLinear) {
            EXPECT_FALSE(allowTiling);
        } else {
            if (imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D ||
                imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
                imgDesc.image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER ||
                imgDesc.buffer != nullptr) {
                EXPECT_FALSE(allowTiling);
            } else {
                EXPECT_TRUE(allowTiling);
            }
        }
    };
};

class GmmImgTilingTests : public GmmTiling {};

INSTANTIATE_TEST_CASE_P(
    GmmTiledTests,
    GmmImgTilingTests,
    testing::ValuesIn(GmmTestConst::imgTypes));

TEST_P(GmmImgTilingTests, allowTiling) {
    bool defaultTilingType = DebugManager.flags.ForceLinearImages.get();

    cl_image_desc imgDesc = {};
    imgDesc.image_type = GetParam();

    checkTiling(imgDesc, defaultTilingType);

    DebugManager.flags.ForceLinearImages.set(!defaultTilingType);

    checkTiling(imgDesc, !defaultTilingType);

    DebugManager.flags.ForceLinearImages.set(defaultTilingType);
}

class GmmImgFromBufferTilingTests : public GmmTiling {};
INSTANTIATE_TEST_CASE_P(
    GmmTiledTests,
    GmmImgFromBufferTilingTests,
    testing::ValuesIn(GmmTestConst::imgFromBufferTypes));

TEST_P(GmmImgFromBufferTilingTests, disallowImgFromBufferTiling) {
    bool defaultTilingType = DebugManager.flags.ForceLinearImages.get();
    if (defaultTilingType) {
        DebugManager.flags.ForceLinearImages.set(false);
    }

    cl_image_desc imgDesc = {};
    imgDesc.image_type = GetParam();
    _cl_mem clMem = {};
    imgDesc.buffer = &clMem;

    checkTiling(imgDesc, false);

    DebugManager.flags.ForceLinearImages.set(defaultTilingType);
}

TEST_F(GmmTests, converOclPlaneToGmmPlane) {
    std::vector<std::pair<OCLPlane, GMM_YUV_PLANE>> v = {{OCLPlane::NO_PLANE, GMM_YUV_PLANE::GMM_NO_PLANE},
                                                         {OCLPlane::PLANE_Y, GMM_YUV_PLANE::GMM_PLANE_Y},
                                                         {OCLPlane::PLANE_U, GMM_YUV_PLANE::GMM_PLANE_U},
                                                         {OCLPlane::PLANE_UV, GMM_YUV_PLANE::GMM_PLANE_U},
                                                         {OCLPlane::PLANE_V, GMM_YUV_PLANE::GMM_PLANE_V}};

    for (auto p : v) {
        EXPECT_TRUE(p.second == GmmHelper::convertPlane(p.first));
    }
}

class GmmImgTest : public GmmTiling {};

INSTANTIATE_TEST_CASE_P(
    GmmImgTests,
    GmmImgTest,
    testing::ValuesIn(GmmTestConst::imgTypes));

TEST_P(GmmImgTest, updateImgInfo) {
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
    cl_image_desc updateImgDesc = {};
    updateImgInfo.imgDesc = &updateImgDesc;
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
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    auto mockResInfo = new NiceMock<MyMockGmmResourceInfo>(&queryGmm->resourceParams);
    queryGmm->gmmResourceInfo.reset(mockResInfo);

    queryGmm->updateImgInfo(updateImgInfo, updateImgDesc, arrayIndex);
    EXPECT_EQ(expectCalls, mockResInfo->getOffsetCalled);

    EXPECT_EQ(imgDesc.image_width, updateImgDesc.image_width);
    EXPECT_EQ(imgDesc.image_height, updateImgDesc.image_height);
    EXPECT_EQ(imgDesc.image_depth, updateImgDesc.image_depth);
    EXPECT_EQ(imgDesc.image_array_size, updateImgDesc.image_array_size);
    EXPECT_GT(updateImgDesc.image_row_pitch, 0u);
    EXPECT_GT(updateImgDesc.image_slice_pitch, 0u);

    if (expectCalls == 1) {
        EXPECT_TRUE(memcmp(&expectedReqInfo[1], &mockResInfo->givenReqInfo[0], sizeof(GMM_REQ_OFFSET_INFO)) == 0);
    } else if (expectCalls == 2u) {
        EXPECT_TRUE(memcmp(&expectedReqInfo[0], &mockResInfo->givenReqInfo[0], sizeof(GMM_REQ_OFFSET_INFO)) == 0);
        EXPECT_TRUE(memcmp(&expectedReqInfo[1], &mockResInfo->givenReqInfo[1], sizeof(GMM_REQ_OFFSET_INFO)) == 0);
    } else {
        EXPECT_TRUE(false);
    }
}

TEST_F(GmmTests, copyResourceBlt) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imgDesc.image_width = 17;
    imgDesc.image_height = 17;
    imgDesc.image_depth = 17;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto gmm = MockGmm::queryImgParams(imgInfo);
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
    auto retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, OCLPlane::PLANE_Y);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    // no-plane
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, OCLPlane::NO_PLANE);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    //plane UV
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, OCLPlane::PLANE_UV);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    //plane V
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, OCLPlane::PLANE_V);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);

    //plane U
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u + height * pitch / 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    EXPECT_CALL(*mockResInfo, cpuBlt(_)).Times(1).WillOnce(Invoke(invokeParamsCopy));
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, OCLPlane::PLANE_U);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
}

TEST(GmmTest, givenAllValidFlagsWhenAskedForUnifiedAuxTranslationCapabilityThenReturnTrue) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

    mockResource->setUnifiedAuxTranslationCapable();
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Info.RenderCompressed);

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST(GmmTest, givenInvalidFlagsSetWhenAskedForUnifiedAuxTranslationCapabilityThenReturnFalse) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
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
    EXPECT_EQ(device->getExecutionEnvironment()->getHardwareInfo(), device->getGmmHelper()->getHardwareInfo());
}

TEST(GmmTest, whenResourceIsCreatedThenHandleItsOwnership) {
    struct MyMockResourecInfo : public GmmResourceInfo {
        using GmmResourceInfo::resourceInfo;

        MyMockResourecInfo(GMM_RESCREATE_PARAMS *inputParams) : GmmResourceInfo(inputParams){};
        MyMockResourecInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo) : GmmResourceInfo(inputGmmResourceInfo){};
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
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    EXPECT_FALSE(gmm->hasMultisampleControlSurface());
}

TEST(GmmTest, givenGmmWithSetMCSInResourceInfoGpuFlagsWhenCallhasMultisampleControlSurfaceThenReturnTrue) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(nullptr, 1, false));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResource->setMultisampleControlSurface();
    EXPECT_TRUE(gmm->hasMultisampleControlSurface());
}

TEST(GmmSimplifiedCacheSelectionPolicy, givenGmmInSimplifiedCacheSelectionPolicyWhenItIsAskedForUncachedIndexThen0IsReturned) {
    GmmHelper gmmHelper(*platformDevices);
    gmmHelper.setSimplifiedMocsTableUsage(true);
    auto index = gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
    auto expectedIndex = GmmHelper::cacheDisabledIndex;
    EXPECT_EQ(expectedIndex, index);
}

TEST(GmmSimplifiedCacheSelectionPolicy, givenGmmInSimplifiedCacheSelectionPolicyWhenItIsAskedForCachedIndexThen4IsReturned) {
    GmmHelper gmmHelper(*platformDevices);
    gmmHelper.setSimplifiedMocsTableUsage(true);
    auto index = gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto expectedIndex = GmmHelper::cacheEnabledIndex;
    EXPECT_EQ(expectedIndex, index);
}

TEST(GmmHelperTest, whenGmmHelperIsInitializedThenClientContextIsSet) {
    ASSERT_NE(nullptr, GmmHelper::getClientContext());
    EXPECT_NE(nullptr, GmmHelper::getClientContext()->getHandle());
}

} // namespace NEO
