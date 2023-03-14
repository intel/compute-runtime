/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_product_helper.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isForceEmuInt32DivRemSPRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isStatelessToStatefulBufferOffsetSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
const char *CompilerProductHelperHw<gfxProduct>::getCachingPolicyOptions(bool isDebuggerActive) const {
    return L1CachePolicyHelper<gfxProduct>::getCachingPolicyOptions(isDebuggerActive);
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::failBuildProgramWithStatefulAccessPreference() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::string CompilerProductHelperHw<gfxProduct>::getExtensions(const HardwareInfo &hwInfo) const {
    std::string extensions = "";

    if (isCreateBufferWithPropertiesSupported()) {
        extensions += "cl_intel_create_buffer_with_properties ";
    }

    if (isDotAccumulateSupported()) {
        extensions += "cl_intel_dot_accumulate ";
    }

    if (isSubgroupLocalBlockIoSupported()) {
        extensions += "cl_intel_subgroup_local_block_io ";
    }

    if (isMatrixMultiplyAccumulateSupported(hwInfo)) {
        extensions += "cl_intel_subgroup_matrix_multiply_accumulate ";
    }

    if (isSplitMatrixMultiplyAccumulateSupported(hwInfo)) {
        extensions += "cl_intel_subgroup_split_matrix_multiply_accumulate ";
    }

    if (isSubgroupNamedBarrierSupported()) {
        extensions += "cl_khr_subgroup_named_barrier ";
    }

    if (isSubgroupExtendedBlockReadSupported()) {
        extensions += "cl_intel_subgroup_extended_block_read ";
    }
    return extensions;
}

} // namespace NEO
