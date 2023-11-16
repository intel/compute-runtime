/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isProgramAllStateComputeCommandFieldsWARequired() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isSplitMatrixMultiplyAccumulateSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isBFloat16ConversionSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isDirectSubmissionSupported() const {
    return true;
}

template <>
bool ReleaseHelperHw<release>::isRcsExposureDisabled() const {
    return true;
}
} // namespace NEO
