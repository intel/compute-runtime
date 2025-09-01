/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isStagingBuffersEnabled() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isCompressionForbidden(const HardwareInfo &hwInfo) const {
    return isCompressionForbiddenCommon(false);
}

template <>
void ProductHelperHw<gfxProduct>::overrideDirectSubmissionTimeouts(uint64_t &timeoutUs, uint64_t &maxTimeoutUs) const {
    timeoutUs = 1'000;
    maxTimeoutUs = 1'000;
    if (debugManager.flags.DirectSubmissionControllerTimeout.get() != -1) {
        timeoutUs = debugManager.flags.DirectSubmissionControllerTimeout.get();
    }
    if (debugManager.flags.DirectSubmissionControllerMaxTimeout.get() != -1) {
        maxTimeoutUs = debugManager.flags.DirectSubmissionControllerMaxTimeout.get();
    }
}

} // namespace NEO
