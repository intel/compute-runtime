/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
const ThreadsPerEUConfigs ReleaseHelperHw<release>::getThreadsPerEUConfigs(uint32_t numThreadsPerEu) const {
    if (numThreadsPerEu == 8u) {
        return {4, 8};
    }
    return {4, 5, 6, 8, 10};
}

template <>
bool ReleaseHelperHw<release>::isRcsExposureDisabled() const {
    return true;
}

} // namespace NEO
