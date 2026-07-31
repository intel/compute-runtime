/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper_base.inl"

#include "release_definitions.h"

namespace NEO {
constexpr auto release = ReleaseType::release3003;

template <>
bool CompilerReleaseHelperHw<release>::isBindlessAddressingDisabled() const {
    return false;
}

} // namespace NEO

template class NEO::CompilerReleaseHelperHw<NEO::release>;
