/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper_xehp_and_later.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

namespace NEO {
struct Xe2HpgCoreFamily;
using Family = NEO::Xe2HpgCoreFamily;
constexpr static auto deviceValue = AubMemDump::DeviceValues::Bmg;

template class AubHelperHw<Family>;
} // namespace NEO

#include "shared/source/aub_mem_dump/aub_mem_dump_pvc_and_later.inl"
