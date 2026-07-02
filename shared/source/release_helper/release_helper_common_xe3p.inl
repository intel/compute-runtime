/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {
template <>
bool ReleaseHelperHw<release>::isResolvingSubDeviceIDNeeded() const {
    return false;
}

} // namespace NEO
