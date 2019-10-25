/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/csr_definitions.h"

#include "hw_cmds.h"

namespace NEO {
template <typename GfxFamily>
struct StateComputeModeHelper {
    static bool isStateComputeModeRequired(CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed);
};
} // namespace NEO
