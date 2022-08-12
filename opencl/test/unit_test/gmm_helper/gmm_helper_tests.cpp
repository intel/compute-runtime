/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/gmm_types_converter.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "GL/gl.h"
#include "GL/glext.h"
#include "igfxfmid.h"

using MockExecutionEnvironmentGmmFixtureTest = Test<NEO::MockExecutionEnvironmentGmmFixture>;

using namespace ::testing;

namespace NEO {

extern GMM_INIT_IN_ARGS passedInputArgs;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;

struct GmmTests : public MockExecutionEnvironmentGmmFixtureTest {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        localPlatformDevice = rootDeviceEnvironment->getMutableHardwareInfo();
    }
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    HardwareInfo *localPlatformDevice = nullptr;
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

TEST_F(GmmTests, WhenGmmIsCreatedThenAllResourceAreCreated) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer();
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, GivenUncacheableWhenGmmIsCreatedThenAllResourceAreCreated) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC, false, {}, true));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer();
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC, gmm->resourceParams.Usage);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmTests, givenHostPointerWithHighestBitSetWhenGmmIsCreatedThenItHasTheSameAddress) {
    uintptr_t addressWithHighestBitSet = 0xffff0000;
    auto address = reinterpret_cast<void *>(addressWithHighestBitSet);
    auto expectedAddress = castToUint64(address);

    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), address, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    EXPECT_EQ(gmm->resourceParams.pExistingSysMem, expectedAddress);
}

TEST_F(GmmTests, GivenBufferSizeLargerThenMaxPitchWhenAskedForGmmCreationThenGmmResourceIsCreatedWithNoRestrictionsFlag) {
    auto maxSize = static_cast<size_t>(GmmHelper::maxPossiblePitch);

    MemoryManager *mm = new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment);
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    auto gmmRes = new Gmm(getGmmHelper(), pSysMem, maxSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);

    ASSERT_TRUE(gmmRes->gmmResourceInfo.get() != nullptr); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    EXPECT_EQ(gmmRes->resourceParams.Flags.Gpu.NoRestriction, 1u);

    mm->freeSystemMemory(pSysMem);
    delete gmmRes;
    delete mm;
}

TEST_F(GmmTests, givenGmmCreatedFromExistingGmmThenHelperDoesNotReleaseParentGmm) {
    auto size = 4096u;
    void *incomingPtr = (void *)0x1000;

    auto gmmRes = new Gmm(getGmmHelper(), incomingPtr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    auto gmmRes2 = new Gmm(getGmmHelper(), gmmRes->gmmResourceInfo->peekGmmResourceInfo());
    auto gmmRes3 = new Gmm(getGmmHelper(), gmmRes2->gmmResourceInfo->peekGmmResourceInfo(), false);

    // copy is being made
    EXPECT_NE(gmmRes2->gmmResourceInfo->peekHandle(), gmmRes->gmmResourceInfo->peekGmmResourceInfo());
    EXPECT_NE(gmmRes3->gmmResourceInfo->peekHandle(), gmmRes2->gmmResourceInfo->peekGmmResourceInfo());

    auto allocationSize = gmmRes->gmmResourceInfo->getSizeAllocation();
    EXPECT_NE(0u, allocationSize);
    EXPECT_EQ(allocationSize, gmmRes2->gmmResourceInfo->getSizeAllocation());
    EXPECT_EQ(allocationSize, gmmRes3->gmmResourceInfo->getSizeAllocation());

    // now delete parent GMM and query child, this shouldn't fail
    delete gmmRes;
    EXPECT_EQ(allocationSize, gmmRes2->gmmResourceInfo->getSizeAllocation());

    delete gmmRes2;
    EXPECT_EQ(allocationSize, gmmRes3->gmmResourceInfo->getSizeAllocation());

    delete gmmRes3;
}

TEST_F(GmmTests, GivenInvalidImageSizeWhenQueryingImgParamsThenImageInfoReturnsSizeZero) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image1D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

    EXPECT_EQ(imgInfo.size, 0u);
}

TEST_F(GmmTests, GivenInvalidImageTypeWhenQueryingImgParamsThenExceptionIsThrown) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 10;
    imgDesc.imageType = ImageType::Invalid;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    EXPECT_THROW(MockGmm::queryImgParams(getGmmHelper(), imgInfo, false), std::exception);
}

TEST_F(GmmTests, WhenQueryingImgParamsThenCorrectValuesAreReturned) {
    const HardwareInfo *hwinfo = defaultHwInfo.get();
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image3D;
    imgDesc.imageWidth = 17;
    imgDesc.imageHeight = 17;
    imgDesc.imageDepth = 17;
    size_t pixelSize = 4;
    size_t minSize = imgDesc.imageWidth * imgDesc.imageHeight * imgDesc.imageDepth * pixelSize;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

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
    Gmm gmm(getGmmHelper(), dummyPtr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    EXPECT_EQ(static_cast<uint64_t>(allocationSize), gmm.resourceParams.BaseWidth64);
}

TEST_F(GmmTests, givenNullptrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = nullptr;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    EXPECT_EQ(gmm->resourceParams.NoGfxMemory, 1u);
}

HWTEST_F(GmmTests, givenGmmWithForceLocalMemThenNonLocalIsSetToFalse) {
    void *pSysMem = nullptr;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    EXPECT_EQ(gmm->resourceParams.Flags.Info.NonLocalOnly, 0u);
}

TEST_F(GmmTests, givenPtrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = reinterpret_cast<void *>(0x1111);
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    EXPECT_EQ(gmm->resourceParams.NoGfxMemory, 0u);
}

TEST_F(GmmTests, given2DimageFromBufferParametersWhenGmmResourceIsCreatedThenItHasDesiredPitchAndSize) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 329;
    imgDesc.imageHeight = 349;
    imgDesc.imageDepth = 1;
    imgDesc.imageRowPitch = 5312;
    imgDesc.fromParent = true;

    SurfaceFormatInfo surfaceFormat = {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, (GFX3DSTATE_SURFACEFORMAT)0, 0, 4, 4, 16};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);
    auto renderSize = queryGmm->gmmResourceInfo->getSizeAllocation();

    size_t expectedSize = imgDesc.imageRowPitch * imgDesc.imageHeight;
    EXPECT_GE(renderSize, expectedSize);
    EXPECT_EQ(imgDesc.imageRowPitch, queryGmm->gmmResourceInfo->getRenderPitch());
}

