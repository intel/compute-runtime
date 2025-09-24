/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/sku_info/operations/sku_info_transfer.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using MockExecutionEnvironmentGmmFixtureTest = Test<NEO::MockExecutionEnvironmentGmmFixture>;

using namespace ::testing;

using namespace NEO;

namespace NEO {
extern GMM_INIT_IN_ARGS passedInputArgs;
extern GT_SYSTEM_INFO passedGtSystemInfo;
extern SKU_FEATURE_TABLE passedFtrTable;
extern WA_TABLE passedWaTable;
extern bool copyInputArgs;

} // namespace NEO

struct GmmHelperTests : public MockExecutionEnvironmentGmmFixtureTest {
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

TEST_F(GmmHelperTests, WhenGmmIsCreatedThenAllResourceAreCreated) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer();
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmHelperTests, GivenUncacheableWhenGmmIsCreatedThenAllResourceAreCreated) {
    std::unique_ptr<MemoryManager> mm(new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment));
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC, {}, gmmRequirements));

    ASSERT_TRUE(gmm->gmmResourceInfo.get() != nullptr);

    void *pGmmSysMem = gmm->gmmResourceInfo->getSystemMemPointer();
    EXPECT_EQ(gmm->resourceParams.Flags.Gpu.NoRestriction, 0u);
    EXPECT_TRUE(pSysMem == pGmmSysMem);
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC, gmm->resourceParams.Usage);

    mm->freeSystemMemory(pSysMem);
}

TEST_F(GmmHelperTests, givenHostPointerWithHighestBitSetWhenGmmIsCreatedThenItHasTheSameAddress) {
    uintptr_t addressWithHighestBitSet = 0xffff0000;
    auto address = reinterpret_cast<void *>(addressWithHighestBitSet);
    auto expectedAddress = castToUint64(address);

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), address, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    EXPECT_EQ(gmm->resourceParams.pExistingSysMem, expectedAddress);
}

TEST_F(GmmHelperTests, GivenBufferSizeLargerThenMaxPitchWhenAskedForGmmCreationThenGmmResourceIsCreatedWithNoRestrictionsFlag) {
    auto maxSize = static_cast<size_t>(GmmHelper::maxPossiblePitch);

    MemoryManager *mm = new MemoryManagerCreate<OsAgnosticMemoryManager>(false, false, *executionEnvironment);
    void *pSysMem = mm->allocateSystemMemory(4096, 4096);

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmmRes = new Gmm(getGmmHelper(), pSysMem, maxSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);

    ASSERT_TRUE(gmmRes->gmmResourceInfo.get() != nullptr); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    EXPECT_EQ(gmmRes->resourceParams.Flags.Gpu.NoRestriction, 1u);

    mm->freeSystemMemory(pSysMem);
    delete gmmRes;
    delete mm;
}

TEST_F(GmmHelperTests, givenGmmCreatedFromExistingGmmThenHelperDoesNotReleaseParentGmm) {
    auto size = 4096u;
    void *incomingPtr = (void *)0x1000;

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmmRes = new Gmm(getGmmHelper(), incomingPtr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
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

TEST_F(GmmHelperTests, GivenInvalidImageSizeWhenQueryingImgParamsThenImageInfoReturnsSizeZero) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image1D;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

    EXPECT_EQ(imgInfo.size, 0u);
}

TEST_F(GmmHelperTests, GivenInvalidImageTypeWhenQueryingImgParamsThenExceptionIsThrown) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 10;
    imgDesc.imageType = ImageType::invalid;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    EXPECT_THROW(MockGmm::queryImgParams(getGmmHelper(), imgInfo, false), std::exception);
}

TEST_F(GmmHelperTests, WhenQueryingImgParamsThenCorrectValuesAreReturned) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image3D;
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
    EXPECT_GT(imgInfo.qPitch, 0u);

    auto &gfxCoreHelper = this->rootDeviceEnvironment->getHelper<GfxCoreHelper>();

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
    EXPECT_EQ(!!queryGmm->resourceParams.Flags.Wa.__ForceOtherHVALIGN4, gfxCoreHelper.hvAlign4Required());
}

TEST_F(GmmHelperTests, givenWidthWhenCreatingResourceThenSetWidth64Field) {
    const void *dummyPtr = reinterpret_cast<void *>(0x123);
    size_t allocationSize = std::numeric_limits<size_t>::max();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), dummyPtr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_EQ(static_cast<uint64_t>(allocationSize), gmm.resourceParams.BaseWidth64);
}

TEST_F(GmmHelperTests, givenNullptrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = nullptr;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    EXPECT_EQ(gmm->resourceParams.NoGfxMemory, 1u);
}

HWTEST_F(GmmHelperTests, givenGmmWithForceLocalMemThenNonLocalIsSetToFalse) {
    void *pSysMem = nullptr;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    EXPECT_EQ(gmm->resourceParams.Flags.Info.NonLocalOnly, 0u);
}

TEST_F(GmmHelperTests, givenPtrWhenGmmConstructorIsCalledThenNoGfxMemoryIsProperlySet) {
    void *pSysMem = reinterpret_cast<void *>(0x1111);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), pSysMem, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));

    EXPECT_EQ(gmm->resourceParams.NoGfxMemory, 0u);
}

TEST_F(GmmHelperTests, givenPlanarFormatsWhenQueryingImageParamsThenUvOffsetIsQueried) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
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

TEST_F(GmmHelperTests, givenTilingModeSetToTileYWhenHwSupportsTilingThenTileYFlagIsSet) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 4;
    imgDesc.imageHeight = 4;
    imgDesc.imageDepth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.linearStorage = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, StorageInfo{}, false);

    EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 0u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
}

TEST_F(GmmHelperTests, givenTilingModeSetToNonTiledWhenCreatingGmmThenLinearFlagIsSet) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 4;
    imgDesc.imageHeight = 4;
    imgDesc.imageDepth = 1;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    imgInfo.linearStorage = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), imgInfo, StorageInfo{}, false);

    EXPECT_EQ(gmm->resourceParams.Flags.Info.Linear, 1u);
    EXPECT_EQ(gmm->resourceParams.Flags.Info.TiledY, 0u);
}

using GmmCanonizeTests = GmmHelperTests;

