/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/linux/engine_info.h"

namespace NEO {

aub_stream::EngineType EngineInfo::getBaseCopyEngineType(uint64_t capabilities) {
    return aub_stream::EngineType::ENGINE_BCS;
}

void EngineInfo::assignCopyEngine(aub_stream::EngineType baseEngineType, uint32_t tileId, const EngineClassInstance &engine,
                                  BcsInfoMask &bcsInfoMask, uint32_t &numHostLinkCopyEngines, uint32_t &numScaleUpLinkCopyEngines) {
    // Main copy engine:
    UNRECOVERABLE_IF(baseEngineType != aub_stream::ENGINE_BCS);
    UNRECOVERABLE_IF(bcsInfoMask.test(0));
    tileToEngineToInstanceMap[tileId][aub_stream::ENGINE_BCS] = engine;
    bcsInfoMask.set(0, true);
}
} // namespace NEO
