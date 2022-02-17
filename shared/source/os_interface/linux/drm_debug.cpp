/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_debug.h"

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include <string>
#include <utility>

namespace NEO {

bool Drm::registerResourceClasses() {
    for (auto classNameUUID : classNamesToUuid) {
        auto className = classNameUUID.first;
        auto uuid = classNameUUID.second;

        const auto result = ioctlHelper->registerStringClassUuid(this, uuid, (uintptr_t)className, strnlen_s(className, 100));
        if (result.retVal != 0) {
            return false;
        }

        classHandles.push_back(result.handle);
    }
    return true;
}

uint32_t Drm::registerResource(ResourceClass classType, const void *data, size_t size) {
    if (classHandles.size() < static_cast<uint32_t>(classType)) {
        return 0;
    }

    std::string uuid;
    if (classType == NEO::Drm::ResourceClass::Elf) {
        uuid = generateElfUUID(data);
    } else {
        uuid = generateUUID();
    }

    const auto uuidClass = classHandles[static_cast<uint32_t>(classType)];
    const auto ptr = size > 0 ? (uintptr_t)data : 0;
    const auto result = ioctlHelper->registerUuid(this, uuid, uuidClass, ptr, size);

    PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_UUID_REGISTER: classType = %d, uuid = %s, data = %p, handle = %lu, ret = %d\n", (int)classType, std::string(uuid, 36).c_str(), ptr, result.handle, result.retVal);
    DEBUG_BREAK_IF(result.retVal != 0);

    return result.handle;
}

uint32_t Drm::registerIsaCookie(uint32_t isaHandle) {
    auto uuid = generateUUID();

    const auto result = ioctlHelper->registerUuid(this, uuid, isaHandle, 0, 0);

    PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_UUID_REGISTER: isa handle = %lu, uuid = %s, data = %p, handle = %lu, ret = %d\n", isaHandle, std::string(uuid, 36).c_str(), 0, result.handle, result.retVal);
    DEBUG_BREAK_IF(result.retVal != 0);

    return result.handle;
}

void Drm::unregisterResource(uint32_t handle) {
    PRINT_DEBUGGER_INFO_LOG("PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER: handle = %lu\n", handle);
    [[maybe_unused]] const auto ret = ioctlHelper->unregisterUuid(this, handle);
    DEBUG_BREAK_IF(ret != 0);
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
    std::string elf_class_uuid = classNamesToUuid[static_cast<uint32_t>(Drm::ResourceClass::Elf)].second;
    std::string UUID1st = elf_class_uuid.substr(0, 18);

    const char uuidString[] = "%s-%04" SCNx64 "-%012" SCNx64;
    char buffer[36 + 1] = "00000000-0000-0000-0000-000000000000";

    uint64_t parts[2] = {0, 0};
    parts[0] = reinterpret_cast<uintptr_t>(data) & 0xFFFFFFFFFFFF;
    parts[1] = (reinterpret_cast<uintptr_t>(data) & 0xFFFF000000000000) >> 48;
    snprintf(buffer, sizeof(buffer), uuidString, UUID1st.c_str(), parts[1], parts[0]);

    return std::string(buffer, 36);
}

void Drm::checkContextDebugSupport() {
    contextDebugSupported = ioctlHelper->isContextDebugSupported(this);
}

void Drm::setContextDebugFlag(uint32_t drmContextId) {
    [[maybe_unused]] const auto retVal = ioctlHelper->setContextDebugFlag(this, drmContextId);
    DEBUG_BREAK_IF(retVal != 0 && contextDebugSupported);
}

uint32_t Drm::notifyFirstCommandQueueCreated() {
    const auto result = ioctlHelper->registerStringClassUuid(this, uuidL0CommandQueueHash, (uintptr_t)uuidL0CommandQueueName, strnlen_s(uuidL0CommandQueueName, 100));
    DEBUG_BREAK_IF(result.retVal);
    return result.handle;
}

void Drm::notifyLastCommandQueueDestroyed(uint32_t handle) {
    unregisterResource(handle);
}

} // namespace NEO