TEST_F(GmmCanonizeTests, WhenCanonizingThenCorrectAddressIsReturned) {
    auto &hwInfo = *rootDeviceEnvironment->getMutableHardwareInfo();

    // 48 bit - canonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); // 0x0000FFFFFFFFFFFF;
    rootDeviceEnvironment->gmmHelper.reset();
    rootDeviceEnvironment->initGmm();
    gmmHelper = rootDeviceEnvironment->getGmmHelper();

    uint64_t testAddr1 = 0x7777777777777777;
    uint64_t goodAddr1 = 0x0000777777777777;
    EXPECT_EQ(gmmHelper->canonize(testAddr1), goodAddr1);

    uint64_t testAddr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t goodAddr2 = 0xFFFFFFFFFFFFFFFF;
    EXPECT_EQ(gmmHelper->canonize(testAddr2), goodAddr2);

    // 36 bit - also canonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36); // 0x0000000FFFFFFFFF;
    rootDeviceEnvironment->gmmHelper.reset();
    rootDeviceEnvironment->initGmm();
    gmmHelper = rootDeviceEnvironment->getGmmHelper();

    EXPECT_EQ(gmmHelper->canonize(testAddr1), goodAddr1);
    EXPECT_EQ(gmmHelper->canonize(testAddr2), goodAddr2);
}

TEST_F(GmmCanonizeTests, WhenDecanonizingThenCorrectAddressIsReturned) {
    auto &hwInfo = *rootDeviceEnvironment->getMutableHardwareInfo();

    // 48 bit - decanonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); // 0x0000FFFFFFFFFFFF;
    rootDeviceEnvironment->gmmHelper.reset();
    rootDeviceEnvironment->initGmm();
    gmmHelper = rootDeviceEnvironment->getGmmHelper();

    uint64_t testAddr1 = 0x7777777777777777;
    uint64_t goodAddr1 = 0x0000777777777777;
    EXPECT_EQ(gmmHelper->decanonize(testAddr1), goodAddr1);

    uint64_t testAddr2 = 0x7FFFFFFFFFFFFFFF;
    uint64_t goodAddr2 = 0x0000FFFFFFFFFFFF;
    EXPECT_EQ(gmmHelper->decanonize(testAddr2), goodAddr2);

    // 36 bit - also decanonize to 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(36); // 0x0000000FFFFFFFFF;
    rootDeviceEnvironment->gmmHelper.reset();
    rootDeviceEnvironment->initGmm();
    gmmHelper = rootDeviceEnvironment->getGmmHelper();

    EXPECT_EQ(gmmHelper->decanonize(testAddr1), goodAddr1);
    EXPECT_EQ(gmmHelper->decanonize(testAddr2), goodAddr2);
}

TEST_F(GmmCanonizeTests, WhenCheckingIsValidCanonicalGpuAddressThenOnlyValidAddressesReturnTrue) {
    auto &hwInfo = *rootDeviceEnvironment->getMutableHardwareInfo();

    // 48 bit
    hwInfo.capabilityTable.gpuAddressSpace = maxNBitValue(48); // 0x0000FFFFFFFFFFFF;
    rootDeviceEnvironment->gmmHelper.reset();
    rootDeviceEnvironment->initGmm();
    gmmHelper = rootDeviceEnvironment->getGmmHelper();

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
    rootDeviceEnvironment->gmmHelper.reset();
    rootDeviceEnvironment->initGmm();
    gmmHelper = rootDeviceEnvironment->getGmmHelper();

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
    rootDeviceEnvironment->gmmHelper.reset();
    rootDeviceEnvironment->initGmm();
    gmmHelper = rootDeviceEnvironment->getGmmHelper();

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

TEST_F(GmmHelperTests, givenMipmapedInputWhenAskedForHalingThenNonDefaultValueIsReturned) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = 60;
    imgDesc.imageHeight = 40;
    imgDesc.imageDepth = 1;
    int mipLevel = 5;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, mipLevel, nullptr);

    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

    EXPECT_EQ(static_cast<int>(queryGmm->resourceParams.MaxLod), mipLevel);
}

struct GmmMediaCompressedTests : public GmmHelperTests {
    void SetUp() override {
        GmmHelperTests::SetUp();
        StorageInfo info;
        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = true;
        gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 4, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, info, gmmRequirements);
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

TEST_F(GmmHelperTests, GivenPlaneWhenCopyingResourceBltThenResourceIsCopiedCorrectly) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image3D;
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
    auto retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::planeY);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(1u, mockResInfo->cpuBltCalled);

    // no-plane
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::noPlane);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(2u, mockResInfo->cpuBltCalled);

    // plane UV
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::planeUV);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(3u, mockResInfo->cpuBltCalled);

    // plane V
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::planeV);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(4u, mockResInfo->cpuBltCalled);

    // plane U
    expectedCpuBlt.Sys.pData = ptrOffset(&sys, height * pitch * 2u + height * pitch / 2u);
    expectedCpuBlt.Sys.RowPitch = pitch / 2;
    expectedCpuBlt.Sys.BufferSize = expectedCpuBlt.Sys.RowPitch * height;
    retVal = gmm->resourceCopyBlt(&sys, &gpu, pitch, height, upload, ImagePlane::planeU);
    EXPECT_EQ(1u, retVal);
    EXPECT_TRUE(memcmp(&expectedCpuBlt, &requestedCpuBlt, sizeof(GMM_RES_COPY_BLT)) == 0);
    EXPECT_EQ(5u, mockResInfo->cpuBltCalled);
}

TEST_F(GmmHelperTests, givenAllValidFlagsWhenAskedForUnifiedAuxTranslationCapabilityThenReturnTrue) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());

    mockResource->setUnifiedAuxTranslationCapable();
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, mockResource->mockResourceCreateParams.Flags.Info.RenderCompressed);

    EXPECT_TRUE(gmm->unifiedAuxTranslationCapable());
}

TEST_F(GmmHelperTests, givenDebugFlagSetWhenCreatingResourceThenPrintCompressionParams) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintGmmCompressionParams.set(true);

    StreamCapture capture;
    capture.captureStdout();

    StorageInfo storageInfo;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
    auto &flags = gmm->resourceParams.Flags;

    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    char expectedStr[512] = {};
    snprintf(expectedStr, 512, "\nGmm Resource compression params: \n\tFlags.Gpu.CCS: %u\n\tFlags.Gpu.UnifiedAuxSurface: %u\n\tFlags.Info.RenderCompressed: %u",
             flags.Gpu.CCS, flags.Gpu.UnifiedAuxSurface, flags.Info.RenderCompressed);

    EXPECT_TRUE(hasSubstr(output, std::string(expectedStr)));
}