TEST_F(GmmTests, givenPlanarFormatsWhenQueryingImageParamsThenUvOffsetIsQueried) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageHeight = 4;
    imgDesc.imageWidth = 4;
    imgDesc.imageDepth = 1;

    SurfaceFormatInfo surfaceFormatNV12 = {GMM_FORMAT_NV12, GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1};
    SurfaceFormatInfo surfaceFormatP010 = {GMM_FORMAT_P010, GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 2, 2};

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormatNV12);
    imgInfo.yOffsetForUVPlane = 0;
    MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

    EXPECT_NE(0u, imgInfo.yOffsetForUVPlane);

    imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormatP010);
    imgInfo.yOffsetForUVPlane = 0;

    MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);
    EXPECT_NE(0u, imgInfo.yOffsetForUVPlane);
}

TEST_F(GmmTests, givenTilingModeSetToTileYWhenHwSupportsTilingThenTileYFlagIsSet) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 4;
    imgDesc.imageHeight = 4;
    imgDesc.imageDepth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.linearStorage = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, StorageInfo{}, false);

    EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 0u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
}

TEST_F(GmmTests, givenTilingModeSetToNonTiledWhenCreatingGmmThenLinearFlagIsSet) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 4;
    imgDesc.imageHeight = 4;
    imgDesc.imageDepth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.linearStorage = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, StorageInfo{}, false);

    EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 1u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
}

TEST_F(GmmTests, givenZeroRowPitchWhenQueryImgFromBufferParamsThenCalculate) {
    MockGraphicsAllocation bufferAllocation(nullptr, 4096);

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 5;
    imgDesc.imageHeight = 5;
    imgDesc.imageRowPitch = 0;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    size_t expectedRowPitch = imgDesc.imageWidth * imgInfo.surfaceFormat->ImageElementSizeInBytes;
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &bufferAllocation);

    EXPECT_EQ(imgInfo.rowPitch, expectedRowPitch);
}

TEST_F(GmmTests, givenNonZeroRowPitchWhenQueryImgFromBufferParamsThenUseUserValue) {
    MockGraphicsAllocation bufferAllocation(nullptr, 4096);

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 5;
    imgDesc.imageHeight = 5;
    imgDesc.imageRowPitch = 123;
    size_t expectedRowPitch = imgDesc.imageRowPitch;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    GmmTypesConverter::queryImgFromBufferParams(imgInfo, &bufferAllocation);

    EXPECT_EQ(imgInfo.rowPitch, expectedRowPitch);
}

using GmmCanonizeTests = GmmTests;

TEST_F(GmmCanonizeTests, WhenCanonizingThenCorrectAddressIsReturned) {
    auto hwInfo = *defaultHwInfo;

    // 48 bit - canonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); // 0x0000FFFFFFFFFFFF;
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    uint64_t testAddr1 = 0x7777777777777777;
    uint64_t goodAddr1 = 0x0000777777777777;
    EXPECT_EQ(gmmHelper->canonize(testAddr1), goodAddr1);

    uint64_t testAddr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t goodAddr2 = 0xFFFFFFFFFFFFFFFF;
    EXPECT_EQ(gmmHelper->canonize(testAddr2), goodAddr2);

    // 36 bit - also canonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36); // 0x0000000FFFFFFFFF;
    gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    EXPECT_EQ(gmmHelper->canonize(testAddr1), goodAddr1);
    EXPECT_EQ(gmmHelper->canonize(testAddr2), goodAddr2);
}

TEST_F(GmmCanonizeTests, WhenDecanonizingThenCorrectAddressIsReturned) {
    auto hwInfo = *defaultHwInfo;

    // 48 bit - decanonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); // 0x0000FFFFFFFFFFFF;
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    uint64_t testAddr1 = 0x7777777777777777;
    uint64_t goodAddr1 = 0x0000777777777777;
    EXPECT_EQ(gmmHelper->decanonize(testAddr1), goodAddr1);

    uint64_t testAddr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t goodAddr2 = 0x0000FFFFFFFFFFFF;
    EXPECT_EQ(gmmHelper->decanonize(testAddr2), goodAddr2);

    // 36 bit - also decanonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36); // 0x0000000FFFFFFFFF;
    gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    EXPECT_EQ(gmmHelper->decanonize(testAddr1), goodAddr1);
    EXPECT_EQ(gmmHelper->decanonize(testAddr2), goodAddr2);
}

