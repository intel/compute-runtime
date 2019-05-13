/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm_neo.h"

namespace NEO {

void *Drm::query(uint32_t queryId) {
    return nullptr;
}

void Drm::queryEngineInfo() {
}

void Drm::queryMemoryInfo() {
}

int Drm::bindDrmContext(uint32_t drmContextId, DeviceBitfield deviceBitfield, aub_stream::EngineType engineType) {
    return -1;
}

} // namespace NEO
