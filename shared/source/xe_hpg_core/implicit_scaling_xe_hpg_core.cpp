/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {

using Family = XeHpgCoreFamily;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = true;

template struct ImplicitScalingDispatch<Family>;
} // namespace NEO