TEST_F(GmmCanonizeTests, WhenCheckingIsValidCanonicalGpuAddressThenOnlyValidAddressesReturnTrue) {
    auto hwInfo = *defaultHwInfo;

    // 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); // 0x0000FFFFFFFFFFFF;
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    uint64_t testAddr1 = 0x0000400000000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr1));

    uint64_t testAddr2 = 0x00000f5670000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr2));

    uint64_t testAddr3 = 0x0000800000000000;
    EXPECT_FALSE(gmmHelper->isValidCanonicalGpuAddress(testAddr3));

    uint64_t testAddr4 = 0xff00ffff00000000;
    EXPECT_FALSE(gmmHelper->isValidCanonicalGpuAddress(testAddr4));

    // 36 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36); // 0x0000000FFFFFFFFF;
    gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    uint64_t testAddr5 = 0x0000000400000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr5));

    uint64_t testAddr6 = 0x00000004ff000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr6));

    uint64_t testAddr7 = 0x0000000800000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr7));

    uint64_t testAddr8 = 0xff00000400000000;
    EXPECT_FALSE(gmmHelper->isValidCanonicalGpuAddress(testAddr8));

    // 57 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(57); // 0x01FFFFFFFFFFFFFFF;
    gmmHelper = std::make_unique<GmmHelper>(nullptr, &hwInfo);

    uint64_t testAddr9 = 0x0080000000000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr9));

    uint64_t testAddr10 = 0x00000004ff000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr10));

    uint64_t testAddr11 = 0x0000000800000000;
    EXPECT_TRUE(gmmHelper->isValidCanonicalGpuAddress(testAddr11));

    uint64_t testAddr12 = 0xfe00fff400000000;
    EXPECT_FALSE(gmmHelper->isValidCanonicalGpuAddress(testAddr12));

    uint64_t testAddr13 = 0xfe008ff400000000;
    EXPECT_FALSE(gmmHelper->isValidCanonicalGpuAddress(testAddr13));
}

TEST_F(GmmTests, givenMipmapedInputWhenAskedForHalingThenNonDefaultValueIsReturned) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2D;
    imgDesc.imageWidth = 60;
    imgDesc.imageHeight = 40;
    imgDesc.imageDepth = 1;
    int mipLevel = 5;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, mipLevel, nullptr);

    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

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
        gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 4, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, true, info, true);
        flags = gmm->gmmResourceInfo->getResourceFlags();
        flags->Gpu.CCS = true;
        flags->Gpu.UnifiedAuxSurface = true;
    }
    std::unique_ptr<Gmm> gmm;
    GMM_RESOURCE_FLAG *flags;
};

TEST_F(GmmMediaCompressedTests, givenMediaCompressedThenUnifiedAuxTranslationCapableIsTrue) {
    flags->Info.MediaCompressed = true;
    flags->Info.RenderCompressed = false;

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST_F(GmmMediaCompressedTests, givenRenderCompressedThenUnifiedAuxTranslationCapableIsTrue) {
    flags->Info.MediaCompressed = false;
    flags->Info.RenderCompressed = true;

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST_F(GmmMediaCompressedTests, givenMediaAndRenderCompressedThenUnifiedAuxTranslationCapableThrowsException) {
    flags->Info.MediaCompressed = true;
    flags->Info.RenderCompressed = true;

    EXPECT_THROW(gmm->unifiedAuxTranslationCapable(), std::exception);
}

TEST_F(GmmMediaCompressedTests, givenNotMediaAndNotRenderCompressedThenUnifiedAuxTranslationCapableIsFalse) {
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

TEST_F(GmmTests, WhenConvertingPlanesThenCorrectPlaneIsReturned) {
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
    updateImgInfo.plane = GMM_YUV_PLANE::GMM_PLANE_U;

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

    if (imgDesc.imageType == ImageType::Image2D || imgDesc.imageType == ImageType::Image2DArray || imgDesc.imageType == ImageType::Image3D) {
        imgDesc.imageHeight = 40;
    }
    if (imgDesc.imageType == ImageType::Image3D) {
        imgDesc.imageDepth = 5;
        expectCalls = 2u;
        expectedReqInfo[0].Slice = 1;
    }

    if (imgDesc.imageType == ImageType::Image2DArray ||
        imgDesc.imageType == ImageType::Image1DArray) {
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

    queryGmm->updateImgInfoAndDesc(updateImgInfo, arrayIndex);
    EXPECT_EQ(expectCalls, mockResInfo->getOffsetCalled);

    EXPECT_EQ(imgDesc.imageWidth, updateImgInfo.imgDesc.imageWidth);
    EXPECT_EQ(imgDesc.imageHeight, updateImgInfo.imgDesc.imageHeight);
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
}

TEST_F(GmmImgTest, givenImgInfoWhenUpdatingOffsetsThenGmmIsCalledToGetOffsets) {
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

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image2DArray;
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
    EXPECT_EQ(mockGmmResourceInfo->gmmGetOffsetOutput.Offset, imgInfo.offset);
    const auto expectedXOffset = mockGmmResourceInfo->gmmGetOffsetOutput.XOffset / (mockGmmResourceInfo->gmmGetBitsPerPixelOutput / 8);
    EXPECT_EQ(expectedXOffset, imgInfo.xOffset);
    EXPECT_EQ(mockGmmResourceInfo->gmmGetOffsetOutput.YOffset, imgInfo.yOffset);
}

TEST_F(GmmTests, GivenPlaneWhenCopyingResourceBltThenResourceIsCopiedCorrectly) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image3D;
    imgDesc.imageWidth = 17;
    imgDesc.imageHeight = 17;
    imgDesc.imageDepth = 17;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto gmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);
    auto mockResInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

    GMM_RES_COPY_BLT &requestedCpuBlt = mockResInfo->requestedResCopyBlt;
    mockResInfo->cpuBltCalled = 0u;
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

    // plane Y
    auto retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_Y);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(1u, mockResInfo->cpuBltCalled);

    // no-plane
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::NO_PLANE);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(2u, mockResInfo->cpuBltCalled);

    // plane UV
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_UV);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(3u, mockResInfo->cpuBltCalled);

    // plane V
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_V);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(4u, mockResInfo->cpuBltCalled);

    // plane U
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u + height * pitch / 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::PLANE_U);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(5u, mockResInfo->cpuBltCalled);
}

