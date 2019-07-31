/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_engine_mapper.h"

#include "drm_neo.h"

namespace NEO {

void *Drm::query(uint32_t queryId) {
    return nullptr;
}

void Drm::queryEngineInfo() {
}

void Drm::queryMemoryInfo() {
}

void Drm::setMemoryRegions() {
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType) {
    return DrmEngineMapper::engineNodeMap(engineType);
}

} // namespace NEO
