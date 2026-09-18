/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/source/release_helpers/release_helper/release_helper_common_xe3p_and_later.inl"

namespace NEO {
template <>
bool ReleaseHelperHw<release>::isResolvingSubDeviceIDNeeded() const {
    return false;
}

} // namespace NEO
