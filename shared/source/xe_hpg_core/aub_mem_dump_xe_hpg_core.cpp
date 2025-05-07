/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper_xehp_and_later.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {
struct XeHpgCoreFamily;
using Family = NEO::XeHpgCoreFamily;

template class AubHelperHw<Family>;
} // namespace NEO

#include "shared/source/aub_mem_dump/aub_mem_dump_xehp_and_later.inl"
