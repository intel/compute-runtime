/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <>
bool ReleaseHelperHw<release>::isMatrixMultiplyAccumulateSupported() const {
    return false;
}

template <>
std::optional<GfxMemoryAllocationMethod> ReleaseHelperHw<release>::getPreferredAllocationMethod(AllocationType allocationType) const {
    switch (allocationType) {
    case AllocationType::TAG_BUFFER:
    case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        return {};
    default:
        return GfxMemoryAllocationMethod::AllocateByKmd;
    }
}

template <>
bool ReleaseHelperHw<release>::isCachingOnCpuAvailable() const {
    return false;
}

template <>
bool ReleaseHelperHw<release>::isDirectSubmissionSupported() const {
    return true;
}

} // namespace NEO
