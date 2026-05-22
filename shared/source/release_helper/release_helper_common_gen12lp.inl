/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
const SupportedNumGrfs ReleaseHelperHw<release>::getSupportedNumGrfs() const {
    return {128u};
}

template <>
bool ReleaseHelperHw<release>::isPipeControlPriorToNonPipelinedStateCommandsWARequired() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isAdjustWalkOrderAvailable() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isResolvingSubDeviceIDNeeded() const {
    return true;
}
template <>
bool ReleaseHelperHw<release>::isAvailableSemaphore64() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isRayTracingSupported() const {
    return false;
}

} // namespace NEO
