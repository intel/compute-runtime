/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper_xehp_and_later.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {
struct Xe3CoreFamily;
using Family = NEO::Xe3CoreFamily;

template class AubHelperHw<Family>;
} // namespace NEO

#include "shared/source/aub_mem_dump/aub_mem_dump_pvc_and_later.inl"
