/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_stream/csr_definitions.h"
#include "core/helpers/hw_cmds.h"

namespace NEO {
template <typename GfxFamily>
struct StateComputeModeHelper {
    static bool isStateComputeModeRequired(CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed);
};
} // namespace NEO
