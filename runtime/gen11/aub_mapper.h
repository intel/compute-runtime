/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen_common/aub_mapper_base.h"
#include "runtime/memory_manager/memory_constants.h"

#include "engine_node.h"

namespace NEO {
struct ICLFamily;

template <>
struct AUBFamilyMapper<ICLFamily> {
    enum { device = AubMemDump::DeviceValues::Icllp };

    using AubTraits = AubMemDump::Traits<device, MemoryConstants::GfxAddressBits>;

    static const AubMemDump::LrcaHelper *const csTraits[aub_stream::NUM_ENGINES];

    static const MMIOList globalMMIO;
    static const MMIOList *perEngineMMIO[aub_stream::NUM_ENGINES];

    typedef AubMemDump::AubDump<AubTraits> AUB;
};
} // namespace NEO
