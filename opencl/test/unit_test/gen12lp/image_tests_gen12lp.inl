/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <functional>

using namespace NEO;

typedef ::testing::Test gen12LpImageTests;

GEN12LPTEST_F(gen12LpImageTests, WhenAppendingSurfaceStateParamsThenSurfaceStateDoesNotChange) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = FamilyType::cmdInitRenderSurfaceState;
    auto surfaceStateAfter = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->appendSurfaceStateParams(&surfaceStateAfter, context.getDevice(0)->getRootDeviceIndex(), false);

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));
}

GEN12LPTEST_F(ImageClearColorFixture, givenImageForGen12LpWhenClearColorParametersAreSetThenClearColorSurfaceInSurfaceStateIsSet) {
    this->setUpImpl<FamilyType>();
    auto surfaceState = this->getSurfaceState<FamilyType>();

    surfaceState.setSurfaceBaseAddress(0xABCDEF1000);

    EXPECT_EQ(false, surfaceState.getClearValueAddressEnable());
    EXPECT_EQ(0u, surfaceState.getClearColorAddress());
    EXPECT_EQ(0u, surfaceState.getClearColorAddressHigh());

    std::unique_ptr<ImageHw<FamilyType>> imageHw(static_cast<ImageHw<FamilyType> *>(ImageHelper<Image2dDefaults>::create(&context)));
    auto gmm = imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm();
    gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = 1;
    EncodeSurfaceState<FamilyType>::setClearColorParams(&surfaceState, gmm);

    EXPECT_EQ(true, surfaceState.getClearValueAddressEnable());
    EXPECT_NE(0u, surfaceState.getClearColorAddress());
    EXPECT_NE(0u, surfaceState.getClearColorAddressHigh());
}

GEN12LPTEST_F(ImageClearColorFixture, givenImageForGen12LpWhenCanonicalAddresForClearColorIsUsedThenItsConvertedToNonCanonicalForm) {
    this->setUpImpl<FamilyType>();
    auto surfaceState = this->getSurfaceState<FamilyType>();

    uint64_t canonicalAddress = 0xffffABCDABCDE000;

    EXPECT_THROW(surfaceState.setClearColorAddressHigh(static_cast<uint32_t>(canonicalAddress >> 32)), std::exception);
    surfaceState.setSurfaceBaseAddress(canonicalAddress);

    std::unique_ptr<ImageHw<FamilyType>> imageHw(static_cast<ImageHw<FamilyType> *>(ImageHelper<Image2dDefaults>::create(&context)));
    auto gmm = imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm();
    gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = 1;
    EXPECT_NO_THROW(EncodeSurfaceState<FamilyType>::setClearColorParams(&surfaceState, gmm));

    uint64_t nonCanonicalAddress = ((static_cast<uint64_t>(surfaceState.getClearColorAddressHigh()) << 32) | surfaceState.getClearColorAddress());
    auto gmmHelper = context.getDevice(0)->getGmmHelper();
    EXPECT_EQ(gmmHelper->decanonize(canonicalAddress), nonCanonicalAddress);
}

GEN12LPTEST_F(ImageClearColorFixture, givenMcsAllocationWhenSetArgIsCalledWithUnifiedAuxCapabilityAndMCSThenProgramAuxFieldsForCcs) {
    this->setUpImpl<FamilyType>();

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    std::unique_ptr<MockContext> context(new MockContext());
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{context->getDevice(0)->getRootDeviceIndex(), MemoryConstants::pageSize});

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    mcsAlloc->setDefaultGmm(new Gmm(context->getDevice(0)->getGmmHelper(), nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));
    surfaceState.setSurfaceBaseAddress(0xABCDEF1000);
    imageHw->setMcsSurfaceInfo(msi);
    imageHw->setMcsAllocation(mcsAlloc);
    auto mockResource = static_cast<MockGmmResourceInfo *>(mcsAlloc->getDefaultGmm()->gmmResourceInfo.get());
    mockResource->setUnifiedAuxTranslationCapable();
    mockResource->setMultisampleControlSurface();

    EXPECT_EQ(0u, surfaceState.getAuxiliarySurfaceBaseAddress());

    imageHw->setAuxParamsForMultisamples(&surfaceState);

    EXPECT_NE(0u, surfaceState.getAuxiliarySurfaceBaseAddress());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
}

GEN12LPTEST_F(gen12LpImageTests, givenCompressionThenSurfaceStateParamsAreSetForCompression) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = true;
    EncodeSurfaceState<FamilyType>::setImageAuxParamsForCCS(&surfaceState,
                                                            imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm());

    EXPECT_FALSE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

GEN12LPTEST_F(gen12LpImageTests, givenNoCompressionWhenProgramingImageSurfaceStateThenCompressionIsDisabled) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    surfaceState.setMemoryCompressionEnable(true);
    surfaceState.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled = false;
    imageHw->setImageArg(&surfaceState, false, 0, 0, false);

    EXPECT_FALSE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
}

GEN12LPTEST_F(gen12LpImageTests, givenMediaCompressionThenSurfaceStateParamsAreSetForMediaCompression) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    surfaceState.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    EncodeSurfaceState<FamilyType>::setImageAuxParamsForCCS(&surfaceState,
                                                            imageHw->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm());

    EXPECT_TRUE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
}
