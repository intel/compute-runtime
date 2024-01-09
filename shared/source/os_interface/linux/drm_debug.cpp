/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_debug.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <cinttypes>
#include <string>
#include <utility>

namespace NEO {

bool Drm::registerResourceClasses() { return ioctlHelper->registerResourceClasses(); }
uint32_t Drm::registerResource(DrmResourceClass classType, const void *data, size_t size) { return ioctlHelper->registerResource(classType, data, size); }
uint32_t Drm::registerIsaCookie(uint32_t isaHandle) { return ioctlHelper->registerIsaCookie(isaHandle); }
void Drm::unregisterResource(uint32_t handle) { return ioctlHelper->unregisterResource(handle); }
bool Drm::resourceRegistrationEnabled() { return ioctlHelper->resourceRegistrationEnabled(); }

uint32_t Drm::notifyFirstCommandQueueCreated(const void *data, size_t size) { return ioctlHelper->notifyFirstCommandQueueCreated(data, size); }
void Drm::notifyLastCommandQueueDestroyed(uint32_t handle) { return ioctlHelper->notifyLastCommandQueueDestroyed(handle); }

void Drm::checkContextDebugSupport() {
    contextDebugSupported = ioctlHelper->isContextDebugSupported();
}

void Drm::setContextDebugFlag(uint32_t drmContextId) {
    [[maybe_unused]] const auto retVal = ioctlHelper->setContextDebugFlag(drmContextId);
    DEBUG_BREAK_IF(retVal != 0 && contextDebugSupported);
}

} // namespace NEO
