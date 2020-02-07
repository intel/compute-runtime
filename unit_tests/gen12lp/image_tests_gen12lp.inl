/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "core/memory_manager/memory_manager.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/gen12lp/special_ult_helper_gen12lp.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"

#include "mock_gmm_client_context.h"

#include <functional>

using namespace NEO;

typedef ::testing::Test gen12LpImageTests;

GEN12LPTEST_F(gen12LpImageTests, appendSurfaceStateParamsDoesNothing) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image1dDefaults>::create(&context));
    auto surfaceStateBefore = FamilyType::cmdInitRenderSurfaceState;
    auto surfaceStateAfter = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());

    EXPECT_EQ(0, memcmp(&surfaceStateBefore, &surfaceStateAfter, sizeof(RENDER_SURFACE_STATE)));

    imageHw->appendSurfaceStateParams(&surfaceStateAfter);

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
    auto gmm = imageHw->getGraphicsAllocation()->getDefaultGmm();
    gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = 1;
    imageHw->setClearColorParams(&surfaceState, gmm);

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
    auto gmm = imageHw->getGraphicsAllocation()->getDefaultGmm();
    gmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = 1;
    EXPECT_NO_THROW(imageHw->setClearColorParams(&surfaceState, gmm));

    uint64_t nonCanonicalAddress = ((static_cast<uint64_t>(surfaceState.getClearColorAddressHigh()) << 32) | surfaceState.getClearColorAddress());
    EXPECT_EQ(GmmHelper::decanonize(canonicalAddress), nonCanonicalAddress);
}

GEN12LPTEST_F(ImageClearColorFixture, givenMcsAllocationWhenSetArgIsCalledWithUnifiedAuxCapabilityAndMCSThenProgramAuxFieldsForCcs) {
    this->setUpImpl<FamilyType>();

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    std::unique_ptr<MockContext> context(new MockContext());
    McsSurfaceInfo msi = {10, 20, 3};
    auto mcsAlloc = context->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelper<>::create(context.get(), &imgDesc));

    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    mcsAlloc->setDefaultGmm(new Gmm(context->getDevice(0)->getExecutionEnvironment()->getGmmClientContext(), nullptr, 1, false));
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

GEN12LPTEST_F(gen12LpImageTests, givenRenderCompressionSurfaceStateParamsAreSetForRenderCompression) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->getGraphicsAllocation()->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Info.RenderCompressed = true;
    imageHw->setAuxParamsForCCS(&surfaceState, imageHw->getGraphicsAllocation()->getDefaultGmm());

    EXPECT_FALSE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}

GEN12LPTEST_F(gen12LpImageTests, givenMediaCompressionSurfaceStateParamsAreSetForMediaCompression) {
    MockContext context;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    cl_image_desc imgDesc = Image2dDefaults::imageDesc;
    imgDesc.num_samples = 8;
    std::unique_ptr<Image> image(Image2dHelper<>::create(&context, &imgDesc));
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    imageHw->getGraphicsAllocation()->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    surfaceState.setAuxiliarySurfaceMode(RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    imageHw->setAuxParamsForCCS(&surfaceState, imageHw->getGraphicsAllocation()->getDefaultGmm());

    EXPECT_TRUE(surfaceState.getMemoryCompressionEnable());
    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
}

using Gen12lpRenderSurfaceStateDataTests = ::testing::Test;

GEN12LPTEST_F(Gen12lpRenderSurfaceStateDataTests, WhenMemoryObjectControlStateIndexToMocsTablesIsSetThenValueIsShift) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;

    uint32_t value = 4;
    surfaceState.setMemoryObjectControlStateIndexToMocsTables(value);

    EXPECT_EQ(surfaceState.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables, value >> 1);
    EXPECT_EQ(surfaceState.getMemoryObjectControlStateIndexToMocsTables(), value);
}
