/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_hw_info_config.h"

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
void CompilerProductHelperHw<gfxProduct>::adjustHwInfoForIgc(HardwareInfo &hwInfo) const {}

} // namespace NEO
