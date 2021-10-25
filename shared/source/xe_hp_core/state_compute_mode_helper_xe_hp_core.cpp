/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_compute_mode_helper.h"

namespace NEO {

template <>
bool StateComputeModeHelper<XeHpFamily>::isStateComputeModeRequired(const CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed) {
    return csrSizeRequestFlags.coherencyRequestChanged || csrSizeRequestFlags.numGrfRequiredChanged;
}

} // namespace NEO