TEST_F(GmmHelperTests, givenDebugFlagSetWhenCreatingImageResourceThenPrintCompressionParams) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintGmmCompressionParams.set(true);

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image3D;
    imgDesc.imageWidth = 17;
    imgDesc.imageHeight = 17;
    imgDesc.imageDepth = 17;

    StreamCapture capture;
    capture.captureStdout();

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    auto gmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);
    auto &flags = gmm->resourceParams.Flags;

    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    char expectedStr[512] = {};
    snprintf(expectedStr, 512, "\nGmm Resource compression params: \n\tFlags.Gpu.CCS: %u\n\tFlags.Gpu.UnifiedAuxSurface: %u\n\tFlags.Info.RenderCompressed: %u",
             flags.Gpu.CCS, flags.Gpu.UnifiedAuxSurface, flags.Info.RenderCompressed);

    EXPECT_TRUE(hasSubstr(output, std::string(expectedStr)));
}

TEST_F(GmmHelperTests, givenAlignmentValueWhenConstructingGmmThenSetAlignmentInResourceCreateObject) {
    const uint32_t alignment = 8096;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm{getGmmHelper(), nullptr, 1, alignment, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements};
    EXPECT_EQ(alignment, gmm.resourceParams.BaseAlignment);
}

TEST_F(GmmHelperTests, givenInvalidFlagsSetWhenAskedForUnifiedAuxTranslationCapabilityThenReturnFalse) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
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

TEST_F(GmmHelperTests, whenLargePagesAreExplicitlyAllowedAndUserPtrIsNullThenAllowOptimizationPadding) {
    size_t allocationSize = 128;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_FALSE(gmm.resourceParams.Flags.Info.NoOptimizationPadding);
}

TEST_F(GmmHelperTests, whenLargePagesAreExplicitlyDisallowedButUserPtrIsNotNullThenAllowOptimizationPadding) {
    const void *dummyPtr = reinterpret_cast<void *>(0x123);
    size_t allocationSize = 128;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), dummyPtr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_FALSE(gmm.resourceParams.Flags.Info.NoOptimizationPadding);
}

TEST_F(GmmHelperTests, whenLargePagesAreExplicitlyDisallowedAndUserPtrIsNullThenDisableOptimizationPadding) {
    size_t allocationSize = 128;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_TRUE(gmm.resourceParams.Flags.Info.NoOptimizationPadding);
}

TEST_F(GmmHelperTests, givenSizeIsMisallignedTo64kbWhenForceDisablingLargePagesThenSizeIsPreserved) {
    const void *dummyPtr = reinterpret_cast<void *>(0x123);
    size_t allocationSize = 256U;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), dummyPtr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_EQ(allocationSize, gmm.resourceParams.BaseWidth64);
}

TEST_F(GmmHelperTests, givenSizeIsAllignedTo64kbWhenForceDisablingLargePagesThenSizeIsAlteredToBreak64kbAlignment) {
    size_t allocationSize = MemoryConstants::pageSize64k;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = false;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, allocationSize, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_EQ(allocationSize + MemoryConstants::pageSize, gmm.resourceParams.BaseWidth64);
}

TEST(GmmTest, givenHwInfoWhenDeviceIsCreatedThenSetThisHwInfoToGmmHelper) {
    std::unique_ptr<MockDevice> device(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    EXPECT_EQ(&device->getHardwareInfo(), device->getGmmHelper()->getHardwareInfo());
}

TEST(GmmTest, givenAllocationTypeWhenGettingUsageTypeThenReturnCorrectValue) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    HardwareInfo hwInfo;
    hwInfo.capabilityTable.isIntegratedDevice = false;

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto uncachedGmmUsageType = productHelper.isNewCoherencyModelSupported() ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC : GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
        for (auto forceUncached : {true, false}) {
            auto usage = CacheSettingsHelper::getGmmUsageType(allocationType, forceUncached, productHelper, &hwInfo);
            auto expectedUsage = GMM_RESOURCE_USAGE_UNKNOWN;

            switch (allocationType) {
            case AllocationType::constantSurface:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
                break;
            case AllocationType::image:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_IMAGE;
                break;
            case AllocationType::preemption:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            case AllocationType::internalHeap:
            case AllocationType::linearStream:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED : GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
                break;
            case AllocationType::gpuTimestampDeviceBuffer:
            case AllocationType::timestampPacketTagBuffer:
                expectedUsage = (forceUncached || !productHelper.isNonCoherentTimestampsModeEnabled()) ? uncachedGmmUsageType
                                                                                                       : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            case AllocationType::bufferHostMemory:
            case AllocationType::externalHostPtr:
            case AllocationType::fillPattern:
            case AllocationType::internalHostMemory:
            case AllocationType::mapAllocation:
            case AllocationType::svmCpu:
            case AllocationType::svmZeroCopy:
            case AllocationType::tagBuffer:
            case AllocationType::printfSurface:
            case AllocationType::hostFunction:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
                break;
            default:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            }

            EXPECT_EQ(expectedUsage, usage);
        }
    }

    hwInfo.capabilityTable.isIntegratedDevice = true;

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto uncachedGmmUsageType = productHelper.isNewCoherencyModelSupported() ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC : GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
        for (auto forceUncached : {true, false}) {
            auto usage = CacheSettingsHelper::getGmmUsageType(allocationType, forceUncached, productHelper, &hwInfo);
            auto expectedUsage = GMM_RESOURCE_USAGE_UNKNOWN;

            switch (allocationType) {
            case AllocationType::constantSurface:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
                break;
            case AllocationType::image:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_IMAGE;
                break;
            case AllocationType::preemption:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            case AllocationType::internalHeap:
            case AllocationType::linearStream:
                expectedUsage = forceUncached ? GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED : GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
                break;
            case AllocationType::gpuTimestampDeviceBuffer:
            case AllocationType::timestampPacketTagBuffer:
                expectedUsage = (forceUncached || !productHelper.isNonCoherentTimestampsModeEnabled()) ? uncachedGmmUsageType
                                                                                                       : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            case AllocationType::bufferHostMemory:
            case AllocationType::externalHostPtr:
            case AllocationType::fillPattern:
            case AllocationType::internalHostMemory:
            case AllocationType::mapAllocation:
            case AllocationType::svmCpu:
            case AllocationType::svmZeroCopy:
            case AllocationType::printfSurface:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
                break;
            case AllocationType::tagBuffer:
            case AllocationType::hostFunction:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
                break;
            case AllocationType::semaphoreBuffer:
            case AllocationType::ringBuffer:
            case AllocationType::commandBuffer:
                if (forceUncached) {
                    expectedUsage = uncachedGmmUsageType;
                    break;
                }

                if (productHelper.isResourceUncachedForCS(allocationType)) {
                    expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
                    break;
                } else if (allocationType == AllocationType::semaphoreBuffer) {
                    expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
                    break;
                }
                [[fallthrough]];
            default:
                expectedUsage = forceUncached ? uncachedGmmUsageType : GMM_RESOURCE_USAGE_OCL_BUFFER;
                break;
            }

            EXPECT_EQ(expectedUsage, usage);
        }
    }
}

