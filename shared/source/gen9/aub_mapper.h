/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_mapper_base.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

namespace NEO {
struct SKLFamily;

template <>
struct AUBFamilyMapper<SKLFamily> {
    enum { device = AubMemDump::DeviceValues::Skl };

    using AubTraits = AubMemDump::Traits<device, MemoryConstants::GfxAddressBits>;

    static const AubMemDump::LrcaHelper *const csTraits[aub_stream::NUM_ENGINES];

    static const MMIOList globalMMIO;
    static const MMIOList *perEngineMMIO[aub_stream::NUM_ENGINES];

    typedef AubMemDump::AubDump<AubTraits> AUB;
};
} // namespace NEO