TEST_F(GmmTests, givenAllValidFlagsWhenAskedForUnifiedAuxTranslationCapabilityThenReturnTrue) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

    mockResource->setUnifiedAuxTranslationCapable();
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Info.RenderCompressed);

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST_F(GmmTests, givenAlignmentValueWhenConstructingGmmThenSetAlignmentInResourceCreateObject) {
    const uint32_t alignment = 8096;
    Gmm gmm{getGmmHelper(), nullptr, 1, alignment, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true};
    EXPECT_EQ(alignment, gmm.resourceParams.BaseAlignment);
}

TEST_F(GmmTests, givenInvalidFlagsSetWhenAskedForUnifiedAuxTranslationCapabilityThenReturnFalse) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
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

TEST_F(GmmTests, whenLargePagesAreImplicitlyAllowedThenEnableOptimizationPadding) {
    size_t allocationSize = 128;
    Gmm gmm(getGmmHelper(), nullptr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    EXPECT_FALSE(gmm.resourceParams.Flags.Info.NoOptimizationPadding);
}

TEST_F(GmmTests, whenLargePagesAreExplicitlyAllowedAndUserPtrIsNullThenAllowOptimizationPadding) {
    size_t allocationSize = 128;
    bool allowLargePages = true;
    Gmm gmm(getGmmHelper(), nullptr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, allowLargePages);
    EXPECT_FALSE(gmm.resourceParams.Flags.Info.NoOptimizationPadding);
}

TEST_F(GmmTests, whenLargePagesAreExplicitlyDisallowedButUserPtrIsNotNullThenAllowOptimizationPadding) {
    const void *dummyPtr = reinterpret_cast<void *>(0x123);
    size_t allocationSize = 128;
    bool allowLargePages = false;
    Gmm gmm(getGmmHelper(), dummyPtr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, allowLargePages);
    EXPECT_FALSE(gmm.resourceParams.Flags.Info.NoOptimizationPadding);
}

TEST_F(GmmTests, whenLargePagesAreExplicitlyDisallowedAndUserPtrIsNullThenDisableOptimizationPadding) {
    size_t allocationSize = 128;
    bool allowLargePages = false;
    Gmm gmm(getGmmHelper(), nullptr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, allowLargePages);
    EXPECT_TRUE(gmm.resourceParams.Flags.Info.NoOptimizationPadding);
}

TEST_F(GmmTests, givenSizeIsMisallignedTo64kbWhenForceDisablingLargePagesThenSizeIsPreserved) {
    const void *dummyPtr = reinterpret_cast<void *>(0x123);
    size_t allocationSize = 256U;
    bool allowLargePages = false;
    Gmm gmm(getGmmHelper(), dummyPtr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, allowLargePages);
    EXPECT_EQ(allocationSize, gmm.resourceParams.BaseWidth64);
}

TEST_F(GmmTests, givenSizeIsAllignedTo64kbWhenForceDisablingLargePagesThenSizeIsAlteredToBreak64kbAlignment) {
    size_t allocationSize = MemoryConstants::pageSize64k;
    bool allowLargePages = false;
    Gmm gmm(getGmmHelper(), nullptr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, allowLargePages);
    EXPECT_EQ(allocationSize + MemoryConstants::pageSize, gmm.resourceParams.BaseWidth64);
}

TEST(GmmTest, givenHwInfoWhenDeviceIsCreatedThenSetThisHwInfoToGmmHelper) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_EQ(&device->getHardwareInfo(), device->getGmmHelper()->getHardwareInfo());
}

TEST(GmmTest, givenAllocationTypeWhenGettingUsageTypeThenReturnCorrectValue) {
    const auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::COUNT); i++) {
        auto allocationType = static_cast<AllocationType>(i);

        for (auto forceUncached : {true, false}) {
            auto usage = CacheSettingsHelper::getGmmUsageType(allocationType, forceUncached, *defaultHwInfo);
            auto expectedUsage = GMM_RESOURCE_USAGE_UNKNOWN;

            switch (allocationType) {
            case AllocationType::CONSTANT_SURFACE:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED : GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
                break;
            case AllocationType::IMAGE:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED : GMM_RESOURCE_USAGE_OCL_IMAGE;
                break;
            case AllocationType::PREEMPTION:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            case AllocationType::INTERNAL_HEAP:
            case AllocationType::LINEAR_STREAM:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED : GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
                break;
            case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
            case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
                expectedUsage = (forceUncached || hwInfoConfig->isDcFlushAllowed()) ? GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED
                                                                                    : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            default:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            }

            EXPECT_EQ(expectedUsage, usage);
        }
    }
}

TEST(GmmTest, givenForceAllResourcesUncachedFlagSetWhenGettingUsageTypeThenReturnUncached) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceAllResourcesUncached.set(true);

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::COUNT); i++) {
        auto allocationType = static_cast<AllocationType>(i);

        auto usage = CacheSettingsHelper::getGmmUsageType(allocationType, false, *defaultHwInfo);
        auto expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;

        if (allocationType == AllocationType::PREEMPTION) {
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;

        } else if ((allocationType == AllocationType::INTERNAL_HEAP) || (allocationType == AllocationType::LINEAR_STREAM)) {
            expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
        }

        EXPECT_EQ(expectedUsage, usage);
    }
}

