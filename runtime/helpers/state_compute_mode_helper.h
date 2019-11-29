/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_cmds.h"
#include "runtime/command_stream/csr_definitions.h"

namespace NEO {
template <typename GfxFamily>
struct StateComputeModeHelper {
    static bool isStateComputeModeRequired(CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed);
};
} // namespace NEO
