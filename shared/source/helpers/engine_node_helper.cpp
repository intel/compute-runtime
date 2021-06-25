/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"

namespace NEO::EngineHelpers {

std::string engineUsageToString(EngineUsage usage) {
    switch (usage) {
    case EngineUsage::Regular:
        return "Regular";
    case EngineUsage::LowPriority:
        return "LowPriority";
    case EngineUsage::Internal:
        return "Internal";
    default:
        return "Unknown";
    }
}

std::string engineTypeToString(aub_stream::EngineType engineType) {
    switch (engineType) {
    case aub_stream::EngineType::ENGINE_RCS:
        return "RCS";
    case aub_stream::EngineType::ENGINE_BCS:
        return "BCS";
    case aub_stream::EngineType::ENGINE_VCS:
        return "VCS";
    case aub_stream::EngineType::ENGINE_VECS:
        return "VECS";
    case aub_stream::EngineType::ENGINE_CCS:
        return "CCS";
    case aub_stream::EngineType::ENGINE_CCS1:
        return "CCS1";
    case aub_stream::EngineType::ENGINE_CCS2:
        return "CCS2";
    case aub_stream::EngineType::ENGINE_CCS3:
        return "CCS3";
    default:
        return engineTypeToStringAdditional(engineType);
    }
}

bool isCcs(aub_stream::EngineType engineType) {
    return engineType >= aub_stream::ENGINE_CCS && engineType <= aub_stream::ENGINE_CCS3;
}

} // namespace NEO::EngineHelpers