TEST(GmmTest, givenAllocationTypeAndMitigatedDcFlushWhenGettingUsageTypeThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowDcFlush.set(0);
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    HardwareInfo hwInfo;
    hwInfo.capabilityTable.isIntegratedDevice = false;

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto usage = CacheSettingsHelper::getGmmUsageType(allocationType, false, productHelper, &hwInfo);
        auto expectedUsage = GMM_RESOURCE_USAGE_UNKNOWN;

        switch (allocationType) {
        case AllocationType::constantSurface:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
            break;
        case AllocationType::image:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_IMAGE;
            break;
        case AllocationType::internalHeap:
        case AllocationType::linearStream:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
            break;
        case AllocationType::bufferHostMemory:
        case AllocationType::externalHostPtr:
        case AllocationType::fillPattern:
        case AllocationType::internalHostMemory:
        case AllocationType::mapAllocation:
        case AllocationType::svmCpu:
        case AllocationType::svmZeroCopy:
        case AllocationType::tagBuffer:
        case AllocationType::printfSurface:
        case AllocationType::hostFunction:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
            break;
        default:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER;
            break;
        }

        EXPECT_EQ(expectedUsage, usage);
    }

    hwInfo.capabilityTable.isIntegratedDevice = true;

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto usage = CacheSettingsHelper::getGmmUsageType(allocationType, false, productHelper, &hwInfo);
        auto expectedUsage = GMM_RESOURCE_USAGE_UNKNOWN;

        switch (allocationType) {
        case AllocationType::constantSurface:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
            break;
        case AllocationType::image:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_IMAGE;
            break;
        case AllocationType::internalHeap:
        case AllocationType::linearStream:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
            break;
        case AllocationType::bufferHostMemory:
        case AllocationType::externalHostPtr:
        case AllocationType::fillPattern:
        case AllocationType::internalHostMemory:
        case AllocationType::mapAllocation:
        case AllocationType::svmCpu:
        case AllocationType::svmZeroCopy:
        case AllocationType::printfSurface:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
            break;
        case AllocationType::tagBuffer:
        case AllocationType::hostFunction:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
            break;
        case AllocationType::semaphoreBuffer:
        case AllocationType::ringBuffer:
        case AllocationType::commandBuffer:
            if (productHelper.isResourceUncachedForCS(allocationType)) {
                expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
                break;
            } else if (allocationType == AllocationType::semaphoreBuffer) {
                expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
                break;
            }
            [[fallthrough]];
        default:
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER;
            break;
        }

        EXPECT_EQ(expectedUsage, usage);
    }
}

TEST(GmmTest, whenTimestampAllocationsAreQueriedThenCorrectBufferPolicyIsReturned) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    auto uncachedType = productHelper.isNewCoherencyModelSupported() ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC : GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    auto expectedUsage = uncachedType;
    if (productHelper.isNonCoherentTimestampsModeEnabled()) {
        expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER;
    }
    EXPECT_EQ(expectedUsage, CacheSettingsHelper::getGmmUsageType(AllocationType::gpuTimestampDeviceBuffer, false, productHelper, defaultHwInfo.get()));
    EXPECT_EQ(expectedUsage, CacheSettingsHelper::getGmmUsageType(AllocationType::timestampPacketTagBuffer, false, productHelper, defaultHwInfo.get()));

    debugManager.flags.ForceNonCoherentModeForTimestamps.set(0);
    EXPECT_EQ(uncachedType, CacheSettingsHelper::getGmmUsageType(AllocationType::gpuTimestampDeviceBuffer, false, productHelper, defaultHwInfo.get()));
    EXPECT_EQ(uncachedType, CacheSettingsHelper::getGmmUsageType(AllocationType::timestampPacketTagBuffer, false, productHelper, defaultHwInfo.get()));

    debugManager.flags.ForceNonCoherentModeForTimestamps.set(1);
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER, CacheSettingsHelper::getGmmUsageType(AllocationType::gpuTimestampDeviceBuffer, false, productHelper, defaultHwInfo.get()));
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER, CacheSettingsHelper::getGmmUsageType(AllocationType::timestampPacketTagBuffer, false, productHelper, defaultHwInfo.get()));
}

TEST(GmmTest, givenForceAllResourcesUncachedFlagSetWhenGettingUsageTypeThenReturnUncached) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceAllResourcesUncached.set(true);
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);

        auto usage = CacheSettingsHelper::getGmmUsageType(allocationType, false, productHelper, defaultHwInfo.get());

        auto expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
        if (productHelper.isNewCoherencyModelSupported()) {
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
        }
        if (allocationType == AllocationType::preemption) {
            expectedUsage = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;

        } else if ((allocationType == AllocationType::internalHeap) || (allocationType == AllocationType::linearStream)) {
            expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
        }

        EXPECT_EQ(expectedUsage, usage);
    }
}

