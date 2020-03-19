/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_compute_mode_helper.h"

namespace NEO {
template <>
bool StateComputeModeHelper<TGLLPFamily>::isStateComputeModeRequired(const CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed) { return false; }
} // namespace NEO