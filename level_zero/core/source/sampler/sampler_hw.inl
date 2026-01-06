/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/numeric.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/sampler/sampler_hw.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t SamplerCoreFamily<gfxCoreFamily>::initialize(Device *device, const ze_sampler_desc_t *desc) {
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    BaseClass::initialize(device, desc);

    samplerState.setNonNormalizedCoordinateEnable(!desc->isNormalized);

    samplerState.setLodPreclampMode(SAMPLER_STATE::LOD_PRECLAMP_MODE::LOD_PRECLAMP_MODE_OGL);

    auto addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
    auto addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;
    auto addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP_BORDER;

    switch (desc->addressMode) {
    case ZE_SAMPLER_ADDRESS_MODE_NONE:
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
        break;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP:
        addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_CLAMP;
        break;
    case ZE_SAMPLER_ADDRESS_MODE_MIRROR:
        addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR;
        break;
    case ZE_SAMPLER_ADDRESS_MODE_REPEAT:
        addressControlModeX = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        addressControlModeY = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        addressControlModeZ = SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP;
        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto minMode = SAMPLER_STATE::MIN_MODE_FILTER_NEAREST;
    auto magMode = SAMPLER_STATE::MAG_MODE_FILTER_NEAREST;
    auto mipMode = SAMPLER_STATE::MIP_MODE_FILTER_NEAREST;

    auto rAddrMinFilterRounding = false;
    auto rAddrMagFilterRounding = false;
    auto vAddrMinFilterRounding = false;
    auto vAddrMagFilterRounding = false;
    auto uAddrMinFilterRounding = false;
    auto uAddrMagFilterRounding = false;

    switch (desc->filterMode) {
    case ZE_SAMPLER_FILTER_MODE_NEAREST:
        minMode = SAMPLER_STATE::MIN_MODE_FILTER_NEAREST;
        magMode = SAMPLER_STATE::MAG_MODE_FILTER_NEAREST;
        mipMode = SAMPLER_STATE::MIP_MODE_FILTER_NEAREST;

        break;
    case ZE_SAMPLER_FILTER_MODE_LINEAR:
        minMode = SAMPLER_STATE::MIN_MODE_FILTER_LINEAR;
        magMode = SAMPLER_STATE::MAG_MODE_FILTER_LINEAR;
        mipMode = SAMPLER_STATE::MIP_MODE_FILTER_NEAREST;

        rAddrMinFilterRounding = true;
        rAddrMagFilterRounding = true;
        vAddrMinFilterRounding = true;
        vAddrMagFilterRounding = true;
        uAddrMinFilterRounding = true;
        uAddrMagFilterRounding = true;

        break;
    default:
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    samplerState.setMinModeFilter(minMode);
    samplerState.setMagModeFilter(magMode);
    samplerState.setMipModeFilter(mipMode);

    samplerState.setRAddressMinFilterRoundingEnable(rAddrMinFilterRounding);
    samplerState.setRAddressMagFilterRoundingEnable(rAddrMagFilterRounding);
    samplerState.setVAddressMinFilterRoundingEnable(vAddrMinFilterRounding);
    samplerState.setVAddressMagFilterRoundingEnable(vAddrMagFilterRounding);
    samplerState.setUAddressMinFilterRoundingEnable(uAddrMinFilterRounding);
    samplerState.setUAddressMagFilterRoundingEnable(uAddrMagFilterRounding);

    samplerState.setTcxAddressControlMode(addressControlModeX);
    samplerState.setTcyAddressControlMode(addressControlModeY);
    samplerState.setTczAddressControlMode(addressControlModeZ);

    NEO::FixedU4D8 minLodValue =
        NEO::FixedU4D8(std::min(getGenSamplerMaxLod(), this->lodMin));
    NEO::FixedU4D8 maxLodValue =
        NEO::FixedU4D8(std::min(getGenSamplerMaxLod(), this->lodMax));
    samplerState.setMinLod(minLodValue.getRawAccess());
    samplerState.setMaxLod(maxLodValue.getRawAccess());

    auto &hwInfo = device->getHwInfo();
    auto &helper = device->getNEODevice()->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
    helper.adjustSamplerState(&samplerState, hwInfo);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void SamplerCoreFamily<gfxCoreFamily>::copySamplerStateToDSH(ArrayRef<uint8_t> dynamicStateHeap,
                                                             const uint32_t samplerOffset) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using SAMPLER_STATE = typename GfxFamily::SAMPLER_STATE;

    auto freeSpace = dynamicStateHeap.size() - samplerOffset;
    memcpy_s(&dynamicStateHeap[samplerOffset], freeSpace, &samplerState, sizeof(SAMPLER_STATE));
}

} // namespace L0
