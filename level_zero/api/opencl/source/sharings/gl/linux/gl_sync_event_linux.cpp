/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/sharings/gl/gl_sync_event.h"

namespace NEO {
namespace LEO {

GlSyncEvent::GlSyncEvent(Context &context, const GL_CL_SYNC_INFO &sync)
    : Event(&context),
      glSync(std::make_unique<GL_CL_SYNC_INFO>(sync)) {
}

GlSyncEvent::~GlSyncEvent() {}

GlSyncEvent *GlSyncEvent::create(Context &context, cl_GLsync sync, cl_int *errCode) {
    return nullptr;
}

void GlSyncEvent::signalGlEvent(void *userData) {
}

} // namespace LEO
} // namespace NEO