TEST(GmmTest, givenUsageTypeWhenAskingIfUncachableThenReturnCorrectValue) {
    for (GMM_RESOURCE_USAGE_TYPE_ENUM usage : {GMM_RESOURCE_USAGE_OCL_IMAGE,
                                               GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER,
                                               GMM_RESOURCE_USAGE_OCL_BUFFER_CONST,
                                               GMM_RESOURCE_USAGE_OCL_BUFFER,
                                               GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC,
                                               GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED,
                                               GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED}) {

        if (usage == GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC ||
            usage == GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED ||
            usage == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) {
            EXPECT_TRUE(CacheSettingsHelper::isUncachedType(usage));
        } else {
            EXPECT_FALSE(CacheSettingsHelper::isUncachedType(usage));
        }
    }
}

TEST(GmmTest, givenInternalHeapOrLinearStreamWhenDebugFlagIsSetThenReturnUncachedType) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisableCachingForHeaps.set(true);

    auto usage = CacheSettingsHelper::getGmmUsageType(AllocationType::INTERNAL_HEAP, false, *defaultHwInfo);
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED, usage);

    usage = CacheSettingsHelper::getGmmUsageType(AllocationType::LINEAR_STREAM, false, *defaultHwInfo);
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED, usage);
}

TEST(GmmTest, givenConstSurfaceWhenDebugFlagIsSetThenReturnUncachedType) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(false);

    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED,
              CacheSettingsHelper::getGmmUsageType(AllocationType::CONSTANT_SURFACE, false, *defaultHwInfo));
}

TEST(GmmTest, givenUncachedDebugFlagMaskSetWhenAskingForUsageTypeThenReturnUncached) {
    DebugManagerStateRestore restore;

    constexpr int64_t bufferMask = 1 << (static_cast<int64_t>(AllocationType::BUFFER) - 1);
    constexpr int64_t imageMask = 1 << (static_cast<int64_t>(AllocationType::IMAGE) - 1);

    DebugManager.flags.ForceUncachedGmmUsageType.set(bufferMask | imageMask);

    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED,
              CacheSettingsHelper::getGmmUsageType(AllocationType::BUFFER, false, *defaultHwInfo));

    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED,
              CacheSettingsHelper::getGmmUsageType(AllocationType::IMAGE, false, *defaultHwInfo));

    EXPECT_NE(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED,
              CacheSettingsHelper::getGmmUsageType(AllocationType::BUFFER_HOST_MEMORY, false, *defaultHwInfo));
}

TEST(GmmTest, givenAllocationForStatefulAccessWhenDebugFlagIsSetThenReturnUncachedType) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisableCachingForStatefulBufferAccess.set(true);

    for (auto allocType : {AllocationType::BUFFER,
                           AllocationType::BUFFER_HOST_MEMORY,
                           AllocationType::EXTERNAL_HOST_PTR,
                           AllocationType::FILL_PATTERN,
                           AllocationType::INTERNAL_HOST_MEMORY,
                           AllocationType::MAP_ALLOCATION,
                           AllocationType::SHARED_BUFFER,
                           AllocationType::SVM_CPU,
                           AllocationType::SVM_GPU,
                           AllocationType::SVM_ZERO_COPY,
                           AllocationType::UNIFIED_SHARED_MEMORY}) {

        EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED, CacheSettingsHelper::getGmmUsageType(allocType, false, *defaultHwInfo));
    }
}

TEST_F(GmmTests, whenGmmIsCreatedAndForceAllResourcesUncachedIsSetThenResourceUsageIsSetToUncachedSurface) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceAllResourcesUncached = true;

    auto size = 4096u;
    void *incomingPtr = (void *)0x1000;
    auto gmm1 = std::make_unique<Gmm>(getGmmHelper(), incomingPtr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);
    EXPECT_EQ(GMM_RESOURCE_USAGE_SURFACE_UNCACHED, gmm1->resourceParams.Usage);

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image1D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto gmm2 = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);
    EXPECT_EQ(GMM_RESOURCE_USAGE_SURFACE_UNCACHED, gmm2->resourceParams.Usage);

    auto gmm3 = std::make_unique<Gmm>(getGmmHelper(), gmm1->gmmResourceInfo->peekGmmResourceInfo());
    EXPECT_EQ(GMM_RESOURCE_USAGE_SURFACE_UNCACHED, gmm3->resourceParams.Usage);
}

TEST_F(GmmTests, whenResourceIsCreatedThenHandleItsOwnership) {
    struct MyMockResourecInfo : public GmmResourceInfo {
        using GmmResourceInfo::resourceInfo;

        MyMockResourecInfo(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *inputParams) : GmmResourceInfo(clientContext, inputParams){};
        MyMockResourecInfo(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) : GmmResourceInfo(clientContext, inputGmmResourceInfo){};
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

    MyMockResourecInfo myMockResourceInfo1(getGmmClientContext(), &gmmParams);
    EXPECT_NE(nullptr, myMockResourceInfo1.resourceInfo.get());

    MyMockResourecInfo myMockResourceInfo2(getGmmClientContext(), myMockResourceInfo1.resourceInfo.get());
    EXPECT_NE(nullptr, myMockResourceInfo2.resourceInfo.get());

    EXPECT_NE(myMockResourceInfo1.resourceInfo.get(), myMockResourceInfo2.resourceInfo.get());
}

using GmmEnvironmentTest = MockExecutionEnvironmentGmmFixtureTest;

TEST_F(GmmEnvironmentTest, givenGmmWithNotSetMCSInResourceInfoGpuFlagsWhenCallHasMultisampleControlSurfaceThenReturnFalse) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    EXPECT_FALSE(gmm->hasMultisampleControlSurface());
}

TEST_F(GmmEnvironmentTest, givenGmmWithSetMCSInResourceInfoGpuFlagsWhenCallhasMultisampleControlSurfaceThenReturnTrue) {
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResource->setMultisampleControlSurface();
    EXPECT_TRUE(gmm->hasMultisampleControlSurface());
}

