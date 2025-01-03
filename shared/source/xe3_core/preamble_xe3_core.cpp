/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {
struct Xe3CoreFamily;
using Family = Xe3CoreFamily;
} // namespace NEO

#include "shared/source/helpers/preamble_xe3_and_later.inl"
#include "shared/source/helpers/preamble_xehp_and_later.inl"

namespace NEO {

template struct PreambleHelper<Family>;

} // namespace NEO
