/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mem_obj/image_compression_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "test_traits_common.h"

#include <functional>

using namespace NEO;

using XeHPAndLaterImageTests = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterImageTests, WhenAppendingSurfaceStateParamsThenDoNothing) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = FamilyType::cmdInitRenderSurfaceState;
    auto surfaceStateAfter = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->appendSurfaceStateParams(&surfaceStateAfter, context.getDevice(0)->getRootDeviceIndex());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterImageTests, givenCompressionEnabledWhenAppendingSurfaceStateParamsThenProgramCompressionFormat) {
    MockContext context;
    auto mockGmmClient = static_cast<MockGmmClientContext *>(context.getDevice(0)->getRootDeviceEnvironment().getGmmClientContext());
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto image = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(&context));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    mockGmmClient->capturedFormat = GMM_FORMAT_INVALID;
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    surfaceState.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(0u, surfaceState.getCompressionFormat());
    EXPECT_EQ(GMM_FORMAT_INVALID, mockGmmClient->capturedFormat);

    auto gmm = image->getMultiGraphicsAllocation().getDefaultGraphicsAllocation()->getDefaultGmm();
    gmm->setCompressionEnabled(true);

    surfaceState = FamilyType::cmdInitRenderSurfaceState;
    imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(&surfaceState, gmm));

    EXPECT_NE(0u, surfaceState.getCompressionFormat());
    EXPECT_EQ(image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getResourceFormat(), mockGmmClient->capturedFormat);
}

HWTEST2_F(XeHPAndLaterImageTests, givenCompressionWhenAppendingImageFromBufferThenTwoIsSetAsCompressionFormat, IsXeHpcCore) {
    MockContext context;

    uint32_t compressionFormat = context.getDevice(0)->getGmmHelper()->getClientContext()->getSurfaceStateCompressionFormat(GMM_RESOURCE_FORMAT::GMM_FORMAT_GENERIC_8BIT);

    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_READ_WRITE, 128 * 256 * 4, nullptr, &retVal);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm->setCompressionEnabled(true);

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    buffer->getGraphicsAllocation(0)->setGmm(gmm, 0);

    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
        CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(&surfaceState, gmm));

    EXPECT_EQ(compressionFormat, surfaceState.getCompressionFormat());
    clReleaseMemObject(imageDesc.mem_object);
}

HWTEST2_F(XeHPAndLaterImageTests, givenImageFromBufferWhenSettingSurfaceStateThenPickCompressionFormatFromDebugVariable, IsXeHpcCore) {
    DebugManagerStateRestore restorer;

    uint32_t bufferCompressionFormat = 3;
    debugManager.flags.ForceBufferCompressionFormat.set(bufferCompressionFormat);

    MockContext context;
    cl_int retVal = CL_SUCCESS;
    cl_image_format imageFormat = {};
    cl_image_desc imageDesc = {};

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_height = 128;
    imageDesc.image_width = 256;

    imageDesc.mem_object = clCreateBuffer(&context, CL_MEM_READ_WRITE, 128 * 256 * 4, nullptr, &retVal);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = new Gmm(context.getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    gmm->setCompressionEnabled(true);

    auto buffer = castToObject<Buffer>(imageDesc.mem_object);
    buffer->getGraphicsAllocation(0)->setGmm(gmm, 0);

    auto surfaceFormat = Image::getSurfaceFormatFromTable(CL_MEM_READ_WRITE, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = std::unique_ptr<Image>(Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context.getDevice(0)->getDevice()),
                                                      CL_MEM_READ_WRITE, 0, surfaceFormat, &imageDesc, NULL, retVal));
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_TRUE(EncodeSurfaceState<FamilyType>::isAuxModeEnabled(&surfaceState, gmm));

    EXPECT_EQ(bufferCompressionFormat, surfaceState.getCompressionFormat());
    clReleaseMemObject(imageDesc.mem_object);
}

struct CompressionParamsSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::surfaceStateCompressionParamsSupported;
        }
        return false;
    }
};

