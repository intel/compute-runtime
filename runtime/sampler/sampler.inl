/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/utilities/numeric.h"

#include <algorithm>

namespace OCLRT {

template <typename GfxFamily>
void SamplerHw<GfxFamily>::setArg(void *memory) {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;
    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(memory);
    samplerState->setNonNormalizedCoordinateEnable(!this->normalizedCoordinates);

    auto addressControlModeX = SAMPLER_STATE::TCX_ADDRESS_CONTROL_MODE_CLAMP;
    auto addressControlModeY = SAMPLER_STATE::TCY_ADDRESS_CONTROL_MODE_CLAMP;
    auto addressControlModeZ = SAMPLER_STATE::TCZ_ADDRESS_CONTROL_MODE_CLAMP;

    // clang-format off
    switch (this->addressingMode) {
    case CL_ADDRESS_NONE: 
    case CL_ADDRESS_CLAMP:
        addressControlModeX = SAMPLER_STATE::TCX_ADDRESS_CONTROL_MODE_CLAMP_BORDER; 
        addressControlModeY = SAMPLER_STATE::TCY_ADDRESS_CONTROL_MODE_CLAMP_BORDER; 
        addressControlModeZ = SAMPLER_STATE::TCZ_ADDRESS_CONTROL_MODE_CLAMP_BORDER; 
        break;
    case CL_ADDRESS_CLAMP_TO_EDGE:
        addressControlModeX = SAMPLER_STATE::TCX_ADDRESS_CONTROL_MODE_CLAMP;        
        addressControlModeY = SAMPLER_STATE::TCY_ADDRESS_CONTROL_MODE_CLAMP;        
        addressControlModeZ = SAMPLER_STATE::TCZ_ADDRESS_CONTROL_MODE_CLAMP;        
        break;
    case CL_ADDRESS_MIRRORED_REPEAT: 
        addressControlModeX = SAMPLER_STATE::TCX_ADDRESS_CONTROL_MODE_MIRROR;       
        addressControlModeY = SAMPLER_STATE::TCY_ADDRESS_CONTROL_MODE_MIRROR;       
        addressControlModeZ = SAMPLER_STATE::TCZ_ADDRESS_CONTROL_MODE_MIRROR;       
        break;
    case CL_ADDRESS_REPEAT:
        addressControlModeX = SAMPLER_STATE::TCX_ADDRESS_CONTROL_MODE_WRAP;
        addressControlModeY = SAMPLER_STATE::TCY_ADDRESS_CONTROL_MODE_WRAP;
        addressControlModeZ = SAMPLER_STATE::TCZ_ADDRESS_CONTROL_MODE_WRAP;
        break;
    }
    // clang-format on

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
} // namespace OCLRT
