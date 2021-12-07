/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper_xehp_and_later.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"

namespace NEO {
struct XE_HPG_COREFamily;
using Family = NEO::XE_HPG_COREFamily;
constexpr static auto deviceValue = AubMemDump::DeviceValues::Dg2;

template class AubHelperHw<Family>;
} // namespace NEO

#include "shared/source/aub_mem_dump/aub_mem_dump_xehp_and_later.inl"