HWTEST2_F(XeHPAndLaterImageTests, givenMcsAllocationWhenSetArgIsCalledWithUnifiedAuxCapabilityAndMCSThenProgramAuxFieldsForCcs, CompressionParamsSupportedMatcher) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    MockContext context;
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    mcsAlloc->setDefaultGmm(new Gmm(pClDevice->getRootDeviceEnvironment().getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    surfaceState.setSurfaceBaseAddress(0xABCDEF1000);
    imageHw->setMcsSurfaceInfo(msi);
    imageHw->setMcsAllocation(mcsAlloc);
    auto mockResource = static_cast<MockGmmResourceInfo *>(mcsAlloc->getDefaultGmm()->gmmResourceInfo.get());
    mockResource->setUnifiedAuxTranslationCapable();
    mockResource->setMultisampleControlSurface();

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());

    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    auto releaseHelper = pClDevice->getDevice().getReleaseHelper();
    auto expectedMode = AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE;
    if (releaseHelper && releaseHelper->isAuxSurfaceModeOverrideRequired())
        expectedMode = AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E;

    EXPECT_NE(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), expectedMode);
}

HWTEST2_F(XeHPAndLaterImageTests, givenMcsAllocationWhenSetAuxParamsForMultisampleCalledThenAuxModeIsExpected, IsAtLeastXe2HpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockContext context;
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    mcsAlloc->setDefaultGmm(new Gmm(pClDevice->getRootDeviceEnvironment().getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    surfaceState.setSurfaceBaseAddress(0xABCDEF1000);
    imageHw->setMcsSurfaceInfo(msi);
    imageHw->setMcsAllocation(mcsAlloc);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());

    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    auto releaseHelper = pClDevice->getDevice().getReleaseHelper();
    RENDER_SURFACE_STATE expectedSS = {};
    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&expectedSS, releaseHelper);

    EXPECT_NE(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), expectedSS.getAuxiliarySurfaceMode());
}

HWTEST2_F(XeHPAndLaterImageTests, givenImageWithUnifiedMcsWhenSetAuxParamsForMultisampleThenAuxModeIsExpected, IsAtLeastXe2HpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockContext context;
    McsSurfaceInfo msi = {10, 20, 3};

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceBaseAddress(0xABCDEF1000);
    imageHw->setMcsSurfaceInfo(msi);
    imageHw->setIsUnifiedMcsSurface(true);

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());

    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    auto releaseHelper = pClDevice->getDevice().getReleaseHelper();
    RENDER_SURFACE_STATE expectedSS = {};
    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&expectedSS, releaseHelper);
    EXPECT_NE(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), expectedSS.getAuxiliarySurfaceMode());
}

HWTEST2_F(XeHPAndLaterImageTests, givenImageWithoutMcsWhenSetAuxParamsForMultisampleThenAuxSurfBaseAddredssIsZero, IsAtLeastXe2HpgCore) {
    MockContext context;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
}

HWTEST2_F(XeHPAndLaterImageTests, givenImageWithoutMcsWithDepthFormatWhenSetAuxParamsForMultisampleThenStorageFormatIsSetToDepthStencil, IsAtLeastXe2HpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockContext context;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    cl_image_format imgFormat = {CL_DEPTH, CL_FLOAT};
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc, &imgFormat));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT::SURFACE_FORMAT_R8G8B8A8_UINT);
    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    EXPECT_EQ(surfaceState.getMultisampledSurfaceStorageFormat(),
              RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_DEPTH_STENCIL);
}

HWTEST2_F(XeHPAndLaterImageTests, givenImageWithoutMcsWithNotDepthFormatWhenSetAuxParamsForMultisampleThenStorageFormatIsSetToMSS, IsAtLeastXe2HpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockContext context;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    cl_image_format imgFormat = {CL_RGBA, CL_FLOAT};
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc, &imgFormat));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT::SURFACE_FORMAT_R8G8B8A8_UINT);
    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    EXPECT_EQ(surfaceState.getMultisampledSurfaceStorageFormat(),
              RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
}