TEST(GmmTest, givenUsageTypeWhenAskingIfUncacheableThenReturnCorrectValue) {
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
    debugManager.flags.DisableCachingForHeaps.set(true);
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    auto usage = CacheSettingsHelper::getGmmUsageType(AllocationType::internalHeap, false, productHelper, defaultHwInfo.get());
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED, usage);

    usage = CacheSettingsHelper::getGmmUsageType(AllocationType::linearStream, false, productHelper, defaultHwInfo.get());
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED, usage);
}

TEST(GmmTest, givenConstSurfaceWhenDebugFlagIsSetThenReturnUncachedType) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(false);
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    auto expectedUncachedGmmUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    if (productHelper.isNewCoherencyModelSupported()) {
        expectedUncachedGmmUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
    }
    EXPECT_EQ(expectedUncachedGmmUsageType,
              CacheSettingsHelper::getGmmUsageType(AllocationType::constantSurface, false, productHelper, defaultHwInfo.get()));
}

TEST(GmmTest, givenUncachedDebugFlagMaskSetWhenAskingForUsageTypeThenReturnUncached) {
    DebugManagerStateRestore restore;

    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    constexpr int64_t bufferMask = 1 << (static_cast<int64_t>(AllocationType::buffer) - 1);
    constexpr int64_t imageMask = 1 << (static_cast<int64_t>(AllocationType::image) - 1);

    debugManager.flags.ForceUncachedGmmUsageType.set(bufferMask | imageMask);
    auto expectedUncachedGmmUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    if (productHelper.isNewCoherencyModelSupported()) {
        expectedUncachedGmmUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
    }
    EXPECT_EQ(expectedUncachedGmmUsageType,
              CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, false, productHelper, defaultHwInfo.get()));

    EXPECT_EQ(expectedUncachedGmmUsageType,
              CacheSettingsHelper::getGmmUsageType(AllocationType::image, false, productHelper, defaultHwInfo.get()));

    EXPECT_NE(expectedUncachedGmmUsageType,
              CacheSettingsHelper::getGmmUsageType(AllocationType::bufferHostMemory, false, productHelper, defaultHwInfo.get()));
}

TEST(GmmTest, givenFlagForceGmmSystemMemoryBufferForAllocationsWhenCallGetGmmUsageTypeThenReturnSystemMemoryBuffer) {
    DebugManagerStateRestore restore;

    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    constexpr int64_t bufferMask = 1ll << static_cast<int64_t>(AllocationType::buffer);
    constexpr int64_t globalFence = 1ll << static_cast<int64_t>(AllocationType::globalFence);

    auto defaultGmmUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER;

    EXPECT_EQ(defaultGmmUsageType,
              CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, false, productHelper, defaultHwInfo.get()));
    EXPECT_EQ(defaultGmmUsageType,
              CacheSettingsHelper::getGmmUsageType(AllocationType::globalFence, false, productHelper, defaultHwInfo.get()));

    debugManager.flags.ForceGmmSystemMemoryBufferForAllocations.set(bufferMask | globalFence);
    auto expectedGmmUsageTypeAfterForcingFlag = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;

    EXPECT_EQ(expectedGmmUsageTypeAfterForcingFlag,
              CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, false, productHelper, defaultHwInfo.get()));
    EXPECT_EQ(expectedGmmUsageTypeAfterForcingFlag,
              CacheSettingsHelper::getGmmUsageType(AllocationType::globalFence, false, productHelper, defaultHwInfo.get()));
}

TEST(GmmTest, givenAllocationForStatefulAccessWhenDebugFlagIsSetThenReturnUncachedType) {
    DebugManagerStateRestore restore;
    debugManager.flags.DisableCachingForStatefulBufferAccess.set(true);
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    auto expectedUncachedGmmUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    if (productHelper.isNewCoherencyModelSupported()) {
        expectedUncachedGmmUsageType = GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
    }
    for (auto allocType : {AllocationType::buffer,
                           AllocationType::bufferHostMemory,
                           AllocationType::externalHostPtr,
                           AllocationType::fillPattern,
                           AllocationType::internalHostMemory,
                           AllocationType::mapAllocation,
                           AllocationType::sharedBuffer,
                           AllocationType::svmCpu,
                           AllocationType::svmGpu,
                           AllocationType::svmZeroCopy,
                           AllocationType::unifiedSharedMemory}) {

        EXPECT_EQ(expectedUncachedGmmUsageType, CacheSettingsHelper::getGmmUsageType(allocType, false, productHelper, defaultHwInfo.get()));
    }
}

TEST_F(GmmHelperTests, whenGmmIsCreatedAndForceAllResourcesUncachedIsSetThenResourceUsageIsSetToUncachedSurface) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached = true;

    auto size = 4096u;
    void *incomingPtr = (void *)0x1000;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm1 = std::make_unique<Gmm>(getGmmHelper(), incomingPtr, size, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    EXPECT_EQ(GMM_RESOURCE_USAGE_SURFACE_UNCACHED, gmm1->resourceParams.Usage);

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image1D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    auto gmm2 = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);
    EXPECT_EQ(GMM_RESOURCE_USAGE_SURFACE_UNCACHED, gmm2->resourceParams.Usage);

    auto gmm3 = std::make_unique<Gmm>(getGmmHelper(), gmm1->gmmResourceInfo->peekGmmResourceInfo());
    EXPECT_EQ(GMM_RESOURCE_USAGE_SURFACE_UNCACHED, gmm3->resourceParams.Usage);
}

TEST_F(GmmHelperTests, whenResourceIsCreatedThenHandleItsOwnership) {
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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    EXPECT_FALSE(gmm->hasMultisampleControlSurface());
}

TEST_F(GmmEnvironmentTest, givenGmmWithSetMCSInResourceInfoGpuFlagsWhenCallhasMultisampleControlSurfaceThenReturnTrue) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    auto mockResource = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockResource->setMultisampleControlSurface();
    EXPECT_TRUE(gmm->hasMultisampleControlSurface());
}

