/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/numeric.h"

#include <algorithm>

namespace NEO {

template <typename GfxFamily>
void SamplerHw<GfxFamily>::setArg(void *memory) {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(memory);
    samplerState->setNonNormalizedCoordinateEnable(!this->normalizedCoordinates);

    auto addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
    auto addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
    auto addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;

    switch (this->addressingMode) {
    case CL_ADDRESS_NONE:
    case CL_ADDRESS_CLAMP:
        addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
        addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
        addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
        break;
    case CL_ADDRESS_CLAMP_TO_EDGE:
        addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        break;
    case CL_ADDRESS_MIRRORED_REPEAT:
        addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        break;
    case CL_ADDRESS_REPEAT:
        addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        break;
    }

    auto minMode = SAMPLER_STATE::MIN_MODE_FILTER_NEAREST;
    auto magMode = SAMPLER_STATE::MAG_MODE_FILTER_NEAREST;
    auto mipMode = SAMPLER_STATE::MIP_MODE_FILTER_NEAREST;

    if (CL_FILTER_LINEAR == filterMode) {
        minMode = SAMPLER_STATE::MIN_MODE_FILTER_LINEAR;
        magMode = SAMPLER_STATE::MAG_MODE_FILTER_LINEAR;
    }

    if (CL_FILTER_LINEAR == mipFilterMode) {
        mipMode = SAMPLER_STATE::MIP_MODE_FILTER_LINEAR;
    }

    samplerState->setMinModeFilter(minMode);
    samplerState->setMagModeFilter(magMode);
    samplerState->setMipModeFilter(mipMode);
    samplerState->setTcxAddressControlMode(addressControlModeX);
    samplerState->setTcyAddressControlMode(addressControlModeY);
    samplerState->setTczAddressControlMode(addressControlModeZ);

    if (CL_FILTER_NEAREST != filterMode) {
        samplerState->setRAddressMinFilterRoundingEnable(true);
        samplerState->setRAddressMagFilterRoundingEnable(true);
        samplerState->setVAddressMinFilterRoundingEnable(true);
        samplerState->setVAddressMagFilterRoundingEnable(true);
        samplerState->setUAddressMinFilterRoundingEnable(true);
        samplerState->setUAddressMagFilterRoundingEnable(true);
    } else {
        samplerState->setRAddressMinFilterRoundingEnable(false);
        samplerState->setRAddressMagFilterRoundingEnable(false);
        samplerState->setVAddressMinFilterRoundingEnable(false);
        samplerState->setVAddressMagFilterRoundingEnable(false);
        samplerState->setUAddressMinFilterRoundingEnable(false);
        samplerState->setUAddressMagFilterRoundingEnable(false);
    }

    FixedU4D8 minLodValue = FixedU4D8(std::min(getGenSamplerMaxLod(), this->lodMin));
    FixedU4D8 maxLodValue = FixedU4D8(std::min(getGenSamplerMaxLod(), this->lodMax));
    samplerState->setMinLod(minLodValue.getRawAccess());
    samplerState->setMaxLod(maxLodValue.getRawAccess());

    appendSamplerStateParams(samplerState);
}

template <typename GfxFamily>
void SamplerHw<GfxFamily>::appendSamplerStateParams(typename GfxFamily::SAMPLER_STATE *state) {
}

template <typename GfxFamily>
size_t SamplerHw<GfxFamily>::getSamplerStateSize() {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
    return sizeof(SAMPLER_STATE);
}
} // namespace NEO
