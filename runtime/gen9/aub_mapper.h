/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen_common/aub_mapper_base.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {
struct SKLFamily;

template <>
struct AUBFamilyMapper<SKLFamily> {
    enum { device = AubMemDump::DeviceValues::Skl };

    using AubTraits = AubMemDump::Traits<device, MemoryConstants::GfxAddressBits>;

    static const AubMemDump::LrcaHelper *const csTraits[EngineType::NUM_ENGINES];

    static const MMIOList globalMMIO;
    static const MMIOList *perEngineMMIO[EngineType::NUM_ENGINES];

    typedef AubMemDump::AubDump<AubTraits> AUB;
};
} // namespace OCLRT
