/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper_base.inl"

#include "neo_aot_platforms.h"
#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release3510;

template <>
bool CompilerReleaseHelperHw<release>::isAvailableSemaphore64Base() const {
    return static_cast<bool>(hardwareIpVersion.value != AOT::NVL_P_A0);
}

} // namespace NEO
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper_common_xe3p_and_later.inl"

template class NEO::CompilerReleaseHelperHw<NEO::release>;
