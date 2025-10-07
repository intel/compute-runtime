/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_mapper_base.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

#include "aubstream/engine_node.h"

namespace NEO {
struct Xe3CoreFamily;

template <>
struct AUBFamilyMapper<Xe3CoreFamily> {

    static const AubMemDump::LrcaHelper *const csTraits[aub_stream::NUM_ENGINES];
};
} // namespace NEO