HWTEST2_F(XeHPAndLaterImageTests, givenImageWithoutMcsWithTypelessSurfaceStateFormatWhenSetAuxParamsForMultisampleThenStorageFormatIsSetToMSS, IsAtLeastXe2HpgCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    MockContext context;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    cl_image_format imgFormat = {CL_DEPTH, CL_FLOAT};
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc, &imgFormat));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceFormat(RENDER_SURFACE_STATE::SURFACE_FORMAT::SURFACE_FORMAT_R32_FLOAT_X8X24_TYPELESS);
    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    EXPECT_EQ(surfaceState.getMultisampledSurfaceStorageFormat(),
              RENDER_SURFACE_STATE::MULTISAMPLED_SURFACE_STORAGE_FORMAT::MULTISAMPLED_SURFACE_STORAGE_FORMAT_MSS);
}

HWTEST2_F(ImageClearColorFixture, givenImageForXeHPAndLaterWhenClearColorParametersAreSetThenClearColorSurfaceInSurfaceStateIsSet, CompressionParamsSupportedMatcher) {
    this->setUpImpl<FamilyType>();
    auto surfaceState = this->getSurfaceState<FamilyType>();

    surfaceState.setSurfaceBaseAddress(0xABCDEF1000);

    EXPECT_EQ(false, surfaceState.getClearValueAddressEnable());
    EXPECT_EQ(0u, surfaceState.getClearColorAddress());
    EXPECT_EQ(0u, surfaceState.getClearColorAddressHigh());

    std::unique_ptr<ImageHw<FamilyType>> imageHw(static_cast<ImageHw<FamilyType> *>(ImageHelperUlt<Image2dDefaults>::create(&context)));
    auto gmm = imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm();
    gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = 1;
    EncodeSurfaceState<FamilyType>::setClearColorParams(&surfaceState, gmm);

    EXPECT_EQ(true, surfaceState.getClearValueAddressEnable());
    EXPECT_NE(0u, surfaceState.getClearColorAddress());
    EXPECT_NE(0u, surfaceState.getClearColorAddressHigh());
}

HWTEST2_F(ImageClearColorFixture, givenSurfaceStateWhenAuxParamsForMCSCCSAreSetThenAuxParamsInSurfaceStateIsSet, CompressionParamsSupportedMatcher) {
    this->setUpImpl<FamilyType>();
    auto surfaceState = this->getSurfaceState<FamilyType>();

    auto releaseHelper = context.getDevice(0)->getRootDeviceEnvironment().getReleaseHelper();
    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&surfaceState, releaseHelper);

    auto expectedAuxMode = releaseHelper && releaseHelper->isAuxSurfaceModeOverrideRequired() ? EncodeSurfaceState<FamilyType>::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E : EncodeSurfaceState<FamilyType>::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE;

    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), expectedAuxMode);
}

struct CompressionClearColorAddressMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::clearColorAddressMatcher;
        }
        return false;
    }
};

HWTEST2_F(ImageClearColorFixture, givenImageForXeHPAndLaterWhenCanonicalAddresForClearColorIsUsedThenItsConvertedToNonCanonicalForm, CompressionClearColorAddressMatcher) {
    this->setUpImpl<FamilyType>();
    auto surfaceState = this->getSurfaceState<FamilyType>();

    uint64_t canonicalAddress = 0xffffABCDABCDE000;

    EXPECT_THROW(surfaceState.setClearColorAddressHigh(static_cast<uint32_t>(canonicalAddress >> 32)), std::exception);
    surfaceState.setSurfaceBaseAddress(canonicalAddress);

    std::unique_ptr<ImageHw<FamilyType>> imageHw(static_cast<ImageHw<FamilyType> *>(ImageHelperUlt<Image2dDefaults>::create(&context)));

    auto gmm = imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm();
    gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = 1;
    EXPECT_NO_THROW(EncodeSurfaceState<FamilyType>::setClearColorParams(&surfaceState, gmm));

    uint64_t nonCanonicalAddress = ((static_cast<uint64_t>(surfaceState.getClearColorAddressHigh()) << 32) | surfaceState.getClearColorAddress());
    auto gmmHelper = context.getDevice(0)->getGmmHelper();

    EXPECT_EQ(gmmHelper->decanonize(canonicalAddress), nonCanonicalAddress);
}

