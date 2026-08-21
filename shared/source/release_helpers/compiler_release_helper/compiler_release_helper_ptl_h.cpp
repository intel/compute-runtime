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
constexpr auto release = ReleaseType::release3000;

template <>
bool CompilerReleaseHelperHw<release>::getFtrXe2Compression() const {
    return !(hardwareIpVersion.value == AOT::PTL_H_A0);
}

} // namespace NEO

template class NEO::CompilerReleaseHelperHw<NEO::release>;
