/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/api.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/d3d/cl_d3d_api.h"
#include "runtime/sharings/d3d/d3d_buffer.h"
#include "runtime/sharings/d3d/d3d_sharing.h"
#include "runtime/sharings/d3d/d3d_surface.h"
#include "runtime/sharings/d3d/d3d_texture.h"
#include "runtime/utilities/arrayref.h"
#include "unit_tests/fixtures/d3d_test_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace NEO {
template <typename T>
class D3DAuxTests : public D3DTests<T> {};
TYPED_TEST_CASE_P(D3DAuxTests);

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    mockMM->mapAuxGpuVaRetValue = false;
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable());

    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(0u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given2dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed) {
    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture2dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture2dDesc));

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, (D3DTexture2d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    mockMM->mapAuxGpuVaRetValue = false;
    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable());

    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(0u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isRenderCompressed);
}

TYPED_TEST_P(D3DAuxTests, given3dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed) {
    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    EXPECT_CALL(*this->mockSharingFcns, getTexture3dDesc(_, _)).Times(1).WillOnce(SetArgPointee<0>(this->mockSharingFcns->mockTexture3dDesc));

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, (D3DTexture3d *)&this->dummyD3DTexture, CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(1u, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isRenderCompressed);
}

REGISTER_TYPED_TEST_CASE_P(D3DAuxTests,
                           given2dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed,
                           given2dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed,
                           given2dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable,
                           given2dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed,
                           given3dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsRenderCompressed,
                           given3dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsRenderCompressed,
                           given3dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable,
                           given3dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetRenderCompressed);

INSTANTIATE_TYPED_TEST_CASE_P(D3DSharingTests, D3DAuxTests, D3DTypes);

} // namespace NEO