TEST_F(GmmEnvironmentTest, whenGmmHelperIsInitializedThenClientContextIsSet) {
    ASSERT_NE(nullptr, getGmmHelper());
    EXPECT_NE(nullptr, getGmmClientContext()->getHandle());
}

struct GmmHelperTests : MockExecutionEnvironmentGmmFixtureTest {
    using MockExecutionEnvironmentGmmFixture::executionEnvironment;
};

TEST_F(GmmHelperTests, givenValidGmmFunctionsWhenCreateGmmHelperWithInitializedOsInterfaceThenProperParametersArePassed) {
    std::unique_ptr<GmmHelper> gmmHelper;
    DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = defaultHwInfo.get();
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

    auto hwInfo = defaultHwInfo.get();
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

TEST(GmmHelperTest, givenGmmHelperAndL3CacheDisabledForDebugThenCorrectMOCSIsReturned) {
    decltype(GmmHelper::createGmmContextWrapperFunc) createGmmContextSave = GmmHelper::createGmmContextWrapperFunc;
    GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<MockGmmClientContext>;

    std::unique_ptr<GmmHelper> gmmHelper;
    auto hwInfo = defaultHwInfo.get();
    gmmHelper.reset(new GmmHelper(nullptr, hwInfo));

    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    EXPECT_EQ(2u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(8u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    EXPECT_EQ(16u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    EXPECT_EQ(32u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));

    gmmHelper->forceAllResourcesUncached();

    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));
    GmmHelper::createGmmContextWrapperFunc = createGmmContextSave;
}

TEST(GmmHelperTest, givenGmmHelperAndForceAllResourcesUncachedDebugVariableSetThenCorrectMOCSIsReturned) {
    decltype(GmmHelper::createGmmContextWrapperFunc) createGmmContextSave = GmmHelper::createGmmContextWrapperFunc;
    GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<MockGmmClientContext>;

    std::unique_ptr<GmmHelper> gmmHelper;
    auto hwInfo = defaultHwInfo.get();
    gmmHelper.reset(new GmmHelper(nullptr, hwInfo));

    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    EXPECT_EQ(2u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(8u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    EXPECT_EQ(16u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    EXPECT_EQ(32u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));

    DebugManagerStateRestore restore;
    DebugManager.flags.ForceAllResourcesUncached.set(true);

    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));
    GmmHelper::createGmmContextWrapperFunc = createGmmContextSave;
}

struct GmmCompressionTests : public MockExecutionEnvironmentGmmFixtureTest {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixtureTest::SetUp();
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        localPlatformDevice = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

        localPlatformDevice->capabilityTable.ftrRenderCompressedImages = true;
        localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;
        localPlatformDevice->featureTable.flags.ftrLocalMemory = true;

        setupImgInfo();
    }

    void setupImgInfo() {
        imgDesc.imageType = ImageType::Image2D;
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

TEST_F(GmmCompressionTests, givenEnabledAndNotPreferredE2ECWhenApplyingForBuffersThenDontSetValidFlags) {
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    gmm->resourceParams = {};

    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;

    gmm->applyAuxFlagsForBuffer(false);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(gmm->isCompressionEnabled);
}

TEST_F(GmmCompressionTests, givenDisabledAndPreferredE2ECWhenApplyingForBuffersThenDontSetValidFlags) {
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    gmm->resourceParams = {};

    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = false;

    gmm->applyAuxFlagsForBuffer(true);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(gmm->isCompressionEnabled);
}

HWTEST_F(GmmCompressionTests, givenAllValidInputsWhenQueryingThenSetAppropriateFlags) {
    EXPECT_TRUE(localPlatformDevice->capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(imgInfo.surfaceFormat->GMMSurfaceFormat != GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12);
    EXPECT_TRUE(imgInfo.plane == GMM_YUV_PLANE_ENUM::GMM_NO_PLANE);

    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    EXPECT_GT(compressionFormat, 0u);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.TiledY);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_TRUE(queryGmm->isCompressionEnabled);
}

TEST_F(GmmCompressionTests, givenAllValidInputsAndNoLocalMemoryRequestWhenQueryingThenRenderCompressionFlagsAreNotSet) {
    EXPECT_TRUE(localPlatformDevice->capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(imgInfo.surfaceFormat->GMMSurfaceFormat != GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12);
    EXPECT_TRUE(imgInfo.plane == GMM_YUV_PLANE_ENUM::GMM_NO_PLANE);

    imgInfo.useLocalMemory = false;

    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    EXPECT_GT(compressionFormat, 0u);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_FALSE(queryGmm->isCompressionEnabled);
}

TEST_F(GmmCompressionTests, givenNotAllowedRenderCompressionWhenQueryingThenSetAppropriateFlags) {
    localPlatformDevice->capabilityTable.ftrRenderCompressedImages = false;
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_FALSE(queryGmm->isCompressionEnabled);
}

HWTEST_F(GmmCompressionTests, givenNotAllowedCompressionAndEnabledDebugFlagWhenQueryingThenSetAppropriateFlags) {
    DebugManagerStateRestore restore;
    DebugManager.flags.RenderCompressedImagesEnabled.set(1);
    localPlatformDevice->capabilityTable.ftrRenderCompressedImages = false;
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_TRUE(queryGmm->isCompressionEnabled);

    DebugManager.flags.RenderCompressedImagesEnabled.set(0);
    localPlatformDevice->capabilityTable.ftrRenderCompressedImages = true;
    queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_FALSE(queryGmm->isCompressionEnabled);
}

TEST_F(GmmCompressionTests, givenNotPreferredCompressionFlagWhenQueryingThenDisallow) {
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

    EXPECT_FALSE(queryGmm->isCompressionEnabled);
}

TEST_F(GmmCompressionTests, givenNV12FormatWhenQueryingThenDisallow) {
    imgInfo.surfaceFormat = &SurfaceFormats::planarYuv()[0].surfaceFormat;
    EXPECT_TRUE(imgInfo.surfaceFormat->GMMSurfaceFormat == GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12);
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    auto resourceFormat = queryGmm->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
    EXPECT_GT(compressionFormat, 0u);

    EXPECT_FALSE(queryGmm->isCompressionEnabled);
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
    EXPECT_FALSE(queryGmm->isCompressionEnabled);

    mockGmmClient->compressionFormatToReturn = validFormat;
    queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    EXPECT_TRUE(queryGmm->isCompressionEnabled);
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
    EXPECT_FALSE(queryGmm->isCompressionEnabled);

    mockGmmClient->compressionFormatToReturn = validFormat;
    queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    EXPECT_TRUE(queryGmm->isCompressionEnabled);
}

TEST_F(GmmCompressionTests, givenPlaneFormatWhenQueryingThenDisallow) {
    GMM_YUV_PLANE gmmPlane[4] = {GMM_YUV_PLANE::GMM_NO_PLANE, GMM_YUV_PLANE::GMM_PLANE_U,
                                 GMM_YUV_PLANE::GMM_PLANE_V, GMM_YUV_PLANE::GMM_PLANE_Y};

    for (auto &plane : gmmPlane) {
        imgInfo.plane = plane;
        auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

        EXPECT_EQ(queryGmm->isCompressionEnabled,
                  plane == GMM_YUV_PLANE::GMM_NO_PLANE);
    }
}

TEST_F(GmmCompressionTests, givenPackedYuvFormatWhenQueryingThenDisallow) {
    for (auto &surfaceFormat : SurfaceFormats::packedYuv()) {
        imgInfo.surfaceFormat = &surfaceFormat.surfaceFormat;
        auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

        EXPECT_FALSE(queryGmm->isCompressionEnabled);
    }
}
HWTEST_F(GmmCompressionTests, whenConstructedWithPreferCompressionFlagThenApplyAuxFlags) {
    Gmm gmm1(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    EXPECT_EQ(0u, gmm1.resourceParams.Flags.Info.RenderCompressed);

    Gmm gmm2(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    EXPECT_EQ(0u, gmm2.resourceParams.Flags.Info.RenderCompressed);

    Gmm gmm3(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, true, {}, true);
    EXPECT_EQ(1u, gmm3.resourceParams.Flags.Info.RenderCompressed);
}

TEST_F(GmmCompressionTests, givenMediaCompressedImageApplyAuxFlagsForImageThenSetFlagsToCompressed) {
    MockGmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm.resourceParams.Flags.Info.MediaCompressed = true;
    gmm.resourceParams.Flags.Info.RenderCompressed = false;
    gmm.setupImageResourceParams(imgInfo, true);

    EXPECT_TRUE(gmm.isCompressionEnabled);
}

TEST_F(GmmCompressionTests, givenRenderCompressedImageApplyAuxFlagsForImageThenSetFlagsToCompressed) {
    MockGmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm.resourceParams.Flags.Info.MediaCompressed = false;
    gmm.resourceParams.Flags.Info.RenderCompressed = true;
    gmm.setupImageResourceParams(imgInfo, true);

    EXPECT_TRUE(gmm.isCompressionEnabled);
}

HWTEST_F(GmmCompressionTests, givenEnabledAndPreferredE2ECWhenApplyingForBuffersThenSetValidFlags) {
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    gmm->resourceParams = {};

    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;

    gmm->applyAuxFlagsForBuffer(true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_TRUE(gmm->isCompressionEnabled);
}

HWTEST_F(GmmCompressionTests, givenDisabledE2ECAndEnabledDebugFlagWhenApplyingForBuffersThenSetValidFlags) {
    DebugManagerStateRestore restore;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    gmm.resourceParams = {};

    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = false;

    gmm.applyAuxFlagsForBuffer(true);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_TRUE(gmm.isCompressionEnabled);

    gmm.resourceParams = {};
    gmm.isCompressionEnabled = false;
    DebugManager.flags.RenderCompressedBuffersEnabled.set(0);
    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;

    gmm.applyAuxFlagsForBuffer(true);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(gmm.isCompressionEnabled);
}

struct GmmLocalMemoryTests : public ::testing::Test, MockExecutionEnvironmentGmmFixture {
    GmmLocalMemoryTests() {
        localPlatformDevice = *defaultHwInfo;
        localPlatformDevice.featureTable.flags.ftrLocalMemory = true;
    }
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&localPlatformDevice);
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

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryWhenUseSystemMemoryIsTrueThenNonLocalOnlyFlagIsSetAndLocalOnlyCleared) {
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
}

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryWhenUsingLocalMemoryAndAllocationIsLockableThenAllFlagsAreCleared) {
    StorageInfo storageInfo{};
    storageInfo.isLockable = true;
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryWhenUsingLocalMemoryFalseAndAllocationIsNotLockableThenNotLockableFlagsIsSetAndLocalAndNonLocalOnlyAreNotSet) {
    StorageInfo storageInfo{};
    storageInfo.isLockable = false;
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenLocalMemoryAndNotLockableAllocationAndStorageInfoWithLocalOnlyRequiredWhenPreparingFlagsForGmmThenNotLockableAndLocalOnlyIsSet) {
    StorageInfo storageInfo{};
    storageInfo.localOnlyRequired = true;
    storageInfo.isLockable = false;
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;

    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenLocalMemoryAndStorageInfoWithLocalOnlyRequiredWhenPreparingFlagsForGmmThenNotLockableAndLocalOnlyAreSet) {
    StorageInfo storageInfo{};
    storageInfo.localOnlyRequired = true;
    storageInfo.isLockable = false;
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;

    DebugManagerStateRestore restorer;

    for (auto csrMode = static_cast<int32_t>(CommandStreamReceiverType::CSR_HW); csrMode < static_cast<int32_t>(CommandStreamReceiverType::CSR_TYPES_NUM); csrMode++) {
        DebugManager.flags.SetCommandStreamReceiver.set(csrMode);
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);
        EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.LocalOnly);
        EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
    }
}

TEST_F(GmmLocalMemoryTests, givenSystemMemoryAndStorageInfoWithLocalOnlyRequiredWhenPreparingFlagsForGmmThenLocalOnlyIsNotSet) {
    StorageInfo storageInfo{};
    storageInfo.localOnlyRequired = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
}

TEST_F(GmmLocalMemoryTests, givenLocalMemoryAndStorageInfoWithoutLocalOnlyRequiredWhenPreparingFlagsForGmmThenLocalOnlyIsNotSet) {
    StorageInfo storageInfo{};
    storageInfo.localOnlyRequired = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
}

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryAndCompressionEnabledWhenUsingLocalMemoryAndAllocationIsNotLockableThenNotLockableAndLocalOnlyFlagsAreSetAndNonLocalOnlyIsNotSet) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);
    StorageInfo storageInfo{};
    storageInfo.isLockable = false;
    storageInfo.systemMemoryPlacement = false;
    storageInfo.memoryBanks.set(1);

    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, true, storageInfo, true);
    EXPECT_TRUE(gmm->isCompressionEnabled);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryWhenUseSystemMemoryIsFalseAndAllocationIsNotLockableThenLocalAndNonLocalOnlyAndNotLockableFlagsAreNotSet) {
    DebugManagerStateRestore restorer;
    for (auto csrMode = static_cast<int32_t>(CommandStreamReceiverType::CSR_HW); csrMode < static_cast<int32_t>(CommandStreamReceiverType::CSR_TYPES_NUM); csrMode++) {
        DebugManager.flags.SetCommandStreamReceiver.set(csrMode);
        StorageInfo storageInfo{};
        storageInfo.memoryBanks.set(1);
        storageInfo.systemMemoryPlacement = false;
        storageInfo.isLockable = false;
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);
        EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
        EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
        EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
    }
}

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
    DebugManager.flags.RenderCompressedImagesEnabled.set(1);
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

    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, storageInfo, true);
    EXPECT_TRUE(gmm->isCompressionEnabled);
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

