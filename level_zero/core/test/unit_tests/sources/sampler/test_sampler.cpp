/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/numeric.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_sampler.h"

namespace L0 {
namespace ult {

const auto samplerAddressMode = ::testing::Values(
    ZE_SAMPLER_ADDRESS_MODE_NONE,
    ZE_SAMPLER_ADDRESS_MODE_REPEAT,
    ZE_SAMPLER_ADDRESS_MODE_CLAMP,
    ZE_SAMPLER_ADDRESS_MODE_MIRROR,
    ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

const auto samplerFilterMode = ::testing::Values(
    ZE_SAMPLER_FILTER_MODE_NEAREST,
    ZE_SAMPLER_FILTER_MODE_LINEAR);

const auto samplerIsNormalized = ::testing::Values(
    true,
    false);

using SamplerCreateSupport = IsGen12LP;

class SamplerCreateTest
    : public Test<DeviceFixture>,
      public ::testing::WithParamInterface<std::tuple<ze_sampler_address_mode_t,
                                                      ze_sampler_filter_mode_t,
                                                      ze_bool_t>> {
  public:
    void SetUp() override {
        Test<DeviceFixture>::SetUp();
    }
    void TearDown() override {
        Test<DeviceFixture>::TearDown();
    }
};

HWTEST2_P(SamplerCreateTest, givenDifferentDescriptorValuesThenSamplerIsCorrectlyCreated, SamplerCreateSupport) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    ze_sampler_address_mode_t addressMode = std::get<0>(GetParam());
    ze_sampler_filter_mode_t filterMode = std::get<1>(GetParam());
    ze_bool_t isNormalized = std::get<2>(GetParam());

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    auto sampler = new MockSamplerHw<FamilyType::gfxCoreFamily>();
    EXPECT_NE(nullptr, sampler);

    sampler->initialize(device, &desc);

    EXPECT_EQ(SAMPLER_STATE::LOD_PRECLAMP_MODE::LOD_PRECLAMP_MODE_OGL, sampler->samplerState.getLodPreclampMode());

    if (isNormalized == static_cast<ze_bool_t>(true)) {
        EXPECT_FALSE(sampler->samplerState.getNonNormalizedCoordinateEnable());
    } else {
        EXPECT_TRUE(sampler->samplerState.getNonNormalizedCoordinateEnable());
    }

    if (addressMode == ZE_SAMPLER_ADDRESS_MODE_NONE) {
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER,
                  sampler->samplerState.getTcxAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER,
                  sampler->samplerState.getTcyAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER,
                  sampler->samplerState.getTczAddressControlMode());
    } else if (addressMode == ZE_SAMPLER_ADDRESS_MODE_REPEAT) {
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP,
                  sampler->samplerState.getTcxAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP,
                  sampler->samplerState.getTcyAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP,
                  sampler->samplerState.getTczAddressControlMode());
    } else if (addressMode == ZE_SAMPLER_ADDRESS_MODE_CLAMP) {
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP,
                  sampler->samplerState.getTcxAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP,
                  sampler->samplerState.getTcyAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP,
                  sampler->samplerState.getTczAddressControlMode());
    } else if (addressMode == ZE_SAMPLER_ADDRESS_MODE_MIRROR) {
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR,
                  sampler->samplerState.getTcxAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR,
                  sampler->samplerState.getTcyAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR,
                  sampler->samplerState.getTczAddressControlMode());
    } else if (addressMode == ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER) {
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER,
                  sampler->samplerState.getTcxAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER,
                  sampler->samplerState.getTcyAddressControlMode());
        EXPECT_EQ(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER,
                  sampler->samplerState.getTczAddressControlMode());
    }

    if (filterMode == ZE_SAMPLER_FILTER_MODE_NEAREST) {
        EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST,
                  sampler->samplerState.getMinModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_NEAREST,
                  sampler->samplerState.getMagModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NEAREST,
                  sampler->samplerState.getMipModeFilter());
        EXPECT_FALSE(sampler->samplerState.getRAddressMinFilterRoundingEnable());
        EXPECT_FALSE(sampler->samplerState.getRAddressMagFilterRoundingEnable());
        EXPECT_FALSE(sampler->samplerState.getVAddressMinFilterRoundingEnable());
        EXPECT_FALSE(sampler->samplerState.getVAddressMagFilterRoundingEnable());
        EXPECT_FALSE(sampler->samplerState.getUAddressMinFilterRoundingEnable());
        EXPECT_FALSE(sampler->samplerState.getUAddressMagFilterRoundingEnable());
    } else if (filterMode == ZE_SAMPLER_FILTER_MODE_LINEAR) {
        EXPECT_EQ(SAMPLER_STATE::MIN_MODE_FILTER_LINEAR,
                  sampler->samplerState.getMinModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MAG_MODE_FILTER_LINEAR,
                  sampler->samplerState.getMagModeFilter());
        EXPECT_EQ(SAMPLER_STATE::MIP_MODE_FILTER_NEAREST,
                  sampler->samplerState.getMipModeFilter());
        EXPECT_TRUE(sampler->samplerState.getRAddressMinFilterRoundingEnable());
        EXPECT_TRUE(sampler->samplerState.getRAddressMagFilterRoundingEnable());
        EXPECT_TRUE(sampler->samplerState.getVAddressMinFilterRoundingEnable());
        EXPECT_TRUE(sampler->samplerState.getVAddressMagFilterRoundingEnable());
        EXPECT_TRUE(sampler->samplerState.getUAddressMinFilterRoundingEnable());
        EXPECT_TRUE(sampler->samplerState.getUAddressMagFilterRoundingEnable());
    }

    NEO::FixedU4D8 minLodValue =
        NEO::FixedU4D8(std::min(sampler->getGenSamplerMaxLod(), sampler->lodMin));
    NEO::FixedU4D8 maxLodValue =
        NEO::FixedU4D8(std::min(sampler->getGenSamplerMaxLod(), sampler->lodMax));
    EXPECT_EQ(minLodValue.getRawAccess(), sampler->samplerState.getMinLod());
    EXPECT_EQ(maxLodValue.getRawAccess(), sampler->samplerState.getMaxLod());

    sampler->destroy();
}

