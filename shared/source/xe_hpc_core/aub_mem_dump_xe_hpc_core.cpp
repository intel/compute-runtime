/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper_xehp_and_later.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {
struct XeHpcCoreFamily;
using Family = NEO::XeHpcCoreFamily;

template class AubHelperHw<Family>;
} // namespace NEO

#include "shared/source/aub_mem_dump/aub_mem_dump_pvc_and_later.inl"