TEST_F(MultiTileGmmTests, givenMultiTileAllocationWhenGmmIsCreatedWithEmptyMemporyBanksThenMultitileArchIsEnabled) {
    StorageInfo storageInfo;
    storageInfo.memoryBanks = 0;

    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, givenMultiTileAllocationWithoutCloningWhenGmmIsCreatedThenSetMinimumOneTile) {
    StorageInfo storageInfo;
    storageInfo.memoryBanks = 1;
    storageInfo.cloningOfPageTables = false;
    storageInfo.systemMemoryPlacement = false;

    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.GpuVaMappingSet);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.LocalMemPreferredSet);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.LocalMemEligibilitySet);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, givenMultiTileWhenGmmIsCreatedWithNonLocalMemoryThenMultitileArchIsPropertlyFilled) {
    StorageInfo storageInfo;

    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(customTileMask, gmm.resourceParams.MultiTileArch.GpuVaMappingSet);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.LocalMemPreferredSet);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.LocalMemEligibilitySet);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, givenMultiTileWhenGmmIsCreatedWithSpecificMemoryBanksThenMultitileArchIsEnabled) {
    StorageInfo storageInfo;
    storageInfo.systemMemoryPlacement = false;
    storageInfo.memoryBanks = 1u;
    storageInfo.cloningOfPageTables = false;

    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(storageInfo.memoryBanks, gmm.resourceParams.MultiTileArch.LocalMemPreferredSet);
    EXPECT_EQ(storageInfo.memoryBanks, gmm.resourceParams.MultiTileArch.GpuVaMappingSet);
    EXPECT_EQ(storageInfo.memoryBanks, gmm.resourceParams.MultiTileArch.LocalMemEligibilitySet);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, givenMultiTileWhenGmmIsCreatedWithCloningEnabledThenGpuVaMappingDependsOnPageTablesVisibityBitfield) {
    StorageInfo storageInfo;
    storageInfo.memoryBanks = 2u;
    storageInfo.cloningOfPageTables = true;
    storageInfo.systemMemoryPlacement = false;
    storageInfo.pageTablesVisibility = 3u;

    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(storageInfo.memoryBanks, gmm.resourceParams.MultiTileArch.LocalMemPreferredSet);
    EXPECT_EQ(storageInfo.pageTablesVisibility, gmm.resourceParams.MultiTileArch.GpuVaMappingSet);
    EXPECT_EQ(storageInfo.memoryBanks, gmm.resourceParams.MultiTileArch.LocalMemEligibilitySet);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, whenAllocationIsTileInstancedWithoutClonningPageTablesThenResourceParamsHaveTileInstancedEnabled) {
    StorageInfo storageInfo;
    storageInfo.cloningOfPageTables = false;
    storageInfo.tileInstanced = true;
    storageInfo.memoryBanks = 2u;
    storageInfo.systemMemoryPlacement = false;

    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, whenAllocationIsTileInstancedWithClonningPageTablesThenResourceParamsHaveTileInstancedDisabled) {
    StorageInfo storageInfo;
    storageInfo.cloningOfPageTables = true;
    storageInfo.tileInstanced = true;

    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, storageInfo, true);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

} // namespace NEO
