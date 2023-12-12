/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include <level_zero/ze_api.h>

namespace L0 {
enum class SamplerPatchValues : uint32_t {
    addressNone = 0x00,
    addressClampToBorder = 0x01,
    addressClampToEdge = 0x02,
    addressRepeat = 0x03,
    addressMirroredRepeat = 0x04,
    addressMirroredRepeat101 = 0x05,
    normalizedCoordsFalse = 0x00,
    normalizedCoordsTrue = 0x08
};

inline SamplerPatchValues getAddrMode(ze_sampler_address_mode_t addressingMode) {
    switch (addressingMode) {
    case ZE_SAMPLER_ADDRESS_MODE_REPEAT:
        return SamplerPatchValues::addressRepeat;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
        return SamplerPatchValues::addressClampToBorder;
    case ZE_SAMPLER_ADDRESS_MODE_CLAMP:
        return SamplerPatchValues::addressClampToEdge;
    case ZE_SAMPLER_ADDRESS_MODE_NONE:
        return SamplerPatchValues::addressNone;
    case ZE_SAMPLER_ADDRESS_MODE_MIRROR:
        return SamplerPatchValues::addressMirroredRepeat;
    default:
        DEBUG_BREAK_IF(true);
    }
    return SamplerPatchValues::addressNone;
}
} // namespace L0
