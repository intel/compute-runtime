/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen_common/aub_mapper_base.h"

namespace OCLRT {
struct CNLFamily;

template <>
struct AUBFamilyMapper<CNLFamily> {
    enum { device = AubMemDump::DeviceValues::Cnl };

    typedef AubMemDump::Traits<device, GfxAddressBits::value> AubTraits;

    static const AubMemDump::LrcaHelper *csTraits[EngineType::NUM_ENGINES];

    static const MMIOList globalMMIO;
    static const MMIOList *perEngineMMIO[EngineType::NUM_ENGINES];

    typedef AubMemDump::AubDump<AubTraits> AUB;
};
} // namespace OCLRT