HWTEST2_F(XeHPAndLaterImageTests, givenMediaCompressionWhenAppendingNewAllocationThenNotZeroIsSetAsCompressionType, CompressionParamsSupportedMatcher) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto hwInfo = defaultHwInfo.get();
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    surfaceState.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);

    EncodeSurfaceState<FamilyType>::setImageAuxParamsForCCS(&surfaceState,
                                                            imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm());

    imageHw->setImageArg(&surfaceState, false, 0, context.getDevice(0)->getRootDeviceIndex());

    if (hwInfo->featureTable.flags.ftrFlatPhysCCS) {
        EXPECT_NE(surfaceState.getCompressionFormat(), GMM_FLATCCS_FORMAT::GMM_FLATCCS_FORMAT_INVALID);
    } else {
        EXPECT_NE(surfaceState.getCompressionFormat(), GMM_E2ECOMP_FORMAT::GMM_E2ECOMP_FORMAT_INVALID);
    }

    EXPECT_TRUE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
}

HWTEST2_F(XeHPAndLaterImageTests, givenCompressionWhenAppendingNewAllocationThenNotZeroIsSetAsCompressionType, CompressionParamsSupportedMatcher) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto hwInfo = defaultHwInfo.get();
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
    auto gmm = imageHw->getGraphicsAllocation(rootDeviceIndex)->getDefaultGmm();

    gmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = true;
    gmm->setCompressionEnabled(true);

    auto mcsGmm = new MockGmm(context.getDevice(0)->getGmmHelper());
    mcsGmm->setCompressionEnabled(true);
    mcsGmm->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = true;
    mcsGmm->gmmResourceInfo->getResourceFlags()->Gpu.UnifiedAuxSurface = true;
    mcsGmm->gmmResourceInfo->getResourceFlags()->Gpu.CCS = true;

    auto mcsAlloc = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, 1});
    mcsAlloc->setDefaultGmm(mcsGmm);
    imageHw->setMcsAllocation(mcsAlloc);

    imageHw->setImageArg(&surfaceState, false, 0, rootDeviceIndex);

    if (hwInfo->featureTable.flags.ftrFlatPhysCCS) {
        EXPECT_NE(surfaceState.getCompressionFormat(), GMM_FLATCCS_FORMAT::GMM_FLATCCS_FORMAT_INVALID);
    } else {
        EXPECT_NE(surfaceState.getCompressionFormat(), GMM_E2ECOMP_FORMAT::GMM_E2ECOMP_FORMAT_INVALID);
    }

    EXPECT_FALSE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

HWTEST2_F(XeHPAndLaterImageTests, givenNoCompressionWhenProgramingImageSurfaceStateThenCompressionIsDisabled, CompressionParamsSupportedMatcher) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    surfaceState.setMemoryCompressionEnable(true);
    surfaceState.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->setCompressionEnabled(false);
    imageHw->setImageArg(&surfaceState, false, 0, 0);

    EXPECT_FALSE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
}

struct XeHPAndLaterImageHelperTests : ::testing::Test {
    void SetUp() override {
        context = std::make_unique<MockContext>();
        image.reset(ImageHelperUlt<Image2dDefaults>::create(context.get()));
        mockGmmResourceInfo = static_cast<MockGmmResourceInfo *>(image->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo.get());
        gmmClientContext = static_cast<MockGmmClientContext *>(context->getDevice(0)->getGmmHelper()->getClientContext());
    }

