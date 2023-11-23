/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_mapper_base.h"
#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {
struct Gen12LpFamily;

template <>
struct AUBFamilyMapper<Gen12LpFamily> {
    enum { device = AubMemDump::DeviceValues::Tgllp };

    using AubTraits = AubMemDump::Traits<device, MemoryConstants::gfxAddressBits>;

    static const AubMemDump::LrcaHelper *const csTraits[aub_stream::NUM_ENGINES];

    static const MMIOList globalMMIO;
    static const MMIOList *perEngineMMIO[aub_stream::NUM_ENGINES];

    typedef AubMemDump::AubDump<AubTraits> AUB;
};
} // namespace NEO
