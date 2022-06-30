/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include <level_zero/ze_api.h>

namespace L0 {
enum class SamplerPatchValues : uint32_t {
    DefaultSampler = 0x00,
    AddressNone = 0x00,
    AddressClampToBorder = 0x01,
    AddressClampToEdge = 0x02,
    AddressRepeat = 0x03,
    AddressMirroredRepeat = 0x04,
    AddressMirroredRepeat101 = 0x05,
    NormalizedCoordsFalse = 0x00,
    NormalizedCoordsTrue = 0x08
};

inline SamplerPatchValues getAddrMode(ze_sampler_address_mode_t addressingMode) {
    switch (addressingMode) {
    case ZE_SAMPLER_ADDRESS_MODE_REPEAT:
        return SamplerPatchValues::AddressRepeat;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
        return SamplerPatchValues::AddressClampToBorder;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP:
        return SamplerPatchValues::AddressClampToEdge;
    case ZE_SAMPLER_ADDRESS_MODE_NONE:
        return SamplerPatchValues::AddressNone;
    case ZE_SAMPLER_ADDRESS_MODE_MIRROR:
        return SamplerPatchValues::AddressMirroredRepeat;
    default:
        DEBUG_BREAK_IF(true);
    }
    return SamplerPatchValues::AddressNone;
}
} // namespace L0