/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"

#include "test.h"

using namespace NEO;

using XeHPPlusHardwareCommandsTest = testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPPlusHardwareCommandsTest, givenXeHPPlusPlatformWhenDoBindingTablePrefetchIsCalledThenReturnsFalse) {
    EXPECT_FALSE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPPlusHardwareCommandsTest, GivenXeHPPlusPlatformWhenSetCoherencyTypeIsCalledThenOnlyEncodingSupportedIsSingleGpuCoherent) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using COHERENCY_TYPE = typename RENDER_SURFACE_STATE::COHERENCY_TYPE;

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    for (COHERENCY_TYPE coherencyType : {RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT, RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT}) {
        EncodeSurfaceState<FamilyType>::setCoherencyType(&surfaceState, coherencyType);
        EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, surfaceState.getCoherencyType());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPPlusHardwareCommandsTest, givenXeHPPlusPlatformWhenGetAdditionalPipelineSelectSizeIsCalledThenZeroIsReturned) {
    MockDevice device;
    EXPECT_EQ(0u, EncodeWA<FamilyType>::getAdditionalPipelineSelectSize(device));
}

using XeHPPlusCommandEncoderTest = Test<DeviceFixture>;

HWTEST2_F(XeHPPlusCommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsAtLeastXeHpCore) {
    auto container = CommandContainer();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 104ul);
}

HWTEST2_F(XeHPPlusCommandEncoderTest, givenCommandContainerWithDirtyHeapWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsAtLeastXeHpCore) {
    auto container = CommandContainer();
    container.setHeapDirty(HeapType::SURFACE_STATE);
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 104ul);
}
