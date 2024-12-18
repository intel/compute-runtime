/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

namespace NEO {
struct Xe2HpgCoreFamily;
using Family = Xe2HpgCoreFamily;
} // namespace NEO

#include "shared/source/helpers/preamble_xe2_hpg.inl"
#include "shared/source/helpers/preamble_xehp_and_later.inl"

namespace NEO {

template struct PreambleHelper<Family>;

} // namespace NEO
