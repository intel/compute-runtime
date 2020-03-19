/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/helpers/hw_cmds.h"

namespace NEO {
template <typename GfxFamily>
struct StateComputeModeHelper {
    static bool isStateComputeModeRequired(const CsrSizeRequestFlags &csrSizeRequestFlags, bool isThreadArbitionPolicyProgrammed);
};
} // namespace NEO
