/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_translate_from_kmd.inl"

namespace NEO {

bool Wddm::isNativeFenceAvailable() {
    return false;
}

void Wddm::setAdditionalEngines() {
}

void Wddm::translateFromKmd(PLATFORM &platform) {
    mapRenderCoreFamily(platform, renderCoreFamilyMap);
    applyPlatformDebugOverrides(platform);
}

} // namespace NEO
