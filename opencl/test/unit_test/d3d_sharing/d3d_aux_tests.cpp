/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/utilities/arrayref.h"

#include "opencl/source/api/api.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/d3d/cl_d3d_api.h"
#include "opencl/source/sharings/d3d/d3d_buffer.h"
#include "opencl/source/sharings/d3d/d3d_sharing.h"
#include "opencl/source/sharings/d3d/d3d_surface.h"
#include "opencl/source/sharings/d3d/d3d_texture.h"
#include "opencl/test/unit_test/fixtures/d3d_test_fixture.h"

#include "gtest/gtest.h"

namespace NEO {
template <typename T>
class D3DAuxTests : public D3DTests<T> {};
TYPED_TEST_CASE_P(D3DAuxTests);

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsCompressed) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t expectedMapAuxGpuVaCalls = hwInfoConfig.isPageTableManagerSupported(hwInfo) ? 1 : 0;

    EXPECT_EQ(expectedMapAuxGpuVaCalls, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isCompressionEnabled);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsCompressed) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    mockMM->mapAuxGpuVaRetValue = false;
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t expectedMapAuxGpuVaCalls = hwInfoConfig.isPageTableManagerSupported(hwInfo) ? 1 : 0;

    EXPECT_EQ(expectedMapAuxGpuVaCalls, mockMM->mapAuxGpuVACalled);
    EXPECT_EQ(!hwInfoConfig.isPageTableManagerSupported(hwInfo), gmm->isCompressionEnabled);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DAuxTests, given2dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable) {
    this->mockSharingFcns->mockTexture2dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;
    this->mockSharingFcns->mockTexture2dDesc.ArraySize = 4;
    this->mockSharingFcns->mockTexture2dDesc.MipLevels = 4;

    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable());

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 4, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(0u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isCompressionEnabled);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DAuxTests, given2dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetCompressed) {
    mockGmmResInfo->setUnifiedAuxTranslationCapable();

    this->mockSharingFcns->getTexture2dDescSetParams = true;
    this->mockSharingFcns->getTexture2dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture2dDesc;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    auto image = std::unique_ptr<Image>(D3DTexture<TypeParam>::create2d(this->context, reinterpret_cast<D3DTexture2d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t expectedMapAuxGpuVaCalls = hwInfoConfig.isPageTableManagerSupported(hwInfo) ? 1 : 0;

    EXPECT_EQ(expectedMapAuxGpuVaCalls, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isCompressionEnabled);

    EXPECT_EQ(1u, this->mockSharingFcns->getTexture2dDescCalled);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsCompressed) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();

    this->mockSharingFcns->getTexture3dDescSetParams = true;
    this->mockSharingFcns->getTexture3dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture3dDesc;

    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, reinterpret_cast<D3DTexture3d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t expectedMapAuxGpuVaCalls = hwInfoConfig.isPageTableManagerSupported(hwInfo) ? 1 : 0;

    EXPECT_EQ(expectedMapAuxGpuVaCalls, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isCompressionEnabled);
    EXPECT_EQ(1u, this->mockSharingFcns->getTexture3dDescCalled);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsCompressed) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();

    this->mockSharingFcns->getTexture3dDescSetParams = true;
    this->mockSharingFcns->getTexture3dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture3dDesc;

    mockMM->mapAuxGpuVaRetValue = false;
    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, reinterpret_cast<D3DTexture3d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t expectedMapAuxGpuVaCalls = hwInfoConfig.isPageTableManagerSupported(hwInfo) ? 1 : 0;

    EXPECT_EQ(expectedMapAuxGpuVaCalls, mockMM->mapAuxGpuVACalled);
    EXPECT_EQ(!hwInfoConfig.isPageTableManagerSupported(hwInfo), gmm->isCompressionEnabled);
    EXPECT_EQ(1u, this->mockSharingFcns->getTexture3dDescCalled);
}

TYPED_TEST_P(D3DAuxTests, given3dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable) {
    this->mockSharingFcns->mockTexture3dDesc.MiscFlags = D3DResourceFlags::MISC_SHARED;

    EXPECT_FALSE(gmm->unifiedAuxTranslationCapable());

    this->mockSharingFcns->getTexture3dDescSetParams = true;
    this->mockSharingFcns->getTexture3dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture3dDesc;

    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, reinterpret_cast<D3DTexture3d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    EXPECT_EQ(0u, mockMM->mapAuxGpuVACalled);
    EXPECT_FALSE(gmm->isCompressionEnabled);
    EXPECT_EQ(1u, this->mockSharingFcns->getTexture3dDescCalled);
}

TYPED_TEST_P(D3DAuxTests, given3dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetCompressed) {
    mockGmmResInfo->setUnifiedAuxTranslationCapable();

    this->mockSharingFcns->getTexture3dDescSetParams = true;
    this->mockSharingFcns->getTexture3dDescParamsSet.textureDesc = this->mockSharingFcns->mockTexture3dDesc;

    mockGmmResInfo->setUnifiedAuxTranslationCapable();
    std::unique_ptr<Image> image(D3DTexture<TypeParam>::create3d(this->context, reinterpret_cast<D3DTexture3d *>(&this->dummyD3DTexture), CL_MEM_READ_WRITE, 1, nullptr));
    ASSERT_NE(nullptr, image.get());

    const auto &hwInfo = context->getDevice(0)->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    uint32_t expectedMapAuxGpuVaCalls = hwInfoConfig.isPageTableManagerSupported(hwInfo) ? 1 : 0;

    EXPECT_EQ(expectedMapAuxGpuVaCalls, mockMM->mapAuxGpuVACalled);
    EXPECT_TRUE(gmm->isCompressionEnabled);
    EXPECT_EQ(1u, this->mockSharingFcns->getTexture3dDescCalled);
}

REGISTER_TYPED_TEST_CASE_P(D3DAuxTests,
                           given2dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsCompressed,
                           given2dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsCompressed,
                           given2dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable,
                           given2dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetCompressed,
                           given3dSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetAsCompressed,
                           given3dSharableTextureWithUnifiedAuxFlagsWhenFailOnAuxMappingThenDontSetAsCompressed,
                           given3dSharableTextureWithoutUnifiedAuxFlagsWhenCreatingThenDontMapAuxTable,
                           given3dNonSharableTextureWithUnifiedAuxFlagsWhenCreatingThenMapAuxTableAndSetCompressed);

INSTANTIATE_TYPED_TEST_CASE_P(D3DSharingTests, D3DAuxTests, D3DTypes);

} // namespace NEO