TEST_F(GmmEnvironmentTest, whenGmmHelperIsInitializedThenClientContextIsSet) {
    ASSERT_NE(nullptr, getGmmHelper());
    EXPECT_NE(nullptr, getGmmClientContext()->getHandle());
}
TEST(GmmHelperTest, givenValidGmmFunctionsWhenCreateGmmHelperWithoutOsInterfaceThenInitializationDoesntCrashAndProperParametersArePassed) {
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = defaultHwInfo.get();
    SKU_FEATURE_TABLE expectedFtrTable = {};
    WA_TABLE expectedWaTable = {};
    SkuInfoTransfer::transferFtrTableForGmm(&expectedFtrTable, &hwInfo->featureTable);
    SkuInfoTransfer::transferWaTableForGmm(&expectedWaTable, &hwInfo->workaroundTable);

    MockExecutionEnvironment executionEnvironment{hwInfo};
    EXPECT_EQ(nullptr, executionEnvironment.rootDeviceEnvironments[0]->osInterface.get());
    EXPECT_EQ(0, memcmp(&hwInfo->platform, &passedInputArgs.Platform, sizeof(PLATFORM)));
    EXPECT_EQ(0, memcmp(&hwInfo->gtSystemInfo, &passedGtSystemInfo, sizeof(GT_SYSTEM_INFO)));
    EXPECT_EQ(0, memcmp(&expectedFtrTable, &passedFtrTable, sizeof(SKU_FEATURE_TABLE)));
    EXPECT_EQ(0, memcmp(&expectedWaTable, &passedWaTable, sizeof(WA_TABLE)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, passedInputArgs.ClientType);
}
TEST(GmmHelperTest, givenEnableFtrTile64OptimizationDebugKeyWhenSetThenProperValueIsPassedToGmmlib) {
    DebugManagerStateRestore restorer;
    VariableBackup<decltype(passedInputArgs)> passedInputArgsBackup(&passedInputArgs);
    VariableBackup<decltype(passedFtrTable)> passedFtrTableBackup(&passedFtrTable);
    VariableBackup<decltype(passedGtSystemInfo)> passedGtSystemInfoBackup(&passedGtSystemInfo);
    VariableBackup<decltype(passedWaTable)> passedWaTableBackup(&passedWaTable);
    VariableBackup<decltype(copyInputArgs)> copyInputArgsBackup(&copyInputArgs, true);

    auto hwInfo = defaultHwInfo.get();
    {
        MockExecutionEnvironment executionEnvironment{hwInfo};
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(-1);
        MockExecutionEnvironment executionEnvironment{hwInfo};
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(0);
        MockExecutionEnvironment executionEnvironment{hwInfo};
        EXPECT_EQ(0u, passedFtrTable.FtrTile64Optimization);
    }
    {
        debugManager.flags.EnableFtrTile64Optimization.set(1);
        MockExecutionEnvironment executionEnvironment{hwInfo};
        EXPECT_EQ(1u, passedFtrTable.FtrTile64Optimization);
    }
}

TEST(GmmHelperTest, givenNewCoherencyModelWhenGetMocsThenDeferToPat) {
    decltype(GmmHelper::createGmmContextWrapperFunc) createGmmContextSave = GmmHelper::createGmmContextWrapperFunc;
    GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<MockGmmClientContext>;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0];
    auto gmmHelper = rootDeviceEnvironment->getGmmHelper();
    if (!rootDeviceEnvironment->getProductHelper().deferMOCSToPatIndex(rootDeviceEnvironment->isWddmOnLinux())) {
        GTEST_SKIP();
    }

    EXPECT_EQ(0u, gmmHelper->getUncachedMOCS());
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(0u, gmmHelper->getL1EnabledMOCS());
    EXPECT_EQ(0u, gmmHelper->getL3EnabledMOCS());
    EXPECT_EQ(0u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));

    GmmHelper::createGmmContextWrapperFunc = createGmmContextSave;
}

TEST(GmmHelperTest, givenGmmHelperAndL3CacheDisabledForDebugThenCorrectMOCSIsReturned) {
    decltype(GmmHelper::createGmmContextWrapperFunc) createGmmContextSave = GmmHelper::createGmmContextWrapperFunc;
    GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<MockGmmClientContext>;

    MockExecutionEnvironment executionEnvironment{};
    if (executionEnvironment.rootDeviceEnvironments[0]->getProductHelper().isNewCoherencyModelSupported()) {
        GTEST_SKIP();
    }
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();

    auto uncachedMocs = gmmHelper->getUncachedMOCS();

    EXPECT_EQ(uncachedMocs, gmmHelper->getUncachedMOCS());
    EXPECT_EQ(2u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(8u, gmmHelper->getL1EnabledMOCS());
    EXPECT_EQ(16u, gmmHelper->getL3EnabledMOCS());
    EXPECT_EQ(32u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));

    gmmHelper->forceAllResourcesUncached();

    EXPECT_EQ(uncachedMocs, gmmHelper->getUncachedMOCS());
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(uncachedMocs, gmmHelper->getL1EnabledMOCS());
    EXPECT_EQ(uncachedMocs, gmmHelper->getL3EnabledMOCS());
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));
    GmmHelper::createGmmContextWrapperFunc = createGmmContextSave;
}

TEST(GmmHelperTest, givenGmmHelperAndForceAllResourcesUncachedDebugVariableSetThenCorrectMOCSIsReturned) {
    decltype(GmmHelper::createGmmContextWrapperFunc) createGmmContextSave = GmmHelper::createGmmContextWrapperFunc;
    GmmHelper::createGmmContextWrapperFunc = GmmClientContext::create<MockGmmClientContext>;

    MockExecutionEnvironment executionEnvironment{};
    if (executionEnvironment.rootDeviceEnvironments[0]->getProductHelper().isNewCoherencyModelSupported()) {
        GTEST_SKIP();
    }
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();

    auto uncachedMocs = gmmHelper->getUncachedMOCS();

    EXPECT_EQ(uncachedMocs, gmmHelper->getUncachedMOCS());
    EXPECT_EQ(2u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(4u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(8u, gmmHelper->getL1EnabledMOCS());
    EXPECT_EQ(16u, gmmHelper->getL3EnabledMOCS());
    EXPECT_EQ(32u, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));

    DebugManagerStateRestore restore;
    debugManager.flags.ForceAllResourcesUncached.set(true);
    gmmHelper->initMocsDefaults();

    EXPECT_EQ(uncachedMocs, gmmHelper->getUncachedMOCS());
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE));
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_IMAGE_FROM_BUFFER));
    EXPECT_EQ(uncachedMocs, gmmHelper->getL1EnabledMOCS());
    EXPECT_EQ(uncachedMocs, gmmHelper->getL3EnabledMOCS());
    EXPECT_EQ(uncachedMocs, gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_INLINE_CONST_HDC));
    GmmHelper::createGmmContextWrapperFunc = createGmmContextSave;
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

