/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm_neo.h"

namespace NEO {
bool Drm::registerResourceClasses() {
    return false;
}

uint32_t Drm::registerResource(ResourceClass classType, void *data, size_t size) {
    return 0;
}

void Drm::unregisterResource(uint32_t handle) {
}
} // namespace NEO