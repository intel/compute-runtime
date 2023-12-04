/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/gl/gl_arb_sync_event.h"

template <bool failCreation>
struct DummyArbEvent : NEO::GlArbSyncEvent {
    DummyArbEvent(NEO::Context &ctx)
        : GlArbSyncEvent(ctx) {
    }

    using GlArbSyncEvent::baseEvent;
    using GlArbSyncEvent::glSyncInfo;
    using GlArbSyncEvent::osInterface;
    bool useBaseSetEvent = false;

    bool setBaseEvent(Event &ev) override {
        return GlArbSyncEvent::setBaseEvent(ev);
    }

    ~DummyArbEvent() override {
        GlArbSyncEvent::glSyncInfo.reset();
    }

    static GlArbSyncEvent *create(Event &baseEv) {
        if (failCreation) {
            return nullptr;
        }
        auto syncEv = new DummyArbEvent<failCreation>(*baseEv.getContext());
        syncEv->baseEvent = &baseEv;
        return syncEv;
    }
};

inline void glArbSyncObjectCleanupMockDoNothing(NEO::OSInterface &osInterface, CL_GL_SYNC_INFO *glSyncInfo) {
}

inline void glArbSyncObjectSignalMockDoNothing(NEO::OsContext &osContext, CL_GL_SYNC_INFO &glSyncInfo) {
}

template <bool fail>
inline bool mockGlArbSyncObjectSetup(NEO::GLSharingFunctions &sharing, NEO::OSInterface &osInterface, CL_GL_SYNC_INFO &glSyncInfo) {
    return (fail == false);
}
