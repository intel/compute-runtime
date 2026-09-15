/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release3510;
} // namespace NEO

#include "shared/source/release_helpers/release_helper/release_helper_common_xe3_and_later.inl"
#include "shared/source/release_helpers/release_helper/release_helper_common_xe3p.inl"
#include "shared/source/release_helpers/release_helper/release_helper_common_xe3p_lpg.inl"
#include "shared/source/release_helpers/release_helper/release_helper_preferred_slm_xe3p_igpu_192k.inl"

template <>
uint64_t NEO::ReleaseHelperHw<NEO::release>::overrideSystemMemoryPatIndexBase(uint64_t patIndex) const {
    return hardwareIpVersion.value == static_cast<uint32_t>(AOT::NVL_P_A0) ? 19u : patIndex;
}

template class NEO::ReleaseHelperHw<NEO::release>;
