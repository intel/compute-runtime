/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "drm_neo.h"

namespace NEO {

bool Drm::registerResourceClasses() {
    return false;
}

uint32_t Drm::registerResource(ResourceClass classType, const void *data, size_t size) {
    return 0;
}

uint32_t Drm::registerIsaCookie(uint32_t isaHandle) {
    return 0;
}

void Drm::unregisterResource(uint32_t handle) {
}

std::string Drm::generateUUID() {
    const char uuidString[] = "00000000-0000-0000-%04" SCNx64 "-%012" SCNx64;
    char buffer[36 + 1] = "00000000-0000-0000-0000-000000000000";
    uuid++;

    UNRECOVERABLE_IF(uuid == 0xFFFFFFFFFFFFFFFF);

    uint64_t parts[2] = {0, 0};
    parts[0] = uuid & 0xFFFFFFFFFFFF;
    parts[1] = (uuid & 0xFFFF000000000000) >> 48;
    snprintf(buffer, sizeof(buffer), uuidString, parts[1], parts[0]);

    return std::string(buffer, 36);
}

std::string Drm::generateElfUUID(const void *data) {
    return generateUUID();
}

void Drm::checkContextDebugSupport() {}
void Drm::setContextDebugFlag(uint32_t drmContextId) {}

} // namespace NEO
