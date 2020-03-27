/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_engine_mapper.h"

#include "drm_neo.h"

namespace NEO {

std::unique_ptr<uint8_t[]> Drm::query(uint32_t queryId) {
    return nullptr;
}

bool Drm::queryEngineInfo() {
    return true;
}

bool Drm::queryMemoryInfo() {
    return true;
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType) {
    return DrmEngineMapper::engineNodeMap(engineType);
}

} // namespace NEO