TEST_F(GmmCompressionTests, givenEnabledAndNotPreferredE2ECWhenApplyingForBuffersThenDontSetValidFlags) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->resourceParams = {};

    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;

    gmm->applyAuxFlagsForBuffer(false);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(gmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenDisabledAndPreferredE2ECWhenApplyingForBuffersThenDontSetValidFlags) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->resourceParams = {};

    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = false;

    gmm->applyAuxFlagsForBuffer(true);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(gmm->isCompressionEnabled());
}

HWTEST_F(GmmCompressionTests, givenAllValidInputsWhenQueryingThenSetAppropriateFlags) {
    EXPECT_TRUE(localPlatformDevice->capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(imgInfo.surfaceFormat->gmmSurfaceFormat != GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12);
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
    EXPECT_TRUE(queryGmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenAllValidInputsAndNoLocalMemoryRequestWhenQueryingThenRenderCompressionFlagsAreNotSet) {
    EXPECT_TRUE(localPlatformDevice->capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(imgInfo.surfaceFormat->gmmSurfaceFormat != GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12);
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
    EXPECT_FALSE(queryGmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenNotAllowedRenderCompressionWhenQueryingThenSetAppropriateFlags) {
    localPlatformDevice->capabilityTable.ftrRenderCompressedImages = false;
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_FALSE(queryGmm->isCompressionEnabled());
}

HWTEST_F(GmmCompressionTests, givenNotAllowedCompressionAndEnabledDebugFlagWhenQueryingThenSetAppropriateFlags) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedImagesEnabled.set(1);
    localPlatformDevice->capabilityTable.ftrRenderCompressedImages = false;
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(1u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_TRUE(queryGmm->isCompressionEnabled());

    debugManager.flags.RenderCompressedImagesEnabled.set(0);
    localPlatformDevice->capabilityTable.ftrRenderCompressedImages = true;
    queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_EQ(0u, queryGmm->resourceParams.Flags.Gpu.IndirectClearColor);
    EXPECT_FALSE(queryGmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenNotPreferredCompressionFlagWhenQueryingThenDisallow) {
    auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, false);

    EXPECT_FALSE(queryGmm->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenPlaneFormatWhenQueryingThenDisallow) {
    GMM_YUV_PLANE gmmPlane[4] = {GMM_YUV_PLANE::GMM_NO_PLANE, GMM_YUV_PLANE::GMM_PLANE_U,
                                 GMM_YUV_PLANE::GMM_PLANE_V, GMM_YUV_PLANE::GMM_PLANE_Y};

    for (auto &plane : gmmPlane) {
        imgInfo.plane = plane;
        auto queryGmm = MockGmm::queryImgParams(getGmmHelper(), imgInfo, true);

        EXPECT_EQ(queryGmm->isCompressionEnabled(),
                  plane == GMM_YUV_PLANE::GMM_NO_PLANE);
    }
}

HWTEST_F(GmmCompressionTests, whenConstructedWithPreferCompressionFlagThenApplyAuxFlags) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm1(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_EQ(0u, gmm1.resourceParams.Flags.Info.RenderCompressed);

    Gmm gmm2(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_EQ(0u, gmm2.resourceParams.Flags.Info.RenderCompressed);

    gmmRequirements.preferCompressed = true;
    Gmm gmm3(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_EQ(1u, gmm3.resourceParams.Flags.Info.RenderCompressed);
}

TEST_F(GmmCompressionTests, givenMediaCompressedImageApplyAuxFlagsForImageThenSetFlagsToCompressed) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm.resourceParams.Flags.Info.MediaCompressed = true;
    gmm.resourceParams.Flags.Info.RenderCompressed = false;
    gmm.setupImageResourceParams(imgInfo, true);

    EXPECT_TRUE(gmm.isCompressionEnabled());
}

HWTEST2_F(GmmCompressionTests, givenGmmCreatedFromExistingGmmWithRenderCompressionThenCompressionFalseForSecondResource, IsAtMostXeCore) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    auto flags = gmm.gmmResourceInfo->getResourceFlags();
    flags->Gpu.CCS = true;
    flags->Gpu.UnifiedAuxSurface = true;
    flags->Info.MediaCompressed = true;
    flags->Info.RenderCompressed = false;
    gmm.resourceParams.Flags.Info.MediaCompressed = false;
    gmm.resourceParams.Flags.Info.RenderCompressed = true;
    gmm.setupImageResourceParams(imgInfo, true);

    EXPECT_TRUE(gmm.isCompressionEnabled());

    auto gmmRes2 = std::make_unique<Gmm>(getGmmHelper(), gmm.gmmResourceInfo->peekGmmResourceInfo(), false);
    EXPECT_FALSE(gmmRes2->isCompressionEnabled());
}

HWTEST2_F(GmmCompressionTests, givenGmmCreatedFromExistingGmmWithCompressionDisabledThenCompressionFalseForSecondResource, IsAtMostXeCore) {
    localPlatformDevice->capabilityTable.ftrRenderCompressedImages = false;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm.resourceParams.Flags.Info.MediaCompressed = false;
    gmm.resourceParams.Flags.Info.RenderCompressed = false;
    gmm.setupImageResourceParams(imgInfo, false);

    EXPECT_FALSE(gmm.isCompressionEnabled());

    auto gmmRes2 = std::make_unique<Gmm>(getGmmHelper(), gmm.gmmResourceInfo->peekGmmResourceInfo(), false);
    EXPECT_FALSE(gmmRes2->isCompressionEnabled());
}

TEST_F(GmmCompressionTests, givenRenderCompressedImageApplyAuxFlagsForImageThenSetFlagsToCompressed) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm.resourceParams.Flags.Info.MediaCompressed = false;
    gmm.resourceParams.Flags.Info.RenderCompressed = true;
    gmm.setupImageResourceParams(imgInfo, true);

    EXPECT_TRUE(gmm.isCompressionEnabled());
}

HWTEST_F(GmmCompressionTests, givenEnabledAndPreferredE2ECWhenApplyingForBuffersThenSetValidFlags) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    std::unique_ptr<Gmm> gmm(new Gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    gmm->resourceParams = {};

    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;

    gmm->applyAuxFlagsForBuffer(true);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_TRUE(gmm->isCompressionEnabled());
}

HWTEST_F(GmmCompressionTests, givenDisabledE2ECAndEnabledDebugFlagWhenApplyingForBuffersThenSetValidFlags) {
    DebugManagerStateRestore restore;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm.resourceParams = {};

    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = false;

    gmm.applyAuxFlagsForBuffer(true);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(1u, gmm.resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_TRUE(gmm.isCompressionEnabled());

    gmm.resourceParams = {};
    gmm.setCompressionEnabled(false);
    debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    localPlatformDevice->capabilityTable.ftrRenderCompressedBuffers = true;

    gmm.applyAuxFlagsForBuffer(true);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Info.RenderCompressed);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Gpu.CCS);
    EXPECT_EQ(0u, gmm.resourceParams.Flags.Gpu.UnifiedAuxSurface);
    EXPECT_FALSE(gmm.isCompressionEnabled());
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

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryWhenUseSystemMemoryIsTrueThenNonLocalOnlyFlagIsSetAndLocalOnlyCleared) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
}

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryWhenUsingLocalMemoryAndAllocationIsLockableThenAllFlagsAreCleared) {
    StorageInfo storageInfo{};
    storageInfo.isLockable = true;
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmHelperTests, givenNotLockableAllocationWhenCreatingGmmThenNotLockableFlagsIsSetAndLocalAndNonLocalOnlyAreNotSet) {
    StorageInfo storageInfo{};
    storageInfo.isLockable = false;
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenLocalMemoryAndStorageInfoWithLocalOnlyRequiredWhenPreparingFlagsForGmmThenNotLockableAndLocalOnlyAreSet) {
    StorageInfo storageInfo{};
    storageInfo.localOnlyRequired = true;
    storageInfo.isLockable = false;
    storageInfo.memoryBanks.set(1);
    storageInfo.systemMemoryPlacement = false;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    DebugManagerStateRestore restorer;

    for (auto csrMode = static_cast<int32_t>(CommandStreamReceiverType::hardware); csrMode < static_cast<int32_t>(CommandStreamReceiverType::typesNum); csrMode++) {
        debugManager.flags.SetCommandStreamReceiver.set(csrMode);
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
        EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.LocalOnly);
        EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
    }
}

