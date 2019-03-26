/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/gl/gl_sharing.h"

#include "runtime/context/context.h"
#include "runtime/helpers/string.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/sharings/gl/gl_arb_sync_event.h"
#include "runtime/sharings/sharing_factory.h"

namespace NEO {
const uint32_t GLSharingFunctions::sharingId = SharingType::CLGL_SHARING;

int GlSharing::synchronizeHandler(UpdateData &updateData) {
    GLContextGuard guard(*sharingFunctions);
    synchronizeObject(updateData);
    return CL_SUCCESS;
}

template <>
GLSharingFunctions *Context::getSharing() {
    if (GLSharingFunctions::sharingId >= sharingFunctions.size()) {
        DEBUG_BREAK_IF(GLSharingFunctions::sharingId >= sharingFunctions.size());
        return nullptr;
    }

    return reinterpret_cast<GLSharingFunctions *>(sharingFunctions[GLSharingFunctions::sharingId].get());
}

GlArbSyncEvent *GLSharingFunctions::getGlArbSyncEvent(Event &baseEvent) {
    std::lock_guard<std::mutex> lock{glArbEventMutex};
    auto it = glArbEventMapping.find(&baseEvent);
    if (it != glArbEventMapping.end()) {
        return it->second;
    }
    return nullptr;
}

void GLSharingFunctions::removeGlArbSyncEventMapping(Event &baseEvent) {
    std::lock_guard<std::mutex> lock{glArbEventMutex};
    auto it = glArbEventMapping.find(&baseEvent);
    if (it == glArbEventMapping.end()) {
        DEBUG_BREAK_IF(it == glArbEventMapping.end());
        return;
    }
    glArbEventMapping.erase(it);
}

char *createArbSyncEventName() {
    static std::atomic<uint32_t> synchCounter{0};
    uint32_t id = synchCounter++;
    constexpr int maxDigitsForId = std::numeric_limits<uint32_t>::digits10;
    static const char prefix[] = "NEO_SYNC_";
    constexpr int nameMaxLen = sizeof(prefix) + maxDigitsForId + 1;
    char *ret = new char[nameMaxLen];

    snprintf(ret, nameMaxLen, "%s_%d", prefix, id);

    return ret;
}

void destroyArbSyncEventName(char *name) { delete[] name; }
} // namespace NEO