    std::unique_ptr<MockContext> context;
    std::unique_ptr<Image> image;
    MockGmmResourceInfo *mockGmmResourceInfo;
    MockGmmClientContext *gmmClientContext;
    uint8_t mockCompressionFormat = 3u;
};

HWTEST2_F(XeHPAndLaterImageHelperTests, givenMediaCompressedImageWhenAppendingSurfaceStateParamsForCompressionThenCallAppriopriateFunction, CompressionParamsSupportedMatcher) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE rss{};
    platformsImpl->clear();
    rss.setMemoryCompressionEnable(true);
    mockGmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    gmmClientContext->compressionFormatToReturn = mockCompressionFormat;
    const auto expectedGetSurfaceStateCompressionFormatCalled = gmmClientContext->getSurfaceStateCompressionFormatCalled;
    const auto expectedGetMediaSurfaceStateCompressionFormatCalled = gmmClientContext->getMediaSurfaceStateCompressionFormatCalled + 1;

    EncodeSurfaceState<FamilyType>::appendImageCompressionParams(&rss, image->getMultiGraphicsAllocation().getDefaultGraphicsAllocation(),
                                                                 context->getDevice(0)->getGmmHelper(), false, GMM_NO_PLANE);

    EXPECT_EQ(platform(), nullptr);
    EXPECT_EQ(mockCompressionFormat, rss.getCompressionFormat());
    EXPECT_EQ(expectedGetSurfaceStateCompressionFormatCalled, gmmClientContext->getSurfaceStateCompressionFormatCalled);
    EXPECT_EQ(expectedGetMediaSurfaceStateCompressionFormatCalled, gmmClientContext->getMediaSurfaceStateCompressionFormatCalled);
}

HWTEST2_F(XeHPAndLaterImageHelperTests, givenMediaCompressedPlanarImageWhenAppendingSurfaceStateParamsForCompressionThenCorrectCompressionFormatIsSet, CompressionParamsSupportedMatcher) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE rss{};
    platformsImpl->clear();
    rss.setMemoryCompressionEnable(true);
    mockGmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;

    struct {
        uint8_t returnedCompressionFormat;
        uint8_t expectedCompressionFormat;
        GMM_YUV_PLANE_ENUM plane;
    } testInputs[] = {
        // regular image
        {0x0, 0x0, GMM_NO_PLANE},
        {0xF, 0xF, GMM_NO_PLANE},
        {0x10, 0x10, GMM_NO_PLANE},
        {0x1F, 0x1F, GMM_NO_PLANE},
        // luma plane
        {0x0, 0x0, GMM_PLANE_Y},
        {0xF, 0xF, GMM_PLANE_Y},
        {0x10, 0x0, GMM_PLANE_Y},
        {0x1F, 0xF, GMM_PLANE_Y},
        // chroma plane
        {0x0, 0x10, GMM_PLANE_U},
        {0x0, 0x10, GMM_PLANE_V},
        {0xF, 0x1F, GMM_PLANE_U},
        {0xF, 0x1F, GMM_PLANE_V},
        {0x10, 0x10, GMM_PLANE_U},
        {0x10, 0x10, GMM_PLANE_V},
        {0x1F, 0x1F, GMM_PLANE_U},
        {0x1F, 0x1F, GMM_PLANE_V},
    };

    for (auto &testInput : testInputs) {
        gmmClientContext->compressionFormatToReturn = testInput.returnedCompressionFormat;
        EncodeSurfaceState<FamilyType>::appendImageCompressionParams(&rss, image->getMultiGraphicsAllocation().getDefaultGraphicsAllocation(),
                                                                     context->getDevice(0)->getGmmHelper(), false, testInput.plane);
        EXPECT_EQ(testInput.expectedCompressionFormat, rss.getCompressionFormat());
    }
}

