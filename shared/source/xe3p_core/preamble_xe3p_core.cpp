/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"

namespace NEO {
struct Xe3pCoreFamily;
using Family = Xe3pCoreFamily;
} // namespace NEO

#include "shared/source/helpers/preamble_xe3_and_later.inl"
#include "shared/source/helpers/preamble_xehp_and_later.inl"

namespace NEO {

template <>
void PreambleHelper<Family>::setSingleSliceDispatchMode(void *cmd, bool enable) {
}

template struct PreambleHelper<Family>;

} // namespace NEO
