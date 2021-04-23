/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper_xehp_plus.inl"

#include "aub_mem_dump.h"

namespace NEO {
struct XeHpFamily;
using Family = NEO::XeHpFamily;
constexpr static auto deviceValue = AubMemDump::DeviceValues::XeHP_SDV;

template class AubHelperHw<Family>;
} // namespace NEO

#include "shared/source/aub_mem_dump/aub_mem_dump_xehp_plus.inl"
