/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/image/image_surface_state_fixture.h"

using namespace NEO;

using ImageSurfaceStateTestsXeHpcCore = ImageSurfaceStateTests;

XE_HPC_CORETEST_F(ImageSurfaceStateTestsXeHpcCore, givenGmmWithMediaCompressedWhenSetFlagsForMediaCompressionThenAuxiliarySurfaceNoneIsSetAndMemoryCompressionEnable) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());
    castSurfaceState->setAuxiliarySurfaceMode(FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);

    mockGmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = false;
    EncodeSurfaceState<FamilyType>::setFlagsForMediaCompression(castSurfaceState, mockGmm.get());
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    EXPECT_EQ(castSurfaceState->getMemoryCompressionEnable(), false);
    mockGmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed = true;
    EncodeSurfaceState<FamilyType>::setFlagsForMediaCompression(castSurfaceState, mockGmm.get());
    EXPECT_EQ(castSurfaceState->getAuxiliarySurfaceMode(), FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    EXPECT_EQ(castSurfaceState->getMemoryCompressionEnable(), true);
}

XE_HPC_CORETEST_F(ImageSurfaceStateTestsXeHpcCore, givenGmmWhenSetClearColorParamsThenClearValueAddressEnable) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    mockGmm->gmmResourceInfo->getResourceFlags()->Gpu.IndirectClearColor = true;
    EncodeSurfaceState<FamilyType>::setClearColorParams(castSurfaceState, mockGmm.get());
    EXPECT_EQ(castSurfaceState->getClearValueAddressEnable(), true);
}

XE_HPC_CORETEST_F(ImageSurfaceStateTestsXeHpcCore, givenGmmWithMediaCompressedWhenSetMipTailStartLodThenMipTailStartLodIsSet) {
    auto size = sizeof(typename FamilyType::RENDER_SURFACE_STATE);
    auto surfaceState = std::make_unique<char[]>(size);
    auto castSurfaceState = reinterpret_cast<typename FamilyType::RENDER_SURFACE_STATE *>(surfaceState.get());

    ImageSurfaceStateHelper<FamilyType>::setMipTailStartLOD(castSurfaceState, nullptr);

    EXPECT_EQ(castSurfaceState->getMipTailStartLOD(), 0u);

    ImageSurfaceStateHelper<FamilyType>::setMipTailStartLOD(castSurfaceState, mockGmm.get());

    EXPECT_EQ(castSurfaceState->getMipTailStartLOD(), mockGmm->gmmResourceInfo->getMipTailStartLODSurfaceState());
}

XE_HPC_CORETEST_F(ImageSurfaceStateTestsXeHpcCore, givenSurfaceStateAndNullptrReleaseHelperWhenAuxParamsForMCSCCSAreSetThenCorrectAuxModeIsSet) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;

    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&surfaceState, nullptr);

    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), EncodeSurfaceState<FamilyType>::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
}

XE_HPC_CORETEST_F(ImageSurfaceStateTestsXeHpcCore, givenSurfaceStateAndAuxSurfaceModeOverrideRequiredIsFalseWhenAuxParamsForMCSCCSAreSetThenCorrectAuxModeIsSet) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isAuxSurfaceModeOverrideRequiredResult = false;

    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&surfaceState, releaseHelper.get());

    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), EncodeSurfaceState<FamilyType>::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_MCS_LCE);
}

XE_HPC_CORETEST_F(ImageSurfaceStateTestsXeHpcCore, givenSurfaceStateAndAuxSurfaceModeOverrideRequiredIsTrueWhenAuxParamsForMCSCCSAreSetThenCorrectAuxModeIsSet) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isAuxSurfaceModeOverrideRequiredResult = true;

    EncodeSurfaceState<FamilyType>::setAuxParamsForMCSCCS(&surfaceState, releaseHelper.get());

    EXPECT_EQ(surfaceState.getAuxiliarySurfaceMode(), EncodeSurfaceState<FamilyType>::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
}