TEST_F(GmmLocalMemoryTests, givenSystemMemoryAndStorageInfoWithLocalOnlyRequiredWhenPreparingFlagsForGmmThenLocalOnlyIsNotSet) {
    StorageInfo storageInfo{};
    storageInfo.localOnlyRequired = true;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
}

TEST_F(GmmLocalMemoryTests, givenLocalMemoryAndStorageInfoWithoutLocalOnlyRequiredWhenPreparingFlagsForGmmThenLocalOnlyIsNotSet) {
    StorageInfo storageInfo{};
    storageInfo.localOnlyRequired = false;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
}

TEST_F(GmmHelperTests, givenCompressionEnabledWhenUsingLocalMemoryAndAllocationIsNotLockableThenNotLockableAndLocalOnlyFlagsAreSetAndNonLocalOnlyIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    StorageInfo storageInfo{};
    storageInfo.isLockable = false;
    storageInfo.systemMemoryPlacement = false;
    storageInfo.memoryBanks.set(1);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
    EXPECT_TRUE(gmm->isCompressionEnabled());
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
    EXPECT_EQ(defaultHwInfo->featureTable.flags.ftrLocalMemory, gmm->resourceParams.Flags.Info.LocalOnly);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmHelperTests, givenCompressionSupportedWhenAllocationIsLockableThenGmmHasNoCompression) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    StorageInfo storageInfo{};
    storageInfo.isLockable = true;
    storageInfo.systemMemoryPlacement = false;
    storageInfo.memoryBanks.set(1);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
    EXPECT_FALSE(gmm->isCompressionEnabled());
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NotLockable);
}

TEST_F(GmmLocalMemoryTests, givenFtrLocalMemoryWhenUseSystemMemoryIsFalseAndAllocationIsNotLockableThenLocalAndNonLocalOnlyAndNotLockableFlagsAreNotSet) {
    DebugManagerStateRestore restorer;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    for (auto csrMode = static_cast<int32_t>(CommandStreamReceiverType::hardware); csrMode < static_cast<int32_t>(CommandStreamReceiverType::typesNum); csrMode++) {
        debugManager.flags.SetCommandStreamReceiver.set(csrMode);
        StorageInfo storageInfo{};
        storageInfo.memoryBanks.set(1);
        storageInfo.systemMemoryPlacement = false;
        storageInfo.isLockable = false;
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);
        EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);
        EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.LocalOnly);
        EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.NotLockable);
    }
}

TEST_F(MultiTileGmmTests, givenMultiTileAllocationWhenGmmIsCreatedWithEmptyMemporyBanksThenMultitileArchIsEnabled) {
    StorageInfo storageInfo;
    storageInfo.memoryBanks = 0;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, givenMultiTileAllocationWithoutCloningWhenGmmIsCreatedThenSetMinimumOneTile) {
    StorageInfo storageInfo;
    storageInfo.memoryBanks = 1;
    storageInfo.cloningOfPageTables = false;
    storageInfo.systemMemoryPlacement = false;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.GpuVaMappingSet);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.LocalMemPreferredSet);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.LocalMemEligibilitySet);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, givenMultiTileWhenGmmIsCreatedWithNonLocalMemoryThenMultitileArchIsPropertlyFilled) {
    StorageInfo storageInfo;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.TileInstanced);
}

TEST_F(MultiTileGmmTests, whenAllocationIsTileInstancedWithClonningPageTablesThenResourceParamsHaveTileInstancedDisabled) {
    StorageInfo storageInfo;
    storageInfo.cloningOfPageTables = true;
    storageInfo.tileInstanced = true;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm gmm(getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, storageInfo, gmmRequirements);

    EXPECT_EQ(1u, gmm.resourceParams.MultiTileArch.Enable);
    EXPECT_EQ(0u, gmm.resourceParams.MultiTileArch.TileInstanced);
}