INSTANTIATE_TEST_SUITE_P(SamplerDescCombinations, SamplerCreateTest,
                         ::testing::Combine(samplerAddressMode,
                                            samplerFilterMode,
                                            samplerIsNormalized));

using ContextCreateSamplerTest = Test<DeviceFixture>;

HWTEST2_F(ContextCreateSamplerTest, givenDifferentDescriptorValuesThenSamplerIsCorrectlyCreated, SamplerCreateSupport) {
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_LINEAR;
    ze_bool_t isNormalized = false;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    ze_sampler_handle_t hSampler;
    ze_result_t res = context->createSampler(device, &desc, &hSampler);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    auto sampler = reinterpret_cast<L0::SamplerImp *>(L0::Sampler::fromHandle(hSampler));
    EXPECT_NE(nullptr, sampler);

    sampler->destroy();
}

HWTEST2_F(ContextCreateSamplerTest, givenInvalidHardwareFamilyThenSamplerIsNotCreated, SamplerCreateSupport) {
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_LINEAR;
    ze_bool_t isNormalized = false;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    L0::Sampler *sampler = Sampler::create(IGFX_MAX_PRODUCT, device, &desc);

    EXPECT_EQ(nullptr, sampler);
}

HWTEST2_F(ContextCreateSamplerTest, givenInvalidAddressModeThenSamplerIsNotCreated, SamplerCreateSupport) {
    auto addressModeArray = std::make_unique<char[]>(sizeof(ze_sampler_address_mode_t));
    addressModeArray[0] = 99; // out of range value
    auto addressMode = *reinterpret_cast<ze_sampler_address_mode_t *>(addressModeArray.get());
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_LINEAR;
    ze_bool_t isNormalized = false;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    L0::Sampler *sampler = Sampler::create(FamilyType::gfxCoreFamily, device, &desc);

    EXPECT_EQ(nullptr, sampler);
}

HWTEST2_F(ContextCreateSamplerTest, givenInvalidFilterModeThenSamplerIsNotCreated, SamplerCreateSupport) {
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE;
    auto filterModeArray = std::make_unique<char[]>(sizeof(ze_sampler_filter_mode_t));
    filterModeArray[0] = 99; // out of range value
    ze_sampler_filter_mode_t filterMode = *reinterpret_cast<ze_sampler_filter_mode_t *>(filterModeArray.get());
    ze_bool_t isNormalized = false;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    L0::Sampler *sampler = Sampler::create(FamilyType::gfxCoreFamily, device, &desc);

    EXPECT_EQ(nullptr, sampler);
}

using SamplerInitTest = Test<DeviceFixture>;
HWTEST2_F(SamplerInitTest, givenValidHandleReturnUnitialized, IsXeHpcCore) {
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_NONE;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_LINEAR;
    ze_bool_t isNormalized = false;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    ze_sampler_handle_t hSampler;
    ze_result_t res = context->createSampler(device, &desc, &hSampler);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, res);
}

struct SupportsLowQualityFilterSampler {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return NEO::ToGfxCoreFamily<productFamily>::get() >= IGFX_GEN12LP_CORE && NEO::ToGfxCoreFamily<productFamily>::get() != IGFX_XE_HPC_CORE;
    }
};

HWTEST2_F(SamplerInitTest, whenInitializeSamplerAndForceSamplerLowFilteringPrecisionIsFalseThenLowQualityFilterIsDisabled, SupportsLowQualityFilterSampler) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    EXPECT_FALSE(debugManager.flags.ForceSamplerLowFilteringPrecision.get());
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_REPEAT;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST;
    ze_bool_t isNormalized = true;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    auto sampler = static_cast<MockSamplerHw<FamilyType::gfxCoreFamily> *>((*samplerFactory[productFamily])());
    sampler->initialize(device, &desc);

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, sampler->samplerState.getLowQualityFilter());

    sampler->destroy();
}

HWTEST2_F(SamplerInitTest, whenInitializeSamplerAndForceSamplerLowFilteringPrecisionIsTrueThenLowQualityFilterIsEnabled, SupportsLowQualityFilterSampler) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(debugManager.flags.ForceSamplerLowFilteringPrecision.get());
    ze_sampler_address_mode_t addressMode = ZE_SAMPLER_ADDRESS_MODE_REPEAT;
    ze_sampler_filter_mode_t filterMode = ZE_SAMPLER_FILTER_MODE_NEAREST;
    ze_bool_t isNormalized = true;

    ze_sampler_desc_t desc = {};
    desc.addressMode = addressMode;
    desc.filterMode = filterMode;
    desc.isNormalized = isNormalized;

    auto sampler = static_cast<MockSamplerHw<FamilyType::gfxCoreFamily> *>((*samplerFactory[productFamily])());
    sampler->initialize(device, &desc);

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, sampler->samplerState.getLowQualityFilter());

    sampler->destroy();
}

} // namespace ult
} // namespace L0
