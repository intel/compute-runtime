/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper_base.inl"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release1201;

template <>
bool CompilerReleaseHelperHw<release>::isForceEmuInt32DivRemSPRequired() const {
    return true;
}

} // namespace NEO

template class NEO::CompilerReleaseHelperHw<NEO::release>;
