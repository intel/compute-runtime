/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/release_helper/release_helper.h"

namespace NEO {

template <>
const SupportedNumGrfs ReleaseHelperHw<release>::getSupportedNumGrfs() const {
    return {128u};
}

template <>
bool ReleaseHelperHw<release>::isResolvingSubDeviceIDNeeded() const {
    return true;
}
} // namespace NEO
