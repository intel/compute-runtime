/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_mapper_base.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

#include "aubstream/engine_node.h"

namespace NEO {
struct Xe2HpgCoreFamily;

template <>
struct AUBFamilyMapper<Xe2HpgCoreFamily> {
    using AubTraits = AubMemDump::Traits<MemoryConstants::gfxAddressBits>;

    static const AubMemDump::LrcaHelper *const csTraits[aub_stream::NUM_ENGINES];

    static const MMIOList globalMMIO;
    static const MMIOList *perEngineMMIO[aub_stream::NUM_ENGINES];

    using AUB = AubMemDump::AubDump<AubTraits>;
};
} // namespace NEO