HWTEST2_F(XeHPAndLaterImageHelperTests, givenNotMediaCompressedImageWhenAppendingSurfaceStateParamsForCompressionThenCallAppriopriateFunction, CompressionParamsSupportedMatcher) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    RENDER_SURFACE_STATE rss{};
    platformsImpl->clear();
    rss.setMemoryCompressionEnable(true);
    mockGmmResourceInfo->getResourceFlags()->Info.MediaCompressed = false;
    gmmClientContext->compressionFormatToReturn = mockCompressionFormat;
    const auto expectedGetSurfaceStateCompressionFormatCalled = gmmClientContext->getSurfaceStateCompressionFormatCalled + 1;
    const auto expectedGetMediaSurfaceStateCompressionFormatCalled = gmmClientContext->getMediaSurfaceStateCompressionFormatCalled;

    EncodeSurfaceState<FamilyType>::appendImageCompressionParams(&rss, image->getMultiGraphicsAllocation().getDefaultGraphicsAllocation(),
                                                                 context->getDevice(0)->getGmmHelper(), false, GMM_NO_PLANE);
    EXPECT_EQ(platform(), nullptr);
    EXPECT_EQ(mockCompressionFormat, rss.getCompressionFormat());
    EXPECT_EQ(expectedGetSurfaceStateCompressionFormatCalled, gmmClientContext->getSurfaceStateCompressionFormatCalled);
    EXPECT_EQ(expectedGetMediaSurfaceStateCompressionFormatCalled, gmmClientContext->getMediaSurfaceStateCompressionFormatCalled);
}

HWTEST2_F(XeHPAndLaterImageHelperTests, givenAuxModeMcsLceWhenAppendingSurfaceStateParamsForCompressionThenCallAppriopriateFunction, CompressionParamsSupportedMatcher) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    RENDER_SURFACE_STATE rss{};
    platformsImpl->clear();
    rss.setMemoryCompressionEnable(false);
    rss.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
    mockGmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    gmmClientContext->compressionFormatToReturn = mockCompressionFormat;
    const auto expectedGetSurfaceStateCompressionFormatCalled = gmmClientContext->getSurfaceStateCompressionFormatCalled;
    const auto expectedGetMediaSurfaceStateCompressionFormatCalled = gmmClientContext->getMediaSurfaceStateCompressionFormatCalled + 1;

    EncodeSurfaceState<FamilyType>::appendImageCompressionParams(&rss, image->getMultiGraphicsAllocation().getDefaultGraphicsAllocation(),
                                                                 context->getDevice(0)->getGmmHelper(), false, GMM_NO_PLANE);

    EXPECT_EQ(platform(), nullptr);
    EXPECT_EQ(mockCompressionFormat, rss.getCompressionFormat());
    EXPECT_EQ(expectedGetSurfaceStateCompressionFormatCalled, gmmClientContext->getSurfaceStateCompressionFormatCalled);
    EXPECT_EQ(expectedGetMediaSurfaceStateCompressionFormatCalled, gmmClientContext->getMediaSurfaceStateCompressionFormatCalled);
}

HWTEST2_F(XeHPAndLaterImageTests, givenMcsAllocationWhenAuxCapableIsNotSetThenProgramAuxBaseAddressAnyway, IsAtLeastXe2HpgCore) {
    MockContext context;
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context.getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelperUlt<>::create(&context, &imgDesc));

    auto pClDevice = context.getDevice(0);

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    mcsAlloc->setDefaultGmm(new Gmm(pClDevice->getRootDeviceEnvironment().getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements));
    surfaceState.setSurfaceBaseAddress(0xABCDEF1000);
    imageHw->setMcsSurfaceInfo(msi);
    imageHw->setMcsAllocation(mcsAlloc);
    auto mockResource = static_cast<MockGmmResourceInfo *>(mcsAlloc->getDefaultGmm()->gmmResourceInfo.get());
    mockResource->setMultisampleControlSurface();

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());

    imageHw->setAuxParamsForMultisamples(&surfaceState, pClDevice->getRootDeviceIndex());

    EXPECT_NE(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
}
