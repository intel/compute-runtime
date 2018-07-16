/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "gmm_client_context.h"
#include "gtest/gtest.h"
#include "igfxfmid.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_gmm.h"

using namespace ::testing;

namespace OCLRT {
class GmmTests : public ::testing::Test {
    void SetUp() override {
        executionEnvironment.initGmm(*platformDevices);
    }
    ExecutionEnvironment executionEnvironment;
};

TEST_F(GmmTests, resourceCreation) {
    std::unique_ptr<MemoryManager> mm(new OsAgnosticMemoryManager);
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);
    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096, false));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer(1);
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, resourceCreationUncacheable) {
    std::unique_ptr<MemoryManager> mm(new OsAgnosticMemoryManager);
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
    std::unique_ptr<MemoryManager> mm(new OsAgnosticMemoryManager);
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    std::unique_ptr<Gmm> gmm(new Gmm(pSysMem, 4096, false));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, GivenBufferSizeLargerThenMaxPitchWhenAskedForGmmCreationThenGMMResourceIsCreatedWithNoRestrictionsFlag) {
    auto maxSize = GmmHelper::maxPossiblePitch;

    MemoryManager *mm = new OsAgnosticMemoryManager;
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

    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_EQ(0u, imgInfo.size);
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
    if (hwinfo->pPlatform->eRenderCoreFamily == IGFX_GEN8_CORE) {
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
    EXPECT_EQ(queryGmm->resourceParams.BaseWidth, 17u);
    EXPECT_EQ(queryGmm->resourceParams.BaseHeight, 17u);
    EXPECT_EQ(queryGmm->resourceParams.Depth, 17u);
    EXPECT_EQ(queryGmm->resourceParams.ArraySize, 1u);
    EXPECT_EQ(queryGmm->resourceParams.Flags.Wa.__ForceOtherHVALIGN4, 1u);
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

TEST_F(GmmTests, setTilingOnImgQuery) {
    cl_image_desc imgDesc{};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imgDesc.image_width = 17;
    imgDesc.image_height = 17;
    imgDesc.image_depth = 17;

    bool defaultTiling = DebugManager.flags.ForceLinearImages.get();

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_EQ(queryGmm->resourceParams.Flags.Info.Linear, 1u); // always
    if (DebugManager.flags.ForceLinearImages.get()) {
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.TiledY, 0u);
    } else {
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.TiledY, 1u);
    }

    DebugManager.flags.ForceLinearImages.set(!defaultTiling);

    delete queryGmm.get();
    queryGmm.release();
    queryGmm = MockGmm::queryImgParams(imgInfo);

    EXPECT_EQ(queryGmm->resourceParams.Flags.Info.Linear, 1u); // always
    if (DebugManager.flags.ForceLinearImages.get()) {
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.TiledY, 0u);
    } else {
        EXPECT_EQ(queryGmm->resourceParams.Flags.Info.TiledY, 1u);
    }

    DebugManager.flags.ForceLinearImages.set(defaultTiling);
}

TEST_F(GmmTests, givenZeroRowPitchWhenQueryImgFromBufferParamsThenCalculate) {
    GraphicsAllocation bufferAllocation(nullptr, 4096);

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
    GraphicsAllocation bufferAllocation(nullptr, 4096);

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

TEST_F(GmmTests, returnRenderAlignment) {
    uint32_t tileModes[4][2] = {{0, 2}, {1, 3}, {2, 1}, {3, 0}}; // {given, expected}
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_EQ(GmmHelper::getRenderTileMode(tileModes[i][0]), tileModes[i][1]);
    }
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

class MockGmmHelper : public GmmHelper {
  public:
    using GmmHelper::destroyContext;
    using GmmHelper::initContext;
    MockGmmHelper(const HardwareInfo *hwInfo) : GmmHelper(hwInfo) {}
};

TEST(GmmTest, whenContextIsInitializedMultipleTimesThenDontOverride) {
    const HardwareInfo *hwinfo = *platformDevices;
    auto gmmHelper = MockGmmHelper(hwinfo);
    auto currentClientContext = GmmHelper::gmmClientContext;
    auto currentClientContextHandle = GmmHelper::gmmClientContext->getHandle();
    gmmHelper.initContext(hwinfo->pPlatform, hwinfo->pSkuTable, hwinfo->pWaTable, hwinfo->pSysInfo);

    EXPECT_EQ(currentClientContext, GmmHelper::gmmClientContext);
    EXPECT_EQ(currentClientContextHandle, GmmHelper::gmmClientContext->getHandle());
}

TEST(GmmTest, whenContextIsDestroyedMultimpleTimesThenDontCrash) {
    const HardwareInfo *hwinfo = *platformDevices;
    auto gmmHelper = MockGmmHelper(hwinfo);
    gmmHelper.destroyContext();
}

TEST(GmmTest, givenHwInfoWhenDeviceIsCreatedTheSetThisHwInfoToGmmHelper) {
    HardwareInfo localHwInfo = **platformDevices;

    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    EXPECT_EQ(&localHwInfo, GmmHelper::hwInfo);
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
    gmmParams.BaseWidth = 1;
    gmmParams.BaseHeight = 1;
    gmmParams.Depth = 1;
    gmmParams.Flags.Info.Linear = 1;
    gmmParams.Flags.Info.Cacheable = 1;
    gmmParams.Flags.Gpu.Texture = 1;
    gmmParams.Usage = GMM_RESOURCE_USAGE_OCL_BUFFER;
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.initGmm(*platformDevices);
    MyMockResourecInfo myMockResourceInfo1(&gmmParams);
    EXPECT_NE(nullptr, myMockResourceInfo1.resourceInfo.get());

    MyMockResourecInfo myMockResourceInfo2(myMockResourceInfo1.resourceInfo.get());
    EXPECT_NE(nullptr, myMockResourceInfo2.resourceInfo.get());

    EXPECT_NE(myMockResourceInfo1.resourceInfo.get(), myMockResourceInfo2.resourceInfo.get());
}

TEST(GmmSimplifiedCacheSelectionPolicy, givenGmmInSimplifiedCacheSelectionPolicyWhenItIsAskedForUncachedIndexThen0IsReturned) {
    GmmHelper::useSimplifiedMocsTable = true;
    auto index = GmmHelper::getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED);
    auto expectedIndex = GmmHelper::cacheDisabledIndex;
    EXPECT_EQ(expectedIndex, index);
    GmmHelper::useSimplifiedMocsTable = false;
}

TEST(GmmSimplifiedCacheSelectionPolicy, givenGmmInSimplifiedCacheSelectionPolicyWhenItIsAskedForCachedIndexThen4IsReturned) {
    GmmHelper::useSimplifiedMocsTable = true;
    auto index = GmmHelper::getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    auto expectedIndex = GmmHelper::cacheEnabledIndex;
    EXPECT_EQ(expectedIndex, index);
    GmmHelper::useSimplifiedMocsTable = false;
}

} // namespace OCLRT
