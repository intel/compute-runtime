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
bool ReleaseHelperHw<release>::isResolvingSubDeviceIDNeeded() const {
    return true;
}
template <>
bool ReleaseHelperHw<release>::isAvailableSemaphore64() const {
    if (debugManager.flags.Enable64BitSemaphore.get() != -1) {
        return debugManager.flags.Enable64BitSemaphore.get() == 1;
    }
    return false;
}

template <>
bool ReleaseHelperHw<release>::isRayTracingSupported() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isMatrixMultiplyAccumulateSupported() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isDotProductAccumulateSystolicSupported() const {
    return false;
}

} // namespace NEO
