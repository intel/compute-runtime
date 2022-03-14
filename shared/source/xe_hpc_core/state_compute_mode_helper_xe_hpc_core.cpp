/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_compute_mode_helper.h"

namespace NEO {

using Family = XE_HPC_COREFamily;

template <>
bool StateComputeModeHelper<Family>::isStateComputeModeRequired(const CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed) {
    return csrSizeRequestFlags.numGrfRequiredChanged;
}

template struct StateComputeModeHelper<Family>;

} // namespace NEO
