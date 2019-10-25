/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/state_compute_mode_helper.h"

namespace NEO {
template <>
bool StateComputeModeHelper<TGLLPFamily>::isStateComputeModeRequired(CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed) { return false; }
} // namespace NEO