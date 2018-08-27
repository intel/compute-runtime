/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/context/context.h"
#include "runtime/helpers/string.h"
#include "runtime/sharings/sharing_factory.h"
#include "runtime/sharings/gl/gl_sharing.h"
#include "runtime/sharings/gl/gl_arb_sync_event.h"

namespace OCLRT {
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
} // namespace OCLRT
