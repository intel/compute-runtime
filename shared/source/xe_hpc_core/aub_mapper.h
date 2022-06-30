/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_mapper_base.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"

#include "engine_node.h"

namespace NEO {
struct XE_HPC_COREFamily;

template <>
struct AUBFamilyMapper<XE_HPC_COREFamily> {
    enum { device = AubMemDump::DeviceValues::Pvc };

    using AubTraits = AubMemDump::Traits<device, MemoryConstants::GfxAddressBits>;

    static const AubMemDump::LrcaHelper *const csTraits[aub_stream::NUM_ENGINES];

    static const MMIOList globalMMIO;
    static const MMIOList *perEngineMMIO[aub_stream::NUM_ENGINES];

    using AUB = AubMemDump::AubDump<AubTraits>;
};
} // namespace NEO